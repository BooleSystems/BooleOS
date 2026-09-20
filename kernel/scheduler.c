// nullos/kernel/scheduler.c
#include "scheduler.h"
#include "timer.h"
#include "hal.h"
#include "memory/vmm.h"
#include "tss.h"
#include "usermode.h"
#include <stdint.h>

extern void context_switch(uint32_t *old_esp, uint32_t new_esp, uint32_t new_cr3);

static uint32_t scheduler_ready = 0;
static uint32_t current_index = PROCESS_MAX - 1;
static uint32_t scheduler_esp = 0;

static void switch_back_to_scheduler(process_t *process) {
    if (!process)
        return;
    context_switch(&process->esp, scheduler_esp, vmm_get_kernel_directory());
}

void scheduler_init(void) {
    process_init();
    scheduler_ready = 1;
    current_index = PROCESS_MAX - 1;
    scheduler_esp = 0;
}

/* Bootstrap for user processes: updates the TSS and jumps to ring 3.
   p->arg holds the user EIP; p->user_esp holds the user ESP. */
static void user_task_bootstrap(void) {
    process_t *p = process_current();
    if (!p) return;
    tss_set_stack(0x10, (uint32_t)p->stack + p->stack_size);
    jump_to_usermode((uint32_t)p->arg, p->user_esp);
    /* does not return */
}

process_t *scheduler_spawn_user(const char *name, uint32_t user_entry,
                                uint32_t user_esp, uint32_t cr3, uint32_t cwd_cluster,
                                int start_blocked) {
    if (!scheduler_ready) return 0;
    return process_spawn_user(name, user_entry, user_esp, cr3, cwd_cluster, start_blocked, user_task_bootstrap);
}

void scheduler_tick(uint32_t tick) {
    if (!scheduler_ready)
        return;
    process_wake_sleepers(tick);
}

void scheduler_run_once(void) {
    if (!scheduler_ready)
        return;
    for (uint32_t scanned = 0; scanned < PROCESS_MAX; scanned++) {
        uint32_t index = (current_index + 1 + scanned) % PROCESS_MAX;
        process_t *process = process_at(index);
        if (!process || process->state != PROCESS_READY)
            continue;
        current_index = index;
        process_set_current(process);
        process->state = PROCESS_RUNNING;
        process->runs++;
        process->ticks_run = timer_get_ticks();
        tss_set_stack(0x10, (uint32_t)process->stack + process->stack_size);
        context_switch(&scheduler_esp, process->esp, process->cr3);
        if (process->state == PROCESS_RUNNING)
            process->state = PROCESS_READY;
        process_set_current(0);
        return;
    }
}

void scheduler_yield(void) {
    process_t *process = process_current();
    if (!process)
        return;
    if (process->state == PROCESS_RUNNING)
        process->state = PROCESS_READY;
    switch_back_to_scheduler(process);
}

void scheduler_sleep_current(uint32_t ticks) {
    process_t *process = process_current();
    if (!process)
        return;
    if (ticks == 0) {
        scheduler_yield();
        return;
    }
    process_sleep(process, timer_get_ticks(), ticks);
    switch_back_to_scheduler(process);
}

/* Switches the current process away from the CPU without touching its
   state — the caller must have already set it to PROCESS_BLOCKED (or
   whatever state should keep scheduler_run_once() from picking it back
   up) before calling this, so nothing can incorrectly mark it READY as
   part of a plain yield. Whoever owns the event this process is
   waiting on (an IRQ handler, another process releasing a lock, ...)
   is responsible for flipping the state back to PROCESS_READY. */
void scheduler_block_current(void) {
    process_t *process = process_current();
    if (!process)
        return;
    switch_back_to_scheduler(process);
}

void scheduler_dump(void) {
    console_set_color(CONSOLE_CYAN, CONSOLE_BLACK);
    console_puts("[SCHED] ");
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    console_puts("round-robin cooperative context switching\n");
    process_dump();
}
