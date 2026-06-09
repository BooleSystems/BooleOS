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
    uint32_t wake_tick;
    uint32_t ticks_run;
    uint32_t runs;
} process_t;

void process_init(void);
process_t *process_spawn(const char *name, process_entry_t entry, void *arg, void (*bootstrap)(void));
process_t *process_at(uint32_t index);
process_t *process_current(void);
void process_set_current(process_t *process);
void process_exit(process_t *process);
void process_sleep(process_t *process, uint32_t now, uint32_t ticks);
void process_wake_sleepers(uint32_t now);
void process_dump(void);
const char *process_state_name(process_state_t state);

#endif
