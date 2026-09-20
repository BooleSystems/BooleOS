// nullos/kernel/syscall.c
#include "syscall.h"
#include "process.h"
#include "scheduler.h"
#include "keyboard.h"
#include "timer.h"
#include "drivers/vga.h"
#include "memory/pmm.h"
#include "memory/heap.h"
#include "exec.h"
#include "memory/vmm.h"
#include "fs/vfs.h"
#include "fs/fat16.h"
#include "ramfs.h"
#include "drivers/pci.h"
#include "pipe.h"
#include "power.h"
#include <stdint.h>

/* ── file descriptor table ─────────────────────────────────── */

#define FD_PER_PROC  8
#define FD_BASE      3   /* 0=stdin,1=stdout,2=stderr reserved */

/* indexed by [process slot][local fd] */
static vfs_fd_t fd_table[PROCESS_MAX][FD_PER_PROC];

/* returns the current process's slot in the process table, or -1 */
static int proc_slot(void) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (process_at(i) == process_current())
            return (int)i;
    }
    return -1;
}

/* returns p's slot in the process table, or -1 */
static int slot_of(process_t *p) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (process_at(i) == p)
            return (int)i;
    }
    return -1;
}

/* Resolves a byte in cur's virtual address space to its identity-mapped
   physical address — but ONLY if that byte's page is both mapped and
   user-accessible (VMM_USER) in cur's OWN directory, via
   vmm_get_user_phys_from_dir(). This is deliberately stricter than the
   plain vmm_get_phys_from_dir(): every process's directory clones the
   kernel's own PDE0/PDE1 (identity map of the first 8MB — kernel heap,
   page tables, IDT, ...), so that region always resolves as "present",
   but it's mapped VMM_KERNEL, not VMM_USER. Every caller in this file
   that touches a userland-supplied address — directly or through
   copy_from_user()/copy_to_user()/copy_user_str() below — goes through
   this, so none of them can be pointed at that shared kernel region.
   See PROGRESS.md for the full writeup (this used to only check
   "present", which is what let sys_write/sys_read/sys_write_file/
   sys_meminfo, and separately sys_open/sys_create/sys_exec/sys_getarg
   via copy_user_str(), treat that region as a legitimate buffer). */
static char *user_kptr(process_t *cur, uint32_t uaddr) {
    uint32_t phys = vmm_get_user_phys_from_dir(cur->cr3, uaddr);
    if (!phys) return (char *)0;
    return (char *)((phys & ~0xFFFu) | (uaddr & 0xFFFu));
}

/* ── user pointer validation / safe copy ─────────────────────────
   Central point every syscall that touches a userland pointer MUST go
   through before reading or writing it (or, for a bounded/whole-range
   check, calling user_kptr() one byte at a time — see copy_user_str()
   below — is an acceptable, already-existing alternative, since it
   goes through the same VMM_USER-checked resolution). Before this
   existed, sys_write, sys_read, sys_write_file and sys_meminfo all
   dereferenced a raw userland-supplied address directly — a process
   could pass a kernel address as its "buffer" and the kernel would
   happily read kernel memory out to it (info leak) or write into it
   (kernel memory corruption / trivial privilege escalation from ring
   3), or just crash the whole machine on any unmapped address, since
   exception_handler() halts unconditionally on a page fault. */
static int user_ptr_valid(process_t *cur, uint32_t uaddr, uint32_t len) {
    if (!cur || len == 0) return 0;
    if (uaddr + len < uaddr) return 0;   /* reject address-space wraparound */

    uint32_t page      = uaddr & ~0xFFFu;
    uint32_t last_page  = (uaddr + len - 1) & ~0xFFFu;
    for (;;) {
        if (!vmm_get_user_phys_from_dir(cur->cr3, page)) return 0;
        if (page == last_page) break;
        page += 0x1000u;
    }
    return 1;
}

/* Copies len bytes from cur's userland address usrc into the kernel
   buffer kdst. Validates the WHOLE range up front — nothing is copied
   if any byte of it turns out to be unmapped, so callers never see a
   partially-filled buffer on failure. */
static int copy_from_user(process_t *cur, void *kdst, uint32_t usrc, uint32_t len) {
    if (!user_ptr_valid(cur, usrc, len)) return -1;
    uint8_t *dst = (uint8_t *)kdst;
    for (uint32_t i = 0; i < len; i++) {
        char *kp = user_kptr(cur, usrc + i);
        if (!kp) return -1;
        dst[i] = (uint8_t)*kp;
    }
    return 0;
}

/* Same as copy_from_user, opposite direction: writes len bytes from the
   kernel buffer ksrc into cur's userland address udst. */
