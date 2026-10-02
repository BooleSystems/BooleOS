// booleos/kernel/process.c
#include "process.h"
#include "hal.h"
#include "messages.h"
#include "memory/vmm.h"
#include "memory/pmm.h"
#include "tss.h"
#include "irq.h"
#include <stdint.h>

static process_t process_table[PROCESS_MAX];
static uint8_t process_stacks[PROCESS_MAX][PROCESS_STACK_SIZE] __attribute__((aligned(16)));
static process_t *current_process = 0;
static uint32_t next_pid = 1;

/* Atomic "return the next pid and increment". The increment is a
   read-modify-write, so a timer tick landing in the middle of it could
   hand the same pid to two processes. */
static uint32_t alloc_pid(void) {
    uint32_t flags = irq_save();
    uint32_t pid = next_pid++;
    irq_restore(flags);
    return pid;
}

static uint32_t *stack_push(uint32_t *stack, uint32_t value) {
    stack--;
    *stack = value;
    return stack;
}

static uint32_t build_initial_stack(void *stack_mem, uint32_t stack_size, void (*bootstrap)(void)) {
    uint32_t *stack = (uint32_t *)((uint8_t *)stack_mem + stack_size);

    stack = stack_push(stack, (uint32_t)bootstrap);
    stack = stack_push(stack, 0);
    stack = stack_push(stack, 0);
    stack = stack_push(stack, 0);
    stack = stack_push(stack, 0);

    return (uint32_t)stack;
}

static void copy_name(char *dst, const char *src) {
    uint32_t i = 0;

    if (!src || !src[0])
        src = "kernel-task";

    while (i < PROCESS_NAME_MAX - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void process_init(void) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_table[i].pid = 0;
        process_table[i].name[0] = '\0';
        process_table[i].state = PROCESS_UNUSED;
        process_table[i].entry = 0;
        process_table[i].arg = 0;
        process_table[i].esp = 0;
        process_table[i].stack = 0;
        process_table[i].stack_size = 0;
        process_table[i].wake_tick = 0;
        process_table[i].ticks_run = 0;
        process_table[i].runs = 0;
        process_table[i].cwd_cluster = 0;
        process_table[i].stdin_redirect = -1;
        process_table[i].stdout_redirect = -1;
        process_table[i].waiting_for_pid = 0;
        process_table[i].cr3 = 0;
    }

    current_process = 0;
    next_pid = 1;
}

process_t *process_spawn_user(const char *name, uint32_t user_entry,
                              uint32_t user_esp, uint32_t cr3,
                              uint32_t cwd_cluster, int start_blocked,
                              void (*bootstrap)(void)) {
    if (!bootstrap) return 0;

    /* ── 1. atomically reserve a free slot ────────────────────────
       Same discipline as process_fork(): the scan and the claim happen
       inside one interrupt-off section, so two spawns interleaved by
       the preemptive timer can't both pick the same PROCESS_UNUSED
       slot, and the slot leaves PROCESS_UNUSED already marked
       PROCESS_BLOCKED ("reserved, not runnable"). pid and
       waiting_for_pid are also set here, while interrupts are still
       off: a reserved-but-unfilled slot still carries the previous
       occupant's stale values, and a stale pid would make sys_wait()'s
       liveness scan think a long-dead pid is alive, while a stale
       waiting_for_pid could let process_exit() wake this half-built
       slot to READY. */
    int slot = -1;
    uint32_t flags = irq_save();
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            slot = (int)i;
            process_table[i].state = PROCESS_BLOCKED;
            process_table[i].pid = alloc_pid();
            process_table[i].waiting_for_pid = 0;
            process_table[i].cr3 = 0;   /* no address space yet: see process_exit() */
            break;
        }
    }
    irq_restore(flags);
    if (slot < 0) return 0;

    /* ── 2. fill in every field while the slot is unschedulable ─── */
    process_t *p = &process_table[slot];
    copy_name(p->name, name);
    p->entry      = 0;                   /* unused: entry is ring 3 */
    p->arg        = (void *)user_entry;  /* user process's EIP */
    p->stack      = process_stacks[slot];
    p->stack_size = PROCESS_STACK_SIZE;
    p->esp        = build_initial_stack(p->stack, p->stack_size, bootstrap);
    p->cr3        = cr3;
    p->user_esp   = user_esp;
    p->user_stack = 0;
    p->wake_tick  = 0;
    p->ticks_run  = 0;
    p->runs       = 0;
    p->cwd_cluster = cwd_cluster;
    p->stdin_redirect  = -1;
    p->stdout_redirect = -1;

    /* ── 3. only now publish the real state ───────────────────────
       PROCESS_BLOCKED here (start_blocked=1) means "reserved but not
       runnable yet" — see the start_blocked parameter doc in
       process.h. Setting it last is what keeps the scheduler from
       ever switching to a slot whose esp/cr3 aren't built yet. */
    if (!start_blocked)
        p->state = PROCESS_READY;
    return p;
}

