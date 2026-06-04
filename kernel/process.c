// nullos/kernel/process.c
#include "process.h"
#include "drivers/vga.h"
#include <stdint.h>

static process_t process_table[PROCESS_MAX];
static process_t *current_process = 0;
static uint32_t next_pid = 1;

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
        process_table[i].wake_tick = 0;
        process_table[i].ticks_run = 0;
        process_table[i].runs = 0;
    }

    current_process = 0;
    next_pid = 1;
}

process_t *process_spawn(const char *name, process_entry_t entry, void *arg) {
    if (!entry)
        return 0;

    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_t *process = &process_table[i];
        if (process->state == PROCESS_UNUSED) {
            process->pid = next_pid++;
            copy_name(process->name, name);
            process->state = PROCESS_READY;
            process->entry = entry;
            process->arg = arg;
            process->wake_tick = 0;
            process->ticks_run = 0;
            process->runs = 0;
            return process;
        }
    }

    return 0;
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
        case PROCESS_ZOMBIE:   return "zombie";
        default:               return "?";
    }
}

void process_dump(void) {
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("[PROC] ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("PID  STATE     RUNS  NAME\n");

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
        vga_puts(process->name);
        vga_puts("\n");
    }
}