static int copy_to_user(process_t *cur, uint32_t udst, const void *ksrc, uint32_t len) {
    if (!user_ptr_valid(cur, udst, len)) return -1;
    const uint8_t *src = (const uint8_t *)ksrc;
    for (uint32_t i = 0; i < len; i++) {
        char *kp = user_kptr(cur, udst + i);
        if (!kp) return -1;
        *kp = (char)src[i];
    }
    return 0;
}

/* ── SYS_FORK support ──────────────────────────────────────────
   Set by isr128 (see isr.asm) right after 'pusha', to the ESP value
   at that point — a pointer to a 13-word block: the 8 pusha
   registers followed by the CPU's ring3->ring0 trap frame (eip, cs,
   eflags, user_esp, user_ss). That's everything needed to resume the
   interrupted ring-3 execution via 'popa; iret', which is exactly
   what process_fork() fabricates for a new child's first scheduling.

   IMPORTANT: this global is only valid synchronously, for the
   syscall currently being dispatched. If sys_fork() below used it
   directly during the (potentially preempted, possibly slow) page
   copy in process_fork(), a different process's syscall entry could
   overwrite it in the meantime, corrupting the child being built.
   sys_fork() copies it into a LOCAL buffer as its very first action,
   before anything else, and nothing beyond that point — in this file
   or elsewhere — should read g_syscall_frame again for this call. */
uint32_t g_syscall_frame = 0;

#define SYSCALL_FRAME_WORDS 13
#define SYSCALL_FRAME_EAX   7   /* index of the saved EAX within the block above */

static uint32_t sys_fork(void) {
    /* snapshot FIRST — see the comment on g_syscall_frame above */
    uint32_t frame[SYSCALL_FRAME_WORDS];
    const uint32_t *src = (const uint32_t *)g_syscall_frame;
    for (int i = 0; i < SYSCALL_FRAME_WORDS; i++) frame[i] = src[i];
    frame[SYSCALL_FRAME_EAX] = 0;   /* the child sees fork() return 0 */

    process_t *parent = process_current();
    if (!parent) return (uint32_t)-1;
    int parent_slot = proc_slot();
    if (parent_slot < 0) return (uint32_t)-1;

    process_t *child = process_fork(parent, frame);
    if (!child) return (uint32_t)-1;

    /* duplicate the parent's open files into the child's own slot.
       For a pipe-backed fd, this raw struct copy makes the child a
       second real holder of that pipe end — vfs_dup() bumps its
       refcount to match; without it, closing just ONE of the two
       copies would decrement a refcount that was never incremented
       for this duplication, letting the pipe appear fully closed on
       that end while the other copy is still genuinely open (see
       docs/pipes.md). ramfs/FAT16 entries aren't refcounted, so
       vfs_dup() is a no-op for them, same as always. */
    int child_slot = slot_of(child);
    if (child_slot >= 0) {
        for (uint32_t j = 0; j < FD_PER_PROC; j++) {
            fd_table[child_slot][j] = fd_table[parent_slot][j];
            vfs_dup(&fd_table[child_slot][j]);
        }
    }

    return child->pid;
}

/* Writes are staged through a small kernel buffer and copied in with
   copy_from_user() in chunks, same discipline as SYS_READ_CHUNK below
   and sys_write_file()'s kmalloc'd buffer — never hands vfs_write() a
   raw userland pointer. Used by the fd>=FD_BASE path: pipes and FAT16
   files (a stream write at fd->pos, see vfs_write() — consecutive chunks
   accumulate; SYS_WRITE_FILE is the separate whole-file "save" path). */
#define SYS_WRITE_CHUNK 128