void process_make_ready(process_t *p) {
    if (p && p->state == PROCESS_BLOCKED)
        p->state = PROCESS_READY;
}

process_t *process_fork(process_t *parent, const uint32_t *saved_frame) {
    if (!parent || !saved_frame)
        return 0;

    /* ── 1. atomically claim a free process slot ──────────────────
       Protected by cli/sti so two fork()s interleaved by the
       preemptive timer can't both pick the same slot —
       process_spawn_user() claims its slot the same way. */
    int slot = -1;
    __asm__ volatile ("cli");
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            slot = (int)i;
            process_table[i].pid = alloc_pid();
            /* PROCESS_BLOCKED: reserved but not runnable yet — cr3 and
               the kernel stack below aren't built. Only flipped to
               PROCESS_READY once the child is fully formed, so
               scheduler_run_once() can't pick it up half-built. */
            process_table[i].state = PROCESS_BLOCKED;
            process_table[i].cr3 = 0;   /* no address space yet: see process_exit() */
            break;
        }
    }
    __asm__ volatile ("sti");
    if (slot < 0)
        return 0;   /* table full — nothing allocated yet, nothing to undo */

    process_t *child = &process_table[slot];
    copy_name(child->name, parent->name);
    child->entry      = 0;    /* unused: the child resumes via isr128_resume, not a bootstrap */
    child->arg        = 0;
    child->stack       = process_stacks[slot];
    child->stack_size  = PROCESS_STACK_SIZE;
    child->wake_tick   = 0;
    child->ticks_run   = 0;
    child->runs        = 0;
    child->user_stack  = 0;
    child->user_esp    = parent->user_esp;
    child->cwd_cluster = parent->cwd_cluster;   /* "cd" survives fork() */
    /* fd 0/1 redirects are part of the fd table fork() already
       duplicates in full (sys_fork(), which copies fd_table row by
       row) — a child must see the same fd 0/1 its parent did at
       fork time. */
    child->stdin_redirect  = parent->stdin_redirect;
    child->stdout_redirect = parent->stdout_redirect;
    /* per-syscall-in-progress state, not identity — never inherited */
    child->waiting_for_pid = 0;

    /* ── 2. share the address space copy-on-write ─────────────────
       Walks every present user PTE beyond the shared kernel mapping
       (PDE 0/1), so it covers whatever the parent has mapped — code,
       data, stack — not a fixed list of regions. Each page is mapped
       into the child at the SAME frame, its refcount goes up by one,
       and a writable page becomes read-only + VMM_COW in both
       directories; the first write by either side copies it
       (vmm_cow_break()). A page that was already read-only without
       VMM_COW stays that way in both, and one already VMM_COW (a
       process forking again) just gains another sharer.

       The whole pass runs with interrupts off: a timer tick in the
       middle would let the parent (or a sharer of one of its pages)
       run with some PTEs converted and others not, or with a refcount
       one short of its mappings. Nothing in the pass blocks. */
    uint32_t child_cr3 = vmm_create_directory();
    if (!child_cr3) {
        child->state = PROCESS_UNUSED;
        child->pid   = 0;
        return 0;
    }

    uint32_t *parent_pd = (uint32_t *)parent->cr3;
    uint32_t cur_cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cur_cr3));
    int failed = 0;

    uint32_t flags = irq_save();
    for (uint32_t di = 2; di < 1024 && !failed; di++) {
        if (!(parent_pd[di] & VMM_PRESENT)) continue;
        /* A kernel PDE (MMIO mapped at boot): the child already has it from
           vmm_create_directory(), and its frames are not PMM pages. */
        if (!(parent_pd[di] & VMM_USER)) continue;
        uint32_t *parent_pt = (uint32_t *)(parent_pd[di] & 0xFFFFF000);

        for (uint32_t ti = 0; ti < 1024; ti++) {
            uint32_t pte = parent_pt[ti];
            if (!(pte & VMM_PRESENT)) continue;

            uint32_t virt  = (di << 22) | (ti << 12);
            uint32_t frame = pte & 0xFFFFF000;
            uint32_t share = pte & 0xFFF;
            if (share & VMM_WRITABLE)
                share = (share & ~VMM_WRITABLE) | VMM_COW;

            if (pmm_page_ref(frame) != 0) { failed = 1; break; }
            if (vmm_map_user_page_flags(child_cr3, virt, frame, share) != 0) {
                pmm_free_page(frame);   /* undo the reference: not mapped, the unwind can't see it */
                failed = 1;
                break;
            }

            if (share != (pte & 0xFFF)) {
                parent_pt[ti] = frame | share;
                if (parent->cr3 == cur_cr3)
                    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
            }
        }
    }

    if (failed) {
        /* Drops the child's references and frees its page tables and
           directory. Pages the parent already turned read-only stay
           VMM_COW with a count of 1, so its next write takes them back
           writable in place (vmm_cow_break()). */
        vmm_destroy_directory(child_cr3);
        irq_restore(flags);
        child->state = PROCESS_UNUSED;
        child->pid   = 0;
        return 0;
    }
    irq_restore(flags);

    child->cr3 = child_cr3;

    /* ── 3. fabricate the child's kernel stack ─────────────────────
       Lays out, from the top of the child's stack down: the 13-word
       frame captured from the parent's syscall entry (with eax
       already zeroed by the caller — see sys_fork() in syscall.c),
       then the same 4-dummy-words-plus-return-address prologue
       build_initial_stack() uses for brand-new processes, except the
       "return address" is isr128_resume instead of a bootstrap
       function. The first time the scheduler runs this process,
       context_switch()'s pop/ret lands on isr128_resume, which does
       'popa; iret' using the frame right above it — resuming exactly
       where the parent's fork() syscall was, with eax=0. */
    extern void isr128_resume(void);

    uint32_t *top   = (uint32_t *)((uint8_t *)child->stack + child->stack_size);
    uint32_t *frame = top - 13;
    for (int i = 0; i < 13; i++) frame[i] = saved_frame[i];

    uint32_t *sp = frame;
    sp = stack_push(sp, (uint32_t)isr128_resume);
    sp = stack_push(sp, 0);   /* edi */
    sp = stack_push(sp, 0);   /* esi */
    sp = stack_push(sp, 0);   /* ebx */
    sp = stack_push(sp, 0);   /* ebp */
    child->esp = (uint32_t)sp;

    child->state = PROCESS_READY;
    return child;
}

