// nullos/kernel/idt.c
#include "idt.h"
#include "drivers/vga.h"
#include <stdint.h>

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed)) __attribute__((aligned(8))) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t   idtp;
static isr_handler_t handlers[256];

// ISR stubs
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static void idt_set_gate(uint8_t num, void (*base)(void), uint16_t sel, uint8_t flags) {
    uint32_t addr = (uint32_t)base;
    idt[num].base_low  = (uint16_t)(addr & 0xFFFF);
    idt[num].base_high = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags | 0x60;
}

void isr_handler(void) {
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_puts("\n*** EXCEPTION ***\n");
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

void irq_handler(void) {
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
}

void idt_register_handler(uint8_t irq, isr_handler_t handler) {
    handlers[irq] = handler;
}

void idt_init(void) {
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint32_t)&idt[0];

    for (int i = 0; i < 256; i++) {
        idt[i].base_low = 0; idt[i].selector = 0;
        idt[i].zero = 0; idt[i].flags = 0; idt[i].base_high = 0;
        handlers[i] = 0;
    }

    idt_set_gate(0,  isr0,  0x08, 0x8E);
    idt_set_gate(1,  isr1,  0x08, 0x8E);
    idt_set_gate(2,  isr2,  0x08, 0x8E);
    idt_set_gate(3,  isr3,  0x08, 0x8E);
    idt_set_gate(4,  isr4,  0x08, 0x8E);
    idt_set_gate(5,  isr5,  0x08, 0x8E);
    idt_set_gate(6,  isr6,  0x08, 0x8E);
    idt_set_gate(7,  isr7,  0x08, 0x8E);
    idt_set_gate(8,  isr8,  0x08, 0x8E);
    idt_set_gate(9,  isr9,  0x08, 0x8E);
    idt_set_gate(10, isr10, 0x08, 0x8E);
    idt_set_gate(11, isr11, 0x08, 0x8E);
    idt_set_gate(12, isr12, 0x08, 0x8E);
    idt_set_gate(13, isr13, 0x08, 0x8E);
    idt_set_gate(14, isr14, 0x08, 0x8E);
    idt_set_gate(15, isr15, 0x08, 0x8E);
    idt_set_gate(16, isr16, 0x08, 0x8E);
    idt_set_gate(17, isr17, 0x08, 0x8E);
    idt_set_gate(18, isr18, 0x08, 0x8E);
    idt_set_gate(19, isr19, 0x08, 0x8E);
    idt_set_gate(20, isr20, 0x08, 0x8E);
    idt_set_gate(21, isr21, 0x08, 0x8E);
    idt_set_gate(22, isr22, 0x08, 0x8E);
    idt_set_gate(23, isr23, 0x08, 0x8E);
    idt_set_gate(24, isr24, 0x08, 0x8E);
    idt_set_gate(25, isr25, 0x08, 0x8E);
    idt_set_gate(26, isr26, 0x08, 0x8E);
    idt_set_gate(27, isr27, 0x08, 0x8E);
    idt_set_gate(28, isr28, 0x08, 0x8E);
    idt_set_gate(29, isr29, 0x08, 0x8E);
    idt_set_gate(30, isr30, 0x08, 0x8E);
    idt_set_gate(31, isr31, 0x08, 0x8E);
    idt_set_gate(32, irq0,  0x08, 0x8E);
    idt_set_gate(33, irq1,  0x08, 0x8E);
    idt_set_gate(34, irq2,  0x08, 0x8E);
    idt_set_gate(35, irq3,  0x08, 0x8E);
    idt_set_gate(36, irq4,  0x08, 0x8E);
    idt_set_gate(37, irq5,  0x08, 0x8E);
    idt_set_gate(38, irq6,  0x08, 0x8E);
    idt_set_gate(39, irq7,  0x08, 0x8E);
    idt_set_gate(40, irq8,  0x08, 0x8E);
    idt_set_gate(41, irq9,  0x08, 0x8E);
    idt_set_gate(42, irq10, 0x08, 0x8E);
    idt_set_gate(43, irq11, 0x08, 0x8E);
    idt_set_gate(44, irq12, 0x08, 0x8E);
    idt_set_gate(45, irq13, 0x08, 0x8E);
    idt_set_gate(46, irq14, 0x08, 0x8E);
    idt_set_gate(47, irq15, 0x08, 0x8E);

__asm__ volatile ("lidt (%0)" : : "r"(&idtp));
}