static uint32_t sys_write(uint32_t fd, const char *buf, uint32_t len) {
    if (!buf) return (uint32_t)-1;
    if (len == 0) return 0;

    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    if (!user_ptr_valid(cur, (uint32_t)buf, len)) return (uint32_t)-1;

    /* fd 1/2 (stdout/stderr) redirected to an already-open fd (Phase
       16: SYS_EXEC_PIPE) — resolve to the real target before anything
       else, so the rest of this function never needs to know the
       write was redirected. */
    if ((fd == 1 || fd == 2) && cur->stdout_redirect >= 0)
        fd = (uint32_t)cur->stdout_redirect;

    if (fd >= FD_BASE) {
        int slot = proc_slot();
        if (slot < 0) return (uint32_t)-1;
        uint32_t idx = fd - FD_BASE;
        if (idx >= FD_PER_PROC) return (uint32_t)-1;
        vfs_fd_t *f = &fd_table[slot][idx];
        if (!f->used) return (uint32_t)-1;

        uint32_t total = 0;
        while (total < len) {
            char     kchunk[SYS_WRITE_CHUNK];
            uint32_t want = len - total;
            if (want > SYS_WRITE_CHUNK) want = SYS_WRITE_CHUNK;

            if (copy_from_user(cur, kchunk, (uint32_t)buf + total, want) < 0)
                return (total > 0) ? total : (uint32_t)-1;

            int r = vfs_write(f, kchunk, want);
            if (r < 0) return (total > 0) ? total : (uint32_t)-1;

            total += want;
        }
        return total;
    }

    /* fd 0/1/2 with no active redirect: stdout/stderr go to VGA,
       exactly as before pipes existed. */
    for (uint32_t i = 0; i < len; i++) {
        char *kp = user_kptr(cur, (uint32_t)buf + i);
        vga_putchar(*kp);
    }
    return len;
}

static uint32_t sys_exit(uint32_t code) {
    (void)code;
    int slot = proc_slot();
    if (slot >= 0) {
        for (uint32_t j = 0; j < FD_PER_PROC; j++)
            if (fd_table[slot][j].used)
                vfs_close(&fd_table[slot][j]);
    }
    process_t *p = process_current();
    if (p) process_exit(p);
    scheduler_yield();
    for (;;) __asm__ volatile ("hlt");
    return 0;
}

static uint32_t sys_yield(void) {
    scheduler_yield();
    return 0;
}

static uint32_t sys_getpid(void) {
    process_t *p = process_current();
    return p ? p->pid : 0;
}

/* PID of the process in raw mode (no keyboard echo); -1 = none */
static int raw_mode_pid = -1;

/* file reads are staged through a small kernel buffer and copied out
   with copy_to_user() in chunks, instead of ever handing the VFS/FAT16
   backends the raw userland pointer directly — see the comment on
   user_ptr_valid() above for why. */
#define SYS_READ_CHUNK 128

static uint32_t sys_read(uint32_t fd, char *buf, uint32_t len) {
    if (!buf || len == 0) return (uint32_t)-1;

    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    if (!user_ptr_valid(cur, (uint32_t)buf, len)) return (uint32_t)-1;

    /* fd 0 (stdin) redirected to an already-open fd (Phase 16:
       SYS_EXEC_PIPE) — resolve before anything else. */
    if (fd == 0 && cur->stdin_redirect >= 0)
        fd = (uint32_t)cur->stdin_redirect;

    /* fd >= FD_BASE: file read via VFS */
    if (fd >= FD_BASE) {
        int slot = proc_slot();
        if (slot < 0) return (uint32_t)-1;
        uint32_t idx = fd - FD_BASE;
        if (idx >= FD_PER_PROC) return (uint32_t)-1;
        vfs_fd_t *f = &fd_table[slot][idx];
        if (!f->used) return (uint32_t)-1;

        uint32_t total = 0;
        while (total < len) {
            char     kchunk[SYS_READ_CHUNK];
            uint32_t want = len - total;
            if (want > SYS_READ_CHUNK) want = SYS_READ_CHUNK;

            int r = vfs_read(f, kchunk, want);
            if (r < 0) return (total > 0) ? total : (uint32_t)-1;
            if (r == 0) break;   /* EOF */

            if (copy_to_user(cur, (uint32_t)buf + total, kchunk, (uint32_t)r) < 0)
                return (total > 0) ? total : (uint32_t)-1;

            total += (uint32_t)r;
            if ((uint32_t)r < want) break;   /* short read: EOF mid-chunk */
        }
        return total;
    }

    /* fd == 0: keyboard */
    if (fd != 0) return (uint32_t)-1;

    uint32_t n = 0;
    while (n < len) {
        int c;
        while ((c = keyboard_getchar_nowait()) == -1)
            scheduler_sleep_current(1);

        if (c == 0x03) {
            vga_puts("^C\n");
            char ctrlc = 0x03;
            if (copy_to_user(cur, (uint32_t)buf, &ctrlc, 1) < 0) return (uint32_t)-1;
            return 1;
        }

        int in_raw = (raw_mode_pid >= 0 &&
                      process_current() &&
                      (int)process_current()->pid == raw_mode_pid);
        /* echo — except a backspace with nothing to erase: vga_putchar('\b')
           would blank the character to the left of the cursor, i.e. eat the
           shell's own "> " prompt. */
        if (!in_raw && !(c == '\b' && n == 0))
            vga_putchar((char)c);

        if (c == '\b') {
            if (n > 0) n--;     /* backspace: discards the last char */
            continue;
        }

        char ch = (char)c;
        if (copy_to_user(cur, (uint32_t)buf + n, &ch, 1) < 0) return (uint32_t)-1;
        n++;

        if (c == '\n')
            break;
    }
    return n;
}

