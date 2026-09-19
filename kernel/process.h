// nullos/kernel/process.h
#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define PROCESS_MAX 16
#define PROCESS_NAME_MAX 24
#define PROCESS_STACK_SIZE 4096

typedef enum {
    PROCESS_UNUSED = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_SLEEPING,
    PROCESS_BLOCKED,   /* waiting on an external event (e.g. disk IRQ), not a timer */
    PROCESS_ZOMBIE
} process_state_t;

typedef void (*process_entry_t)(void *arg);

typedef struct process {
    uint32_t pid;
    char name[PROCESS_NAME_MAX];
    process_state_t state;
    process_entry_t entry;
    void *arg;
    uint32_t esp;
    void *stack;
    uint32_t stack_size;
    uint32_t cr3;
    void *user_stack;
    uint32_t user_esp;
    uint32_t wake_tick;
    uint32_t ticks_run;
    uint32_t runs;
    uint32_t cwd_cluster;   /* current working directory on FAT16 (0 = root);
                               see fat16_resolve_dir()/SYS_CHDIR. Copied into
                               the child by process_fork() so "cd" survives
                               across fork(), same as any other process state. */
    int stdin_redirect;     /* -1 = no redirect (SYS_READ fd=0 uses the
                               keyboard, as always); otherwise this process's
                               own fd (>= FD_BASE, see syscall.c) that fd 0
                               actually reads from — set at exec() time for
                               a pipeline stage (SYS_EXEC_PIPE), never
                               changed afterward. Copied parent->child by
                               process_fork(), same as the rest of the fd
                               table it duplicates. */
    int stdout_redirect;    /* same as stdin_redirect, for fd 1 (and 2) —
                               SYS_WRITE uses this instead of VGA when set. */
    uint32_t waiting_for_pid;  /* 0 = not waiting; set by sys_wait() right
                               before blocking, cleared by process_exit()
                               (or by the waiter itself right after waking)
                               — see docs/scheduler.md → "waitpid()". */
} process_t;

void process_init(void);
process_t *process_spawn(const char *name, process_entry_t entry, void *arg, void (*bootstrap)(void));
/* cwd_cluster is the new process's starting current directory (0 =
   root). Callers that exec() on behalf of another process (see
   exec()/SYS_EXEC in syscall.c) pass that process's own cwd_cluster,
   so a program launched via "run"/"edit" starts in the same directory
   the caller was in — mirroring how process_fork() already copies
   cwd_cluster from parent to child. Callers with no such "launcher"
   (the kernel spawning the very first process at boot) pass 0.

   start_blocked: 0 leaves the new process PROCESS_READY immediately
   (the plain exec() path, unchanged from before pipes existed). 1
   leaves it PROCESS_BLOCKED instead — reserved but not schedulable —
   for callers (SYS_EXEC_PIPE) that still need to seed one or two
   fd_table entries in the NEW process's own (otherwise empty) fd
   table before it's safe to run: fd_table lives in syscall.c, so
   process.c can't populate it itself, but that means there's a real
   window between this function returning and the caller finishing
   that seeding — if the process were already READY in that window, a
   preemptive timer tick could let the scheduler switch to it early,
   running with a stdin/stdout redirect index that doesn't point at
   anything yet. This is the exact same "reserved but not runnable
   yet" discipline process_fork() already uses for the same reason;
   see the comment on PROCESS_BLOCKED there. The caller MUST call
   process_make_ready() once the process is fully formed — a process
   left PROCESS_BLOCKED forever is a permanent leak of a process-table
   slot. */
process_t *process_spawn_user(const char *name, uint32_t user_entry, uint32_t user_esp, uint32_t cr3, uint32_t cwd_cluster, int start_blocked, void (*bootstrap)(void));

/* Flips a process from PROCESS_BLOCKED to PROCESS_READY — the
   counterpart to process_spawn_user()'s start_blocked=1. A no-op if
   the process isn't currently PROCESS_BLOCKED (defensive: calling it
   twice, or on a process that failed/exited in the meantime, must
   never resurrect or corrupt an unrelated later occupant of the same
   slot). */
void process_make_ready(process_t *p);
process_t *process_at(uint32_t index);
process_t *process_current(void);
void process_set_current(process_t *process);
void process_exit(process_t *process);

/* Duplicates parent into a brand-new process: full copy (not
   copy-on-write) of its address space, open behavior aside — file
   descriptors are duplicated by the caller (see sys_fork() in
   syscall.c), not here — and a fabricated kernel stack that resumes
   at the exact point the parent called fork(), with eax forced to 0.
   saved_frame must be the 13-word block captured from
   g_syscall_frame (8 pusha registers + the CPU's ring3->ring0 trap
   frame — eip/cs/eflags/user_esp/user_ss), already copied into a
   LOCAL buffer by the caller with its eax slot zeroed; see the
   g_syscall_frame comment in syscall.c and isr128_resume in isr.asm.
   Returns the new process_t*, or NULL if there's no free process
   slot or memory ran out partway through the copy — in both cases
   nothing is left behind (no child, no leaked slot). */
process_t *process_fork(process_t *parent, const uint32_t *saved_frame);
void process_sleep(process_t *process, uint32_t now, uint32_t ticks);
void process_wake_sleepers(uint32_t now);
void process_dump(void);
const char *process_state_name(process_state_t state);

#endif