process_t *process_at(uint32_t index) {
    if (index >= PROCESS_MAX)
        return 0;
    return &process_table[index];
}

process_t *process_current(void) {
    return current_process;
}

void process_set_current(process_t *process) {
    current_process = process;
}

void process_exit(process_t *process) {
    /* Frees the whole address space right here (Phase 23-B): the process's
       reference to each user page (a page still shared copy-on-write with
       another process stays allocated), its page tables and its directory,
       through vmm_destroy_directory(). There is no zombie state and no
       deferred reaper, because nothing needs one:
       - the kernel stack is a static per-slot array (process_stacks[]),
         reused by the slot's next occupant, never allocated or freed;
       - when the process exits itself (sys_exit(), or sys_kill() of its own
         pid) it is running on the directory being freed, and
         vmm_destroy_directory() loads the kernel directory first. That
         directory has the same 0-8MB identity map, so the rest of the exit
         path (kernel code, this kernel stack) keeps running; the only thing
         it loses is the user mapping, which it never touches again. The
         CR3 switch, the frees and PROCESS_UNUSED happen in one
         interrupt-off section, and once the state is PROCESS_UNUSED the
         timer no longer preempts it (timer_callback() only preempts a
         RUNNING process), so it reaches scheduler_yield() and never runs
         again;
       - a process killed by another one (Ctrl+C, `kill`) is not running:
         it is READY, SLEEPING or BLOCKED (on sys_wait(), a pipe, or an ATA
         command), and it is never scheduled again once its slot is
         PROCESS_UNUSED, so nothing will walk its directory. The ATA IRQ
         handler and the gate keep only a process_t pointer and check
         state == PROCESS_BLOCKED before waking it; they never touch the
         address space.
       cr3 == 0 is a slot claimed by fork()/spawn but not built yet (killed
       in that window), which owns nothing. The kernel directory is never
       freed (vmm_destroy_directory() refuses it): the shell and every
       other process run on their own directory, so a process exiting can
       only free its own.
       One interrupt-off section from the state check to PROCESS_UNUSED: a
       tick between clearing cr3 and leaving the READY/RUNNING states would
       let the scheduler switch to this process with CR3 = 0, and two exits
       of the same process must not both free its directory (the second
       sees PROCESS_UNUSED and returns). */
    uint32_t flags = irq_save();
    if (!process || process->state == PROCESS_UNUSED) {
        irq_restore(flags);
        return;
    }
    uint32_t cr3 = process->cr3;
    process->cr3 = 0;   /* no stored copy of the freed directory survives */
    if (cr3 && cr3 != vmm_get_kernel_directory())
        vmm_destroy_directory(cr3);

    uint32_t exited_pid = process->pid;

    process->state = PROCESS_UNUSED;
    process->pid   = 0;
    irq_restore(flags);

    /* Wakes every process specifically waiting (via sys_wait(), Phase
       16) for THIS pid — not a generic "some child exited" signal, so
       a process with several children waiting on one specific pid is
       never woken by an unrelated sibling's exit. No cli/sti guard
       here, matching process_wake_sleepers() right below (also a full
       table scan with no protection) — process_exit() only ever runs
       synchronously inside sys_exit()/sys_kill(), never from IRQ
       context, so there's no concurrent mutator to race against here,
       unlike ata_wait_irq()'s real async-IRQ case. */
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_t *p = &process_table[i];
        if (p->state == PROCESS_BLOCKED && p->waiting_for_pid == exited_pid) {
            p->waiting_for_pid = 0;
            p->state = PROCESS_READY;
        }
    }
}

