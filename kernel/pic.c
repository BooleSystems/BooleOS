// nullos/kernel/pic.c
// Remapeia o 8259 PIC para que IRQs não conflitem com exceções da CPU

#include "pic.h"
#include <stdint.h>

// Portas do PIC mestre e escravo
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

// Comandos de inicialização
#define PIC_EOI     0x20    // End of Interrupt
#define ICW1_ICW4   0x01    // ICW4 vai ser enviado
#define ICW1_INIT   0x10    // Inicializar
#define ICW4_8086   0x01    // Modo 8086/88

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Pequeno delay para PICs antigos que precisam de tempo
static inline void io_wait(void) {
    outb(0x80, 0);
}

void pic_init(void) {
    // Salva máscaras atuais
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    // Inicia sequência de inicialização (ICW1)
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); io_wait();

    // ICW2: offset dos vetores
    // PIC mestre: IRQ0-7 -> INT 32-39
    // PIC escravo: IRQ8-15 -> INT 40-47
    outb(PIC1_DATA, 0x20); io_wait();  // Offset mestre = 32
    outb(PIC2_DATA, 0x28); io_wait();  // Offset escravo = 40

    // ICW3: cascade
    outb(PIC1_DATA, 0x04); io_wait();  // Mestre: escravo em IRQ2 (bit 2)
    outb(PIC2_DATA, 0x02); io_wait();  // Escravo: identidade = 2

    // ICW4: modo 8086
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    // Restaura máscaras
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  val;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) | (uint8_t)(1 << irq);
    outb(port, val);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  val;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) & (uint8_t)~(1 << irq);
    outb(port, val);
}
