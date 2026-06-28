// nullos/kernel/timer.c
#include "timer.h"
#include "idt.h"
#include "pic.h"
#include "scheduler.h"
#include "process.h"
#include <stdint.h>

#define PIT_CMD     0x43
#define PIT_CH0     0x40
#define PIT_BASE_HZ    1193182
#define PREEMPT_TICKS  10       /* fatia de tempo: 10 ticks = 100ms a 100Hz */

static volatile uint32_t ticks = 0;
static uint32_t tick_freq = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void timer_callback(uint32_t int_no) {
    (void)int_no;
    ticks++;
    scheduler_tick(ticks);

    /* Preempção: força yield se o processo atual esgotou sua fatia de tempo.
       O irq0 já salvou o contexto completo (pusha + frame CPU via TSS),
       então context_switch aqui é seguro — o iret do irq0 vai restaurar
       o processo corretamente quando for re-agendado. */
    process_t *p = process_current();
    if (p && p->state == PROCESS_RUNNING &&
        (ticks - p->ticks_run) >= PREEMPT_TICKS) {
        scheduler_yield();
    }
}

void timer_init(uint32_t freq_hz) {
    tick_freq = freq_hz;
    uint32_t divisor = PIT_BASE_HZ / freq_hz;
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    idt_register_handler(32, timer_callback);
    pic_unmask_irq(0);
}

uint32_t timer_get_ticks(void) {
    return ticks;
}

void timer_sleep(uint32_t ms) {
    uint32_t end = ticks + (ms * tick_freq / 1000);
    while (ticks < end) {
        __asm__ volatile ("hlt");
    }
}