static uint32_t sys_uptime(void) {
    return timer_get_ticks();
}

/* pmm_uaddr/heap_uaddr/procs_uaddr are raw userland addresses (0 = "skip
   this one", matching the old NULL-pointer-skips-it behavior) — each is
   copied out individually via copy_to_user() rather than dereferenced
   directly, so a bad address just fails that one output instead of
   writing 4 bytes wherever it happened to point. */
static uint32_t sys_meminfo(uint32_t pmm_uaddr, uint32_t heap_uaddr, uint32_t procs_uaddr) {
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;

    uint32_t pmm_val = pmm_free_pages();
    if (pmm_uaddr && copy_to_user(cur, pmm_uaddr, &pmm_val, sizeof(pmm_val)) < 0)
        return (uint32_t)-1;

    uint32_t heap_val = heap_free_bytes();
    if (heap_uaddr && copy_to_user(cur, heap_uaddr, &heap_val, sizeof(heap_val)) < 0)
        return (uint32_t)-1;

    if (procs_uaddr) {
        uint32_t n = 0;
        for (uint32_t i = 0; i < PROCESS_MAX; i++) {
            process_t *p = process_at(i);
            if (p && p->state != PROCESS_UNUSED) n++;
        }
        if (copy_to_user(cur, procs_uaddr, &n, sizeof(n)) < 0) return (uint32_t)-1;
    }
    return 0;
}

static uint32_t sys_ps(void) {
    process_dump();
    return 0;
}

/* argument passed by the last SYS_EXEC (e.g. filename for the editor) */
static char exec_arg[64];

#define USER_STR_MAX 64

/* copies a string from the user's virtual address into a kernel buf */
static int copy_user_str(process_t *cur, uint32_t uaddr, char *buf, uint32_t maxlen) {
    uint32_t i;
    for (i = 0; i < maxlen - 1; i++) {
        char *kp = user_kptr(cur, uaddr + i);
        if (!kp) return -1;
        char c = *kp;
        buf[i] = c;
        if (c == '\0') break;
    }
    buf[i] = '\0';
    return 0;
}

/* Creates a pipe and writes {read_fd, write_fd} into the caller's
   user_fds[2] (both real fds, >= FD_BASE, usable with plain
   nos_read()/nos_write() like any other fd — no pipe-specific
   syscalls beyond this one and SYS_EXEC_PIPE). See docs/pipes.md for
   the buffer/refcounting design pipe_create() implements. */
static uint32_t sys_pipe(uint32_t user_fds) {
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    int slot = proc_slot();
    if (slot < 0) return (uint32_t)-1;

    uint32_t pipe_idx;
    if (pipe_create(&pipe_idx) < 0) return (uint32_t)-1;

    int read_j = -1;
    for (uint32_t j = 0; j < FD_PER_PROC; j++) {
        if (!fd_table[slot][j].used) { read_j = (int)j; break; }
    }
    if (read_j < 0) {
        pipe_release_read(pipe_idx);
        pipe_release_write(pipe_idx);
        return (uint32_t)-1;
    }
    fd_table[slot][read_j].used    = 1;
    fd_table[slot][read_j].backend = VFS_PIPE_READ;
    fd_table[slot][read_j].first   = pipe_idx;
    fd_table[slot][read_j].size    = 0;
    fd_table[slot][read_j].pos     = 0;

    int write_j = -1;
    for (uint32_t j = 0; j < FD_PER_PROC; j++) {
        if ((int)j != read_j && !fd_table[slot][j].used) { write_j = (int)j; break; }
    }
    if (write_j < 0) {
        vfs_close(&fd_table[slot][read_j]);
        pipe_release_write(pipe_idx);
        return (uint32_t)-1;
    }
    fd_table[slot][write_j].used    = 1;
    fd_table[slot][write_j].backend = VFS_PIPE_WRITE;
    fd_table[slot][write_j].first   = pipe_idx;
    fd_table[slot][write_j].size    = 0;
    fd_table[slot][write_j].pos     = 0;

    int32_t fds[2] = { (int32_t)(FD_BASE + (uint32_t)read_j), (int32_t)(FD_BASE + (uint32_t)write_j) };
    if (copy_to_user(cur, user_fds, fds, sizeof(fds)) < 0) {
        vfs_close(&fd_table[slot][read_j]);
        vfs_close(&fd_table[slot][write_j]);
        return (uint32_t)-1;
    }
    return 0;
}

