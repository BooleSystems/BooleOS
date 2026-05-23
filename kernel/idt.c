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
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

// Coloca a IDT num endereço fixo seguro (2MB)
#define IDT_ADDRESS 0x200000
#define IDT_SIZE    48

static idt_ptr_t     idtp;
static isr_handler_t handlers[IDT_SIZE];

extern void irq0(void);
extern void irq1(void);
extern void idt_flush(uint32_t);

static void idt_set_gate(uint8_t num, void (*base)(void), uint16_t sel, uint8_t flags) {
    idt_entry_t *idt = (idt_entry_t *)IDT_ADDRESS;
    uint32_t addr = (uint32_t)base;
    idt[num].base_low  = (uint16_t)(addr & 0xFFFF);
    idt[num].base_high = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

void isr_handler(uint32_t int_no) {
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_puts("\n*** EXCEPTION ");
    vga_putdec(int_no);
    vga_puts(" ***\n");
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

void irq_handler(uint32_t int_no) {
    if (int_no >= 40)
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0xA0));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
    if (int_no < IDT_SIZE && handlers[int_no])
        handlers[int_no](int_no);
}

void idt_register_handler(uint8_t irq, isr_handler_t handler) {
    if (irq < IDT_SIZE)
        handlers[irq] = handler;
}

void idt_init(void) {
    int i;
    idt_entry_t *idt = (idt_entry_t *)IDT_ADDRESS;

    vga_puts(" [1] zerando IDT em 0x200000\n");
    for (i = 0; i < IDT_SIZE; i++) {
        idt[i].base_low  = 0;
        idt[i].selector  = 0;
        idt[i].zero      = 0;
        idt[i].flags     = 0;
        idt[i].base_high = 0;
        handlers[i]      = 0;
    }

    vga_puts(" [2] loop ok\n");
    idtp.limit = (uint16_t)(sizeof(idt_entry_t) * IDT_SIZE - 1);
    idtp.base  = IDT_ADDRESS;

    idt_set_gate(32, irq0, 0x08, 0x8E);
    idt_set_gate(33, irq1, 0x08, 0x8E);

    vga_puts(" [3] gates ok, chamando flush\n");
    idt_flush((uint32_t)&idtp);

    vga_puts(" [4] flush ok!\n");
}
