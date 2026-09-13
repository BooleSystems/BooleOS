// nullos/kernel/pic.h
// Programmable Interrupt Controller (8259 PIC)
// Remaps hardware IRQs so they don't conflict with CPU exceptions

#ifndef PIC_H
#define PIC_H

#include <stdint.h>

// Initializes and remaps the PIC
// IRQ0-7  -> INT 32-39
// IRQ8-15 -> INT 40-47
void pic_init(void);

// Masks (disables) a specific IRQ
void pic_mask_irq(uint8_t irq);

// Unmasks (enables) a specific IRQ
void pic_unmask_irq(uint8_t irq);

#endif // PIC_H