static uint32_t sys_open(const char *user_name) {
    if (!user_name) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    int slot = proc_slot();
    if (slot < 0) return (uint32_t)-1;

    char kname[USER_STR_MAX];
    if (copy_user_str(cur, (uint32_t)user_name, kname, USER_STR_MAX) < 0)
        return (uint32_t)-1;

    /* find a free slot */
    for (uint32_t j = 0; j < FD_PER_PROC; j++) {
        if (!fd_table[slot][j].used) {
            if (vfs_open(cur->cwd_cluster, kname, &fd_table[slot][j]) < 0)
                return (uint32_t)-1;
            return FD_BASE + j;
        }
    }
    return (uint32_t)-1;  /* no free slots */
}

static uint32_t sys_create(const char *user_name) {
    if (!user_name) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    int slot = proc_slot();
    if (slot < 0) return (uint32_t)-1;

    char kname[USER_STR_MAX];
    if (copy_user_str(cur, (uint32_t)user_name, kname, USER_STR_MAX) < 0)
        return (uint32_t)-1;

    for (uint32_t j = 0; j < FD_PER_PROC; j++) {
        if (!fd_table[slot][j].used) {
            if (vfs_create(cur->cwd_cluster, kname, &fd_table[slot][j]) < 0)
                return (uint32_t)-1;
            return FD_BASE + j;
        }
    }
    return (uint32_t)-1;  /* no free slots */
}

static uint32_t sys_close(uint32_t fd) {
    if (fd < FD_BASE) return (uint32_t)-1;
    int slot = proc_slot();
    if (slot < 0) return (uint32_t)-1;
    uint32_t idx = fd - FD_BASE;
    if (idx >= FD_PER_PROC) return (uint32_t)-1;
    if (!fd_table[slot][idx].used) return (uint32_t)-1;
    vfs_close(&fd_table[slot][idx]);
    return 0;
}

static uint32_t sys_exec(const char *user_name, uint32_t user_arg) {
    if (!user_name) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;

    char kname[USER_STR_MAX];
    if (copy_user_str(cur, (uint32_t)user_name, kname, USER_STR_MAX) < 0)
        return (uint32_t)-1;

    /* copies the optional argument (e.g. filename for the editor) */
    exec_arg[0] = '\0';
    if (user_arg)
        copy_user_str(cur, user_arg, exec_arg, sizeof(exec_arg));

    process_t *p = exec(kname, cur->cwd_cluster, 0);
    if (!p) return (uint32_t)-1;
    return p->pid;
}

/* Launches a program exactly like sys_exec(), except the NEW process's
   fd 0/1 are redirected to the caller's OWN already-open stdin_fd/
   stdout_fd (each (uint32_t)-1 means "don't redirect that one" — the
   new process keeps the default keyboard/VGA behavior for it). Used
   only by the shell's "cmd1 | cmd2" pipeline launch (see
   docs/pipes.md); plain "run"/"edit" keep using SYS_EXEC unchanged.

   Spawns the new process PROCESS_BLOCKED (exec()'s start_blocked=1 —
   see process_spawn_user()'s doc comment in process.h for exactly
   why) and only flips it to PROCESS_READY via process_make_ready()
   once its fd_table row has been fully seeded below — otherwise a
   preemptive timer tick landing in between could let the scheduler
   run it with a stdin/stdout redirect index that doesn't point at
   anything yet. */
