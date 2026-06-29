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

/* buffer de scancodes brutos: bit8=ctrl, bits0-7=scancode */
static uint16_t kb_raw_buf[KB_BUFFER_SIZE];
static uint8_t  kb_raw_head = 0;
static uint8_t  kb_raw_tail = 0;

void keyboard_flush(void) {
    kb_head = kb_tail = 0;
    kb_raw_head = kb_raw_tail = 0;
}

static int ctrl_pressed = 0;

static void keyboard_callback(uint32_t int_no) {
    (void)int_no;
    uint8_t scancode = inb(KB_DATA_PORT);

    /* rastreia Ctrl esquerdo (0x1D press, 0x9D release) */
    if (scancode == 0x1D) { ctrl_pressed = 1; return; }
    if (scancode == 0x9D) { ctrl_pressed = 0; return; }

    if (scancode & 0x80) return;  /* outros key-releases */

    char c;
    if (ctrl_pressed && scancode == 0x2E) {
        c = 0x03;  /* Ctrl+C */
    } else {
        c = scancode_map[scancode & 0x7F];
        if (c == 0) c = 0;  /* tecla sem mapeamento: só vai pro raw */
    }

    /* raw: sempre empurra (teclas com e sem mapeamento ASCII) */
    uint16_t raw = (uint16_t)(scancode | (ctrl_pressed ? 0x100 : 0));
    uint8_t rnext = (kb_raw_head + 1) % KB_BUFFER_SIZE;
    if (rnext != kb_raw_tail) {
        kb_raw_buf[kb_raw_head] = raw;
        kb_raw_head = rnext;
    }

    /* ASCII: só empurra se tem mapeamento */
    if (c != 0) {
        uint8_t next = (kb_head + 1) % KB_BUFFER_SIZE;
        if (next != kb_tail) {
            kb_buffer[kb_head] = c;
            kb_head = next;
        }
    }
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

/* Retorna o próximo char do buffer ou -1 se vazio (não bloqueia). */
int keyboard_getchar_nowait(void) {
    if (kb_head == kb_tail)
        return -1;
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return (int)(unsigned char)c;
}

int keyboard_haschar(void) {
    return kb_head != kb_tail;
}

/* retorna scancode bruto (bit8=ctrl) ou -1 se buffer vazio */
int keyboard_raw_nowait(void) {
    if (kb_raw_head == kb_raw_tail) return -1;
    uint16_t raw = kb_raw_buf[kb_raw_tail];
    kb_raw_tail = (kb_raw_tail + 1) % KB_BUFFER_SIZE;
    return (int)(unsigned int)raw;
}
