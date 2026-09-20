// nullos/kernel/process.c
#include "process.h"
#include "drivers/vga.h"
#include "memory/vmm.h"
#include "memory/pmm.h"
#include "tss.h"
#include <stdint.h>

static process_t process_table[PROCESS_MAX];
static uint8_t process_stacks[PROCESS_MAX][PROCESS_STACK_SIZE] __attribute__((aligned(16)));
static process_t *current_process = 0;
static uint32_t next_pid = 1;

/* Atomic "return the next pid and increment". The increment is a
   read-modify-write, so a timer tick landing in the middle of it could
   hand the same pid to two processes.
   Saves EFLAGS and restores it instead of doing a bare cli/sti:
   process_fork() calls this from inside its own cli...sti slot-claim
   section, and an unconditional sti here would re-enable interrupts in
   the middle of that section. */
static uint32_t alloc_pid(void) {
    uint32_t flags;
    __asm__ volatile ("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    uint32_t pid = next_pid++;
    __asm__ volatile ("push %0; popf" : : "r"(flags) : "memory", "cc");
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
    }

    current_process = 0;
    next_pid = 1;
}

/* KNOWN RACE / TECH DEBT (out of scope for Phase 13): process_spawn()
   and process_spawn_user() below scan for a PROCESS_UNUSED slot and
   claim it WITHOUT any cli/sti protection, unlike process_fork()
   further down in this file. If the preemptive timer interrupts this
   scan and a different process also ends up in one of these
   functions (e.g. two exec() calls racing), both could pick the same
   slot before either marks it used, corrupting the process table.
   This hasn't been observed in practice — the scan itself is a
   handful of instructions, far under one timer slice — and fixing it
   was explicitly left out of scope here; only process_fork()'s own
   slot claim was required to be atomic. If this is ever tightened up,
   do both functions together for consistency. */
process_t *process_spawn(const char *name, process_entry_t entry, void *arg, void (*bootstrap)(void)) {
    if (!entry || !bootstrap)
        return 0;

    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_t *process = &process_table[i];
        if (process->state == PROCESS_UNUSED) {
            process->pid = alloc_pid();
            copy_name(process->name, name);

            process->state = PROCESS_READY;
            process->entry = entry;
            process->arg = arg;
            process->stack = process_stacks[i];
            process->stack_size = PROCESS_STACK_SIZE;
            process->esp = build_initial_stack(process->stack, process->stack_size, bootstrap);
            process->wake_tick = 0;
            process->ticks_run = 0;
            process->runs = 0;
            process->cwd_cluster = 0;   /* new processes start at the root */
            process->stdin_redirect = -1;
            process->stdout_redirect = -1;
            process->waiting_for_pid = 0;
            process->cr3 = vmm_create_directory();
            if (!process->cr3)
                process->cr3 = vmm_get_kernel_directory();
            return process;
        }
    }

    return 0;
}

process_t *process_spawn_user(const char *name, uint32_t user_entry,
                              uint32_t user_esp, uint32_t cr3,
                              uint32_t cwd_cluster, int start_blocked,
                              void (*bootstrap)(void)) {
    if (!bootstrap) return 0;

    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_t *p = &process_table[i];
        if (p->state != PROCESS_UNUSED) continue;

        p->pid        = alloc_pid();
        copy_name(p->name, name);
        /* PROCESS_BLOCKED here means "reserved but not runnable yet" —
           see the start_blocked parameter doc in process.h. */
        p->state      = start_blocked ? PROCESS_BLOCKED : PROCESS_READY;
        p->entry      = 0;                   /* unused: entry is ring 3 */
        p->arg        = (void *)user_entry;  /* user process's EIP */
        p->stack      = process_stacks[i];
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
        p->waiting_for_pid = 0;
        return p;
    }
    return 0;
}

void process_make_ready(process_t *p) {
    if (p && p->state == PROCESS_BLOCKED)
        p->state = PROCESS_READY;
}

/* Unwinds a fork() that ran out of memory partway through copying the
   address space: frees every data page currently mapped in cr3's user
   region (PDE 2 and up — PDE 0/1 are the shared kernel identity map,
   never touched here) plus the directory page itself.
   Walking the child's OWN (partially built) directory, instead of
   keeping a separate list of what was allocated so far, is deliberate:
   process_fork() runs on the calling process's kernel stack, which is
   only PROCESS_STACK_SIZE bytes — there's no room to spare for a
   tracking array sized for "however many pages this process happens
   to have". The directory itself already records exactly that.
   This does NOT free the intermediate page-table pages that
   vmm_map_user_page() may have allocated along the way — same
   accepted limitation as process_exit() never reclaiming a process's
   address space at all (see its comment below). */
