// booleos/kernel/timer.c
#include "timer.h"
#include "idt.h"
#include "pic.h"
#include "scheduler.h"
#include "process.h"
#include <stdint.h>

#define PIT_CMD     0x43
#define PIT_CH0     0x40
#define PIT_BASE_HZ    1193182
#define PREEMPT_TICKS  10       /* time slice: 10 ticks = 100ms at 100Hz */

static volatile uint32_t ticks = 0;
static uint32_t tick_freq = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void timer_callback(uint32_t int_no) {
    (void)int_no;
    ticks++;
    scheduler_tick(ticks);

    /* Preemption: forces a yield if the current process has used up its
       time slice. irq0 has already saved the full context (pusha + CPU
       frame via TSS), so context_switch here is safe — irq0's iret will
       restore the process correctly whenever it gets rescheduled. */
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

static inline uint8_t pit_inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* Latches and reads channel 0's current count. */
static uint32_t pit_read_count(void) {
    outb(PIT_CMD, 0x00);
    uint32_t lo = pit_inb(PIT_CH0);
    uint32_t hi = pit_inb(PIT_CH0);
    return lo | (hi << 8);
}

void timer_poll_delay_ms(uint32_t ms) {
    uint32_t period_ms = tick_freq ? 1000u / tick_freq : 10u;   /* 10 ms at 100 Hz */
    if (period_ms == 0) period_ms = 1;
    /* timer_init() programs channel 0 in mode 3 (square wave, command 0x36).
       In mode 3 the counter runs down from the reload value TWICE per
       period (once per output half-cycle, decrementing by 2), so one period
       is two wraps. Counting one wrap per period made every delay half as
       long as asked (found in 24-B: arp_resolve()'s ~500 ms retry fired
       after ~250 ms). */
    uint32_t wraps = 2u * ((ms + period_ms - 1) / period_ms);
    uint32_t guard = wraps * 30000u + 100000u;                  /* reads per half period, generously */
    uint32_t prev = pit_read_count();

    /* The counter counts DOWN and reloads at the end of each half period: a
       value larger than the previous one is one wrap. */
    while (wraps && guard--) {
        uint32_t cur = pit_read_count();
        if (cur > prev) wraps--;
        prev = cur;
    }
}
