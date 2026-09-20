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

/* Same layout as scancode_map, index-for-index, but with Shift held —
   US QWERTY. Added because there was previously NO Shift handling at
   all here (not a wrong entry in an existing shifted table — there
   was no shifted table, and no Shift press/release tracking either),
   which is why Shift+5 never produced '%' and Shift+\ (scancode
   0x2B) never produced '|': every character always came from the
   single unshifted table above, regardless of Shift. Keys with no
   shifted variant (Enter, Backspace, Tab, Esc, space, the keypad '*')
   keep the same character as the unshifted table. */
static const char scancode_map_shift[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_',  '+',
    '\b', '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{',  '}',
    '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"',  '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,    '*',
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

/* raw scancode buffer: bit9=shift, bit8=ctrl, bits0-7=scancode */
static uint16_t kb_raw_buf[KB_BUFFER_SIZE];
static uint8_t  kb_raw_head = 0;
static uint8_t  kb_raw_tail = 0;

void keyboard_flush(void) {
    kb_head = kb_tail = 0;
    kb_raw_head = kb_raw_tail = 0;
}

static int ctrl_pressed  = 0;
static int shift_pressed = 0;

static void keyboard_callback(uint32_t int_no) {
    (void)int_no;
    uint8_t scancode = inb(KB_DATA_PORT);

    /* tracks left Ctrl (0x1D press, 0x9D release) */
    if (scancode == 0x1D) { ctrl_pressed = 1; return; }
    if (scancode == 0x9D) { ctrl_pressed = 0; return; }

    /* tracks left/right Shift (0x2A/0x36 press, 0xAA/0xB6 release) */
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; return; }

    if (scancode & 0x80) return;  /* other key-releases */

    char c;
    if (ctrl_pressed && scancode == 0x2E) {
        c = 0x03;  /* Ctrl+C */
    } else {
        c = shift_pressed ? scancode_map_shift[scancode & 0x7F]
                           : scancode_map[scancode & 0x7F];
        if (c == 0) c = 0;  /* unmapped key: only goes to raw */
    }

    /* raw: always pushed (keys with and without an ASCII mapping) */
    uint16_t raw = (uint16_t)(scancode | (ctrl_pressed ? 0x100 : 0)
                                       | (shift_pressed ? 0x200 : 0));
    uint8_t rnext = (kb_raw_head + 1) % KB_BUFFER_SIZE;
    if (rnext != kb_raw_tail) {
        kb_raw_buf[kb_raw_head] = raw;
        kb_raw_head = rnext;
    }

    /* ASCII: only pushed if it has a mapping */
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

/* Returns the next char from the buffer or -1 if empty (non-blocking). */
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

/* returns raw scancode (bit8=ctrl, bit9=shift) or -1 if the buffer is empty */
int keyboard_raw_nowait(void) {
    if (kb_raw_head == kb_raw_tail) return -1;
    uint16_t raw = kb_raw_buf[kb_raw_tail];
    kb_raw_tail = (kb_raw_tail + 1) % KB_BUFFER_SIZE;
    return (int)(unsigned int)raw;
}
