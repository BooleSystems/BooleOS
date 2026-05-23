// nullos/kernel/keyboard.c
#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "drivers/vga.h"
#include <stdint.h>

#define KB_DATA_PORT 0x60

static const char scancode_map[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',
    '\b', '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']',
    '\n', 0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*',
    0,    ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,
};

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define KB_BUFFER_SIZE 256
static char    kb_buffer[KB_BUFFER_SIZE];
static uint8_t kb_head = 0;
static uint8_t kb_tail = 0;

static void keyboard_callback(uint32_t int_no) {
    (void)int_no;
    uint8_t scancode = inb(KB_DATA_PORT);
    if (scancode & 0x80) return;
    char c = scancode_map[scancode & 0x7F];
    if (c == 0) return;
    uint8_t next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_tail) {
        kb_buffer[kb_head] = c;
        kb_head = next;
    }
    vga_putchar(c);
}

void keyboard_init(void) {
    idt_register_handler(33, keyboard_callback);
    pic_unmask_irq(1);
}

char keyboard_getchar(void) {
    while (kb_head == kb_tail)
        __asm__ volatile ("hlt");
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

int keyboard_haschar(void) {
    return kb_head != kb_tail;
}