static void fork_free_address_space(uint32_t cr3) {
    uint32_t *pd = (uint32_t *)cr3;
    for (uint32_t di = 2; di < 1024; di++) {
        if (!(pd[di] & VMM_PRESENT)) continue;
        uint32_t *pt = (uint32_t *)(pd[di] & 0xFFFFF000);
        for (uint32_t ti = 0; ti < 1024; ti++) {
            if (pt[ti] & VMM_PRESENT)
                pmm_free_page(pt[ti] & 0xFFFFF000);
        }
    }
    pmm_free_page(cr3);
}

process_t *process_fork(process_t *parent, const uint32_t *saved_frame) {
    if (!parent || !saved_frame)
        return 0;

    /* ── 1. atomically claim a free process slot ──────────────────
       Protected by cli/sti so two fork()s interleaved by the
       preemptive timer can't both pick the same slot — see the
       comment above process_spawn() for the equivalent, currently
       unprotected race in that function and process_spawn_user(). */
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

    /* ── 2. duplicate the address space: full copy, not COW ────────
       Walks every present PDE/PTE beyond the shared kernel mapping
       (PDE 0/1), so this naturally covers whatever the parent has
       mapped — code, data, stack, anything else — not a fixed list
       of regions. Each page gets a fresh physical frame (allocated
       via pmm_alloc_page(), matching the same convention elf_load()
       already uses: content is copied through the raw physical
       address, assumed to be within the identity-mapped first 8MB)
       and is mapped into the child's directory via the same
       vmm_map_user_page() exec() already relies on. */
    uint32_t child_cr3 = vmm_create_directory();
    if (!child_cr3) {
        child->state = PROCESS_UNUSED;
        child->pid   = 0;
        return 0;
    }

    uint32_t *parent_pd = (uint32_t *)parent->cr3;
    int failed = 0;

    for (uint32_t di = 2; di < 1024 && !failed; di++) {
        if (!(parent_pd[di] & VMM_PRESENT)) continue;
        uint32_t *parent_pt = (uint32_t *)(parent_pd[di] & 0xFFFFF000);

        for (uint32_t ti = 0; ti < 1024; ti++) {
            if (!(parent_pt[ti] & VMM_PRESENT)) continue;

            uint32_t virt        = (di << 22) | (ti << 12);
            uint32_t parent_phys = parent_pt[ti] & 0xFFFFF000;
            uint32_t child_phys  = pmm_alloc_page();
            if (!child_phys) { failed = 1; break; }

            uint32_t *src = (uint32_t *)parent_phys;
            uint32_t *dst = (uint32_t *)child_phys;
            for (uint32_t w = 0; w < PAGE_SIZE / 4; w++) dst[w] = src[w];

            if (vmm_map_user_page(child_cr3, virt, child_phys) != 0) {
                pmm_free_page(child_phys);   /* not mapped, so the unwind below can't find it */
                failed = 1;
                break;
            }
        }
    }

    if (failed) {
        fork_free_address_space(child_cr3);
        child->state = PROCESS_UNUSED;
        child->pid   = 0;
        return 0;
    }

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
    if (!process || process->state == PROCESS_UNUSED)
        return;

    /* KNOWN LEAK (pre-existing, out of scope): this never frees
       process->cr3 or any of the physical pages mapped under it —
       the process's whole address space (and, for a forked child,
       its independent copy of every page) is simply abandoned. Slots
       are still safely reusable since process_spawn()/process_fork()
       always allocate a fresh cr3 for whatever they build next. */
    uint32_t exited_pid = process->pid;

    process->state = PROCESS_UNUSED;
    process->pid   = 0;

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
        case PROCESS_UNUSED:   return "unused";
        case PROCESS_READY:    return "ready";
        case PROCESS_RUNNING:  return "running";
        case PROCESS_SLEEPING: return "sleep";
        case PROCESS_BLOCKED:  return "blocked";
        case PROCESS_ZOMBIE:   return "zombie";
        default:               return "?";
    }
}

void process_dump(void) {
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("[PROC] ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("PID  STATE     RUNS  ESP       NAME\n");

    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_t *process = &process_table[i];
        if (process->state == PROCESS_UNUSED)
            continue;

        vga_puts("       ");
        vga_putdec(process->pid);
        vga_puts("    ");
        vga_puts(process_state_name(process->state));
        vga_puts("    ");
        vga_putdec(process->runs);
        vga_puts("    ");
        vga_puthex(process->esp);
        vga_puts("    ");
        vga_puts(process->name);
        vga_puts("\n");
    }
}
