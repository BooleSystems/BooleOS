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
} process_t;

void process_init(void);
process_t *process_spawn(const char *name, process_entry_t entry, void *arg, void (*bootstrap)(void));
/* cwd_cluster is the new process's starting current directory (0 =
   root). Callers that exec() on behalf of another process (see
   exec()/SYS_EXEC in syscall.c) pass that process's own cwd_cluster,
   so a program launched via "run"/"edit" starts in the same directory
   the caller was in — mirroring how process_fork() already copies
   cwd_cluster from parent to child. Callers with no such "launcher"
   (the kernel spawning the very first process at boot) pass 0. */
process_t *process_spawn_user(const char *name, uint32_t user_entry, uint32_t user_esp, uint32_t cr3, uint32_t cwd_cluster, void (*bootstrap)(void));
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
