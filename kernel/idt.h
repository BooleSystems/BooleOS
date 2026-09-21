// nullos/kernel/idt.h
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

typedef void (*isr_handler_t)(uint32_t int_no);

void idt_init(void);
void idt_register_handler(uint8_t irq, isr_handler_t handler);

/* Name of CPU exception `int_no` ("#PF Page Fault"), or "Unknown". A static
   table lookup: safe to call from the crash path and from Safe Mode. */
const char *exception_name(uint32_t int_no);

#endif
