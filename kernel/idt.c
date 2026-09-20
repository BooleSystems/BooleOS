// nullos/kernel/idt.c
#include "idt.h"
#include "hal.h"
#include "messages.h"
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
extern void irq14(void);
extern void irq15(void);

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

static const msg_id_t exception_msgs[32] = {
    MSG_EXC_DE, MSG_EXC_DB,
    MSG_EXC_NMI, MSG_EXC_BP,
    MSG_EXC_OF, MSG_EXC_BR,
    MSG_EXC_UD, MSG_EXC_NM,
    MSG_EXC_DF, MSG_EXC_COPROC_OVERRUN,
    MSG_EXC_TS, MSG_EXC_NP,
    MSG_EXC_SS, MSG_EXC_GP,
    MSG_EXC_PF, MSG_EXC_RESERVED,
    MSG_EXC_MF, MSG_EXC_AC,
    MSG_EXC_MC, MSG_EXC_XM,
    MSG_EXC_VE, MSG_EXC_RESERVED,
    MSG_EXC_RESERVED, MSG_EXC_RESERVED,
    MSG_EXC_RESERVED, MSG_EXC_RESERVED,
    MSG_EXC_RESERVED, MSG_EXC_RESERVED,
    MSG_EXC_RESERVED, MSG_EXC_RESERVED,
    MSG_EXC_RESERVED, MSG_EXC_RESERVED,
};

void exception_handler(uint32_t int_no, uint32_t err_code, uint32_t eip) {
    uint32_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    console_set_color(CONSOLE_WHITE, CONSOLE_RED);
    console_puts(msg(MSG_IDT_KERNEL_EXCEPTION));

    console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
    console_puts(msg(MSG_IDT_EXCEPTION));
    if (int_no < 32)
        console_puts(msg(exception_msgs[int_no]));
    else
        console_puts(msg(MSG_IDT_UNKNOWN));
    console_puts("\n");

    console_puts(msg(MSG_IDT_EIP)); console_put_hex(eip);      console_puts("\n");
    console_puts(msg(MSG_IDT_ERR_CODE)); console_put_hex(err_code); console_puts("\n");

    if (int_no == 14) {
        console_puts(msg(MSG_IDT_CR2_ADDR)); console_put_hex(cr2); console_puts("\n");
        console_puts(msg(MSG_IDT_PF_FLAGS));
        console_puts((err_code & 1) ? msg(MSG_IDT_PROTECTION) : msg(MSG_IDT_NOT_PRESENT));
        console_puts((err_code & 2) ? msg(MSG_IDT_WRITE)      : msg(MSG_IDT_READ));
        console_puts((err_code & 4) ? msg(MSG_IDT_USER)        : msg(MSG_IDT_KERNEL));
        console_puts("\n");
    }

    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
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

    console_puts(msg(MSG_IDT_1_ZEROING_IDT_AT_0X200000));
    for (i = 0; i < IDT_SIZE; i++) {
        idt[i].base_low  = 0;
        idt[i].selector  = 0;
        idt[i].zero      = 0;
        idt[i].flags     = 0;
        idt[i].base_high = 0;
        handlers[i]      = 0;
    }

    console_puts(msg(MSG_IDT_2_EXCEPTION_GATES_0_31));
    for (i = 0; i < 32; i++)
        idt_set_gate((uint8_t)i, isr_table[i], 0x08, 0x8E);

    console_puts(msg(MSG_IDT_3_IRQ_GATES_32_33));
    idt_set_gate(32, irq0, 0x08, 0x8E);
    idt_set_gate(33, irq1, 0x08, 0x8E);
    idt_set_gate(46, irq14, 0x08, 0x8E);   /* ATA primary channel */
    idt_set_gate(47, irq15, 0x08, 0x8E);   /* ATA secondary channel */

    /* 0xEF = present | DPL=3 | 32-bit trap gate — preserves IF (no implicit cli) */
    console_puts(msg(MSG_IDT_4_SYSCALL_GATE_0X80_DPL));
    idt_set_gate(128, isr128, 0x08, 0xEF);

    console_puts(msg(MSG_IDT_5_FLUSH));
    idtp.limit = (uint16_t)(sizeof(idt_entry_t) * IDT_SIZE - 1);
    idtp.base  = IDT_ADDRESS;
    idt_flush((uint32_t)&idtp);

    console_puts(msg(MSG_IDT_6_OK));
}
