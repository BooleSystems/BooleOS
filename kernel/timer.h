// nullos/kernel/timer.h
#ifndef TIMER_H
#define TIMER_H
#include <stdint.h>
void timer_init(uint32_t freq_hz);
uint32_t timer_get_ticks(void);
void timer_sleep(uint32_t ms);
#endif
