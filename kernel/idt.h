// nullos/kernel/idt.h
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

typedef void (*isr_handler_t)(uint32_t int_no);

void idt_init(void);
void idt_register_handler(uint8_t irq, isr_handler_t handler);

#endif
