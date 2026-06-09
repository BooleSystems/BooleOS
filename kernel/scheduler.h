// nullos/kernel/scheduler.h
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "process.h"

void scheduler_init(void);
process_t *scheduler_spawn(const char *name, process_entry_t entry, void *arg);
void scheduler_tick(uint32_t tick);
void scheduler_run_once(void);
void scheduler_yield(void);
void scheduler_sleep_current(uint32_t ticks);
void scheduler_dump(void);

#endif
