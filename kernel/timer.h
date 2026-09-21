// nullos/kernel/timer.h
#ifndef TIMER_H
#define TIMER_H
#include <stdint.h>
void timer_init(uint32_t freq_hz);
uint32_t timer_get_ticks(void);
void timer_sleep(uint32_t ms);

/* Busy-waits about `ms` milliseconds by reading the PIT's counter DIRECTLY
   (counting its wrap-arounds), with no IRQ and no tick counter — it works with
   interrupts off, e.g. inside an exception handler. Rounded up to whole timer
   periods; bounded, so a dead PIT cannot hang it for ever. */
void timer_poll_delay_ms(uint32_t ms);
#endif