static uint32_t sys_exec_pipe(const char *user_name, uint32_t stdin_fd, uint32_t stdout_fd) {
    if (!user_name) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    int slot = proc_slot();
    if (slot < 0) return (uint32_t)-1;

    char kname[USER_STR_MAX];
    if (copy_user_str(cur, (uint32_t)user_name, kname, USER_STR_MAX) < 0)
        return (uint32_t)-1;

    /* SYS_EXEC_PIPE carries no argument, but exec_arg is one global that only
       sys_exec() ever reset — without clearing it here, the new process's
       SYS_GETARG would return whatever argument the LAST plain exec() left
       behind (e.g. "cat" after "cat notes.txt" would then try to open
       notes.txt instead of reading its redirected stdin). */
    exec_arg[0] = '\0';

    int want_stdin  = (stdin_fd  != (uint32_t)-1);
    int want_stdout = (stdout_fd != (uint32_t)-1);

    vfs_fd_t *stdin_src = 0, *stdout_src = 0;
    if (want_stdin) {
        if (stdin_fd < FD_BASE) return (uint32_t)-1;
        uint32_t idx = stdin_fd - FD_BASE;
        if (idx >= FD_PER_PROC || !fd_table[slot][idx].used) return (uint32_t)-1;
        stdin_src = &fd_table[slot][idx];
    }
    if (want_stdout) {
        if (stdout_fd < FD_BASE) return (uint32_t)-1;
        uint32_t idx = stdout_fd - FD_BASE;
        if (idx >= FD_PER_PROC || !fd_table[slot][idx].used) return (uint32_t)-1;
        stdout_src = &fd_table[slot][idx];
    }

    process_t *p = exec(kname, cur->cwd_cluster, 1 /* start_blocked */);
    if (!p) return (uint32_t)-1;

    int new_slot = slot_of(p);
    if (new_slot < 0) {
        /* shouldn't happen — stay safe rather than leak a stuck slot */
        process_exit(p);
        return (uint32_t)-1;
    }

    if (want_stdin) {
        int placed = 0;
        for (uint32_t j = 0; j < FD_PER_PROC; j++) {
            if (!fd_table[new_slot][j].used) {
                fd_table[new_slot][j] = *stdin_src;
                vfs_dup(&fd_table[new_slot][j]);
                p->stdin_redirect = (int)(FD_BASE + j);
                placed = 1;
                break;
            }
        }
        if (!placed) { process_exit(p); return (uint32_t)-1; }
    }
    if (want_stdout) {
        int placed = 0;
        for (uint32_t j = 0; j < FD_PER_PROC; j++) {
            if (!fd_table[new_slot][j].used) {
                fd_table[new_slot][j] = *stdout_src;
                vfs_dup(&fd_table[new_slot][j]);
                p->stdout_redirect = (int)(FD_BASE + j);
                placed = 1;
                break;
            }
        }
        if (!placed) { process_exit(p); return (uint32_t)-1; }
    }

    process_make_ready(p);
    return p->pid;
}

static uint32_t sys_getarg(char *user_buf, uint32_t len) {
    if (!user_buf || len == 0) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    uint32_t n = 0;
    while (n < len - 1 && exec_arg[n]) {
        char *kp = user_kptr(cur, (uint32_t)user_buf + n);
        if (!kp) return (uint32_t)-1;
        *kp = exec_arg[n];
        n++;
    }
    char *kp = user_kptr(cur, (uint32_t)user_buf + n);
    if (kp) *kp = '\0';
    return n;
}

static void puts_padded(const char *s, int width) {
    int n = 0;
    while (s[n]) { vga_putchar(s[n++]); }
    while (n++ < width) vga_putchar(' ');
}

/* user_path may be NULL/empty, meaning "list the caller's cwd". When a
   non-empty path is given, it's resolved via fat16_resolve_dir() (the
   same function SYS_CHDIR uses) BEFORE anything is printed — an
   unresolvable/non-directory path is reported and nothing is listed,
   instead of silently falling back to the cwd. */
static uint32_t sys_readdir(const char *user_path) {
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;

    uint32_t list_cluster = cur->cwd_cluster;

    if (user_path) {
        char kpath[USER_STR_MAX];
        if (copy_user_str(cur, (uint32_t)user_path, kpath, USER_STR_MAX) < 0)
            return (uint32_t)-1;
        if (kpath[0]) {
            if (!fat16_available()) return (uint32_t)-1;
            int r = fat16_resolve_dir(cur->cwd_cluster, kpath, &list_cluster);
            if (r != 1) {
                vga_puts("ls: no such directory: ");
                vga_puts(kpath);
                vga_puts("\n");
                return (uint32_t)-1;
            }
        }
    }

    int any = 0;
    int listing_root = (list_cluster == 0);

    /* ramfs is always flat — only shown when listing the actual root,
       since it never gained subdirectories in this phase */
    if (listing_root && ramfs_base) {
        vga_puts("ramfs:\n");
        /* accesses n_entries and entries directly via ramfs_h */
        uint32_t n = *(uint32_t *)ramfs_base;
        ramfs_entry_t *entries = (ramfs_entry_t *)(ramfs_base + sizeof(uint32_t));
        for (uint32_t i = 0; i < n; i++) {
            vga_puts("  ");
            puts_padded(entries[i].name, 20);
            vga_putdec(entries[i].size);
            vga_puts(" B\n");
        }
        any = 1;
    }

    /* FAT16 */
    if (fat16_available()) {
        vga_puts("fat16:\n");
        char name[13];
        uint32_t size;
        uint8_t is_dir;
        for (uint32_t idx = 0; fat16_readdir(list_cluster, idx, name, &size, &is_dir); idx++) {
            vga_puts("  ");
            puts_padded(name, 20);
            if (is_dir) {
                vga_puts("<DIR>\n");
            } else {
                vga_putdec(size);
                vga_puts(" B\n");
            }
            any = 1;
        }
    }

    if (!any) vga_puts("(no files)\n");
    return 0;
}

