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

#define IDT_ADDRESS 0x200000
#define IDT_SIZE    256

static idt_ptr_t     idtp;
static isr_handler_t handlers[IDT_SIZE];

/* IRQ stubs (hardware interrupts) */
extern void irq0(void);
extern void irq1(void);

/* Syscall gate */
extern void isr128(void);

/* CPU exception stubs */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void idt_flush(uint32_t);

static const char *exception_names[32] = {
    "#DE Divide Error",           "#DB Debug",
    "NMI",                        "#BP Breakpoint",
    "#OF Overflow",               "#BR Bound Range",
    "#UD Invalid Opcode",         "#NM No FPU",
    "#DF Double Fault",           "Coprocessor Overrun",
    "#TS Invalid TSS",            "#NP Segment Not Present",
    "#SS Stack Fault",            "#GP General Protection",
    "#PF Page Fault",             "Reserved",
    "#MF x87 Float",              "#AC Alignment Check",
    "#MC Machine Check",          "#XM SIMD Float",
    "#VE Virtualization",         "Reserved",
    "Reserved",                   "Reserved",
    "Reserved",                   "Reserved",
    "Reserved",                   "Reserved",
    "Reserved",                   "Reserved",
    "Reserved",                   "Reserved",
};

void exception_handler(uint32_t int_no, uint32_t err_code, uint32_t eip) {
    uint32_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    vga_set_color(VGA_WHITE, VGA_RED);
    vga_puts("\n\n*** KERNEL EXCEPTION ***\n");

    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_puts("  Exception : ");
    if (int_no < 32)
        vga_puts(exception_names[int_no]);
    else
        vga_puts("Unknown");
    vga_puts("\n");

    vga_puts("  EIP       : "); vga_puthex(eip);      vga_puts("\n");
    vga_puts("  Err Code  : "); vga_puthex(err_code); vga_puts("\n");

    if (int_no == 14) {
        vga_puts("  CR2 (addr): "); vga_puthex(cr2); vga_puts("\n");
        vga_puts("  PF flags  : ");
        vga_puts((err_code & 1) ? "protection " : "not-present ");
        vga_puts((err_code & 2) ? "write "      : "read ");
        vga_puts((err_code & 4) ? "user"        : "kernel");
        vga_puts("\n");
    }

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
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
    handlers[irq] = handler;
}

static void idt_set_gate(uint8_t num, void (*base)(void), uint16_t sel, uint8_t flags) {
    idt_entry_t *idt = (idt_entry_t *)IDT_ADDRESS;
    uint32_t addr = (uint32_t)base;
    idt[num].base_low  = (uint16_t)(addr & 0xFFFF);
    idt[num].base_high = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

/* Lookup table to keep idt_init concise */
static void (*isr_table[32])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

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

    vga_puts(" [2] gates de excecao (0-31)\n");
    for (i = 0; i < 32; i++)
        idt_set_gate((uint8_t)i, isr_table[i], 0x08, 0x8E);

    vga_puts(" [3] gates de IRQ (32-33)\n");
    idt_set_gate(32, irq0, 0x08, 0x8E);
    idt_set_gate(33, irq1, 0x08, 0x8E);

    /* 0xEF = presente | DPL=3 | trap gate de 32 bits — preserva IF (sem cli implícito) */
    vga_puts(" [4] gate syscall (0x80, DPL=3)\n");
    idt_set_gate(128, isr128, 0x08, 0xEF);

    vga_puts(" [5] flush\n");
    idtp.limit = (uint16_t)(sizeof(idt_entry_t) * IDT_SIZE - 1);
    idtp.base  = IDT_ADDRESS;
    idt_flush((uint32_t)&idtp);

    vga_puts(" [6] ok!\n");
}
