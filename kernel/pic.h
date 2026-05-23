// nullos/kernel/pic.h
// Programmable Interrupt Controller (8259 PIC)
// Remapeia as IRQs do hardware para não conflitar com exceções da CPU

#ifndef PIC_H
#define PIC_H

#include <stdint.h>

// Inicializa e remapeia o PIC
// IRQ0-7  -> INT 32-39
// IRQ8-15 -> INT 40-47
void pic_init(void);

// Mascara (desabilita) uma IRQ específica
void pic_mask_irq(uint8_t irq);

// Desmascara (habilita) uma IRQ específica
void pic_unmask_irq(uint8_t irq);

#endif // PIC_H
