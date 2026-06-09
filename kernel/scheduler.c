// nullos/kernel/scheduler.c
#include "scheduler.h"
#include "timer.h"
#include "drivers/vga.h"
#include <stdint.h>

extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

static uint32_t scheduler_ready = 0;
static uint32_t current_index = PROCESS_MAX - 1;
static uint32_t scheduler_esp = 0;

static void scheduler_task_bootstrap(void) {
    process_t *process = process_current();

    if (process && process->entry)
        process->entry(process->arg);

    process_exit(process);
    scheduler_yield();

    for (;;)
        __asm__ volatile ("hlt");
}

static void switch_back_to_scheduler(process_t *process) {
    if (!process)
        return;
    context_switch(&process->esp, scheduler_esp);
}

void scheduler_init(void) {
    process_init();
    scheduler_ready = 1;
    current_index = PROCESS_MAX - 1;
    scheduler_esp = 0;
}

process_t *scheduler_spawn(const char *name, process_entry_t entry, void *arg) {
    if (!scheduler_ready)
        return 0;
    return process_spawn(name, entry, arg, scheduler_task_bootstrap);
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

        context_switch(&scheduler_esp, process->esp);

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

void scheduler_dump(void) {
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("[SCHED] ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("round-robin cooperative context switching\n");
    process_dump();
}