void process_sleep(process_t *process, uint32_t now, uint32_t ticks) {
    if (!process || process->state == PROCESS_UNUSED)
        return;

    process->wake_tick = now + ticks;
    process->state = PROCESS_SLEEPING;
}

void process_wake_sleepers(uint32_t now) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_t *process = &process_table[i];
        if (process->state == PROCESS_SLEEPING && now >= process->wake_tick)
            process->state = PROCESS_READY;
    }
}

const char *process_state_name(process_state_t state) {
    switch (state) {
        case PROCESS_UNUSED:   return msg(MSG_PROC_STATE_UNUSED);
        case PROCESS_READY:    return msg(MSG_PROC_STATE_READY);
        case PROCESS_RUNNING:  return msg(MSG_PROC_STATE_RUNNING);
        case PROCESS_SLEEPING: return msg(MSG_PROC_STATE_SLEEP);
        case PROCESS_BLOCKED:  return msg(MSG_PROC_STATE_BLOCKED);
        case PROCESS_ZOMBIE:   return msg(MSG_PROC_STATE_ZOMBIE);
        default:               return msg(MSG_PROC_STATE_UNKNOWN);
    }
}

void process_dump(void) {
    console_set_color(CONSOLE_CYAN, CONSOLE_BLACK);
    console_puts(msg(MSG_PROC_TAG));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    console_puts(msg(MSG_PROC_TABLE_HEADER));

    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_t *process = &process_table[i];
        if (process->state == PROCESS_UNUSED)
            continue;

        console_puts("       ");
        console_put_dec(process->pid);
        console_puts("    ");
        console_puts(process_state_name(process->state));
        console_puts("    ");
        console_put_dec(process->runs);
        console_puts("    ");
        console_put_hex(process->esp);
        console_puts("    ");
        console_puts(process->name);
        console_puts("\n");
    }
}