static uint32_t sys_mkdir(const char *user_name) {
    if (!user_name) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;

    char kname[USER_STR_MAX];
    if (copy_user_str(cur, (uint32_t)user_name, kname, USER_STR_MAX) < 0)
        return (uint32_t)-1;

    if (!fat16_available()) return (uint32_t)-1;
    return (fat16_mkdir(cur->cwd_cluster, kname) == 0) ? 0 : (uint32_t)-1;
}

/* Only ever mutates cur->cwd_cluster on confirmed success (fat16_resolve_dir
   returning 1) — every failure path (path doesn't exist, names a file
   instead of a directory, or an I/O error) returns early without
   touching it, same discipline fat16_create already uses for I/O
   errors: never guess, never leave the process in a half-changed state. */
static uint32_t sys_chdir(const char *user_path) {
    if (!user_path) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;

    char kpath[USER_STR_MAX];
    if (copy_user_str(cur, (uint32_t)user_path, kpath, USER_STR_MAX) < 0)
        return (uint32_t)-1;

    if (!fat16_available()) return (uint32_t)-1;

    uint32_t new_cluster;
    int r = fat16_resolve_dir(cur->cwd_cluster, kpath, &new_cluster);
    if (r != 1) return (uint32_t)-1;

    cur->cwd_cluster = new_cluster;
    return 0;
}

/* Writes the caller's cwd as an absolute path. fat16_get_path() builds it
   by walking up through ".." (a process only stores its cwd's cluster,
   never a path string). Returns the path length (excluding the NUL) or -1
   if buf is too small, the path can't be reconstructed, or on I/O error. */
#define GETCWD_MAX 128

static uint32_t sys_getcwd(char *user_buf, uint32_t len) {
    if (!user_buf || len == 0) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    if (!user_ptr_valid(cur, (uint32_t)user_buf, len)) return (uint32_t)-1;

    char kbuf[GETCWD_MAX];
    int n = fat16_get_path(cur->cwd_cluster, kbuf, sizeof(kbuf));
    if (n < 0) return (uint32_t)-1;
    if ((uint32_t)n + 1 > len) return (uint32_t)-1;

    if (copy_to_user(cur, (uint32_t)user_buf, kbuf, (uint32_t)n + 1) < 0)
        return (uint32_t)-1;
    return (uint32_t)n;
}

static uint32_t sys_reboot(void)   { return (uint32_t)power_reboot(); }
static uint32_t sys_shutdown(void) { return (uint32_t)power_shutdown(); }

static uint32_t sys_set_raw_mode(uint32_t enable) {
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    if (enable)
        raw_mode_pid = (int)cur->pid;
    else if (raw_mode_pid == (int)cur->pid)
        raw_mode_pid = -1;
    return 0;
}

/* Real blocking wait for one SPECIFIC pid (Phase 16) — replaces the
   original polling implementation (scheduler_sleep_current(10),
   rechecking every 100ms) with the same check-then-block-atomically
   pattern ata_wait_irq() uses: the "is it still alive?" scan and the
   transition to PROCESS_BLOCKED happen inside one cli/sti section, so
   an exit that happens between the check and the block can never be
   missed. process_exit() (process.c) is what actually wakes a
   process out of this wait, matching waiting_for_pid to the exiting
   pid specifically — not a generic "some child exited" signal, so a
   process waiting on one specific child is never woken by a
   different one. The syscall's own interface (a specific pid, not
   "any child") never changed; only the implementation went from
   polling to real blocking, so nos_wait()/every existing caller is
   unaffected. */
static uint32_t sys_wait(uint32_t pid) {
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;

    for (;;) {
        __asm__ volatile ("cli");
        int alive = 0;
        for (uint32_t i = 0; i < PROCESS_MAX; i++) {
            process_t *p = process_at(i);
            if (p && p->pid == pid && p->state != PROCESS_UNUSED) {
                alive = 1;
                break;
            }
        }
        if (!alive) { __asm__ volatile ("sti"); return 0; }

        cur->waiting_for_pid = pid;
        cur->state = PROCESS_BLOCKED;
        __asm__ volatile ("sti");

        scheduler_block_current();   /* resumes once process_exit() wakes us for this pid */
        cur->waiting_for_pid = 0;
    }
}

