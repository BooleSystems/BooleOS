// COM1 serial driver — mirrors kernel output for QEMU -serial stdio
#include "serial.h"
#include <stdint.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);  // disable interrupts
    outb(COM1 + 3, 0x80);  // DLAB on
    outb(COM1 + 0, 0x03);  // 38400 baud (divisor lo)
    outb(COM1 + 1, 0x00);  // divisor hi
    outb(COM1 + 3, 0x03);  // 8N1, DLAB off
    outb(COM1 + 2, 0xC7);  // enable FIFO, 14-byte threshold
    outb(COM1 + 4, 0x0B);  // RTS + DTR
}

void serial_putchar(char c) {
    while (!(inb(COM1 + 5) & 0x20));  // wait for transmit hold empty
    if (c == '\n') {
        outb(COM1, '\r');
        while (!(inb(COM1 + 5) & 0x20));
    }
    outb(COM1, (uint8_t)c);
}
