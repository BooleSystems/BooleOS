// booleos/kernel/irq.h
// Short interrupt-off critical sections (single core, no SMP).

#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

/* Saves EFLAGS and disables interrupts; irq_restore() puts the saved EFLAGS
   back. Nestable, unlike a bare cli/sti: a section entered with interrupts
   already off (an exception handler, an outer section) leaves them off. */
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile ("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags) {
    __asm__ volatile ("push %0; popf" : : "r"(flags) : "memory", "cc");
}

#endif // IRQ_H