static uint32_t sys_kill(uint32_t pid) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_t *p = process_at(i);
        if (p && p->pid == pid && p->state != PROCESS_UNUSED) {
            process_exit(p);
            return 0;
        }
    }
    return (uint32_t)-1;
}

/* Caps a single write_file() call well above anything a real file this
   OS deals with needs (edit.c's whole buffer is 4096 bytes) and well
   below the heap's total capacity, so one write can't hog the entire
   kernel heap. This is on top of, not instead of, the kmalloc() overflow
   fix in heap.c — that fix stops a huge len from ever under-allocating,
   this one stops a large-but-not-overflowing len from being handed to
   kmalloc at all. */
#define SYS_WRITE_FILE_MAX_LEN (64u * 1024u)

static uint32_t sys_write_file(uint32_t fd, uint32_t user_buf, uint32_t len) {
    if (fd < FD_BASE) return (uint32_t)-1;
    int slot = proc_slot();
    if (slot < 0) return (uint32_t)-1;
    uint32_t idx = fd - FD_BASE;
    if (idx >= FD_PER_PROC) return (uint32_t)-1;
    vfs_fd_t *f = &fd_table[slot][idx];
    if (!f->used || f->backend != VFS_FAT16) return (uint32_t)-1;

    if (len == 0) return (uint32_t)vfs_write_all(f, (const char *)0, 0);
    if (len > SYS_WRITE_FILE_MAX_LEN) return (uint32_t)-1;

    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;

    /* copies buf from userland into the kernel (heap), validated */
    char *kbuf = (char *)kmalloc(len);
    if (!kbuf) return (uint32_t)-1;

    if (copy_from_user(cur, kbuf, user_buf, len) < 0) {
        kfree(kbuf);
        return (uint32_t)-1;
    }

    int r = vfs_write_all(f, kbuf, len);
    kfree(kbuf);
    return (r < 0) ? (uint32_t)-1 : 0;
}

uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    switch (num) {
        case SYS_WRITE:   return sys_write(arg1, (const char *)arg2, arg3);
        case SYS_EXIT:    return sys_exit(arg1);
        case SYS_YIELD:   return sys_yield();
        case SYS_GETPID:  return sys_getpid();
        case SYS_READ:    return sys_read(arg1, (char *)arg2, arg3);
        case SYS_UPTIME:  return sys_uptime();
        case SYS_MEMINFO: return sys_meminfo(arg1, arg2, arg3);
        case SYS_PS:      return sys_ps();
        case SYS_KILL:    return sys_kill(arg1);
        case SYS_EXEC:    return sys_exec((const char *)arg1, arg2);
        case SYS_OPEN:     return sys_open((const char *)arg1);
        case SYS_CLOSE:    return sys_close(arg1);
        case SYS_READ_RAW: {
            int r;
            while ((r = keyboard_raw_nowait()) == -1)
                scheduler_sleep_current(1);
            return (uint32_t)r;
        }
        case SYS_GOTOXY:    vga_set_cursor((uint8_t)arg1, (uint8_t)arg2); return 0;
        case SYS_CLEAR:     vga_clear(); return 0;
        case SYS_GETARG:    return sys_getarg((char *)arg1, arg2);
        case SYS_KBD_FLUSH: keyboard_flush(); return 0;
        case SYS_SETCOLOR:     vga_set_color((vga_color_t)arg1, (vga_color_t)arg2); return 0;
        case SYS_SET_RAW_MODE: return sys_set_raw_mode(arg1);
        case SYS_WAIT:         return sys_wait(arg1);
        case SYS_READDIR:      return sys_readdir((const char *)arg1);
        case SYS_WRITE_FILE:   return sys_write_file(arg1, arg2, arg3);
        case SYS_CREATE:       return sys_create((const char *)arg1);
        case SYS_PCI_LIST:     pci_print_list(); return (uint32_t)pci_device_count();
        case SYS_FORK:         return sys_fork();
        case SYS_CHDIR:        return sys_chdir((const char *)arg1);
        case SYS_MKDIR:        return sys_mkdir((const char *)arg1);
        case SYS_PIPE:         return sys_pipe(arg1);
        case SYS_EXEC_PIPE:    return sys_exec_pipe((const char *)arg1, arg2, arg3);
        case SYS_GETCWD:       return sys_getcwd((char *)arg1, arg2);
        case SYS_REBOOT:       return sys_reboot();
        case SYS_SHUTDOWN:     return sys_shutdown();
        default:
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("[SYSCALL] unknown number: ");
            vga_putdec(num);
            vga_puts("\n");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            return (uint32_t)-1;
    }
}
