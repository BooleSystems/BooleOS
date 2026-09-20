// nullos/kernel/drivers/vga.c
// VGA text mode 80x25 driver
// The VGA buffer lives at 0xB8000. Each cell = 2 bytes: [attribute | char]

#include "vga.h"
#include "../serial.h"
#include <stdint.h>
#include <stddef.h>

// VGA text mode framebuffer address
#define VGA_BUFFER ((volatile uint16_t *)0xB8000)

// VGA controller I/O ports (to move the hardware cursor)
#define VGA_CTRL_PORT   0x3D4
#define VGA_DATA_PORT   0x3D5
#define VGA_CURSOR_HIGH 0x0E
#define VGA_CURSOR_LOW  0x0F

// Internal terminal state
static uint8_t  term_col   = 0;
static uint8_t  term_row   = 0;
static uint8_t  term_color = 0;

// ============================================================
// Internal functions
// ============================================================

// Writes to an I/O port (needed to move the cursor)
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Builds the VGA attribute byte: [bg(4) | fg(4)]
static inline uint8_t vga_make_attr(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)((bg << 4) | (fg & 0x0F));
}

// Builds a VGA entry: [attribute(8) | char(8)]
static inline uint16_t vga_make_entry(char c, uint8_t attr) {
    return (uint16_t)((uint16_t)attr << 8) | (uint8_t)c;
}

// Updates the hardware cursor to the current position
static void vga_update_cursor(void) {
    uint16_t pos = (uint16_t)(term_row * VGA_COLS + term_col);
    outb(VGA_CTRL_PORT, VGA_CURSOR_HIGH);
    outb(VGA_DATA_PORT, (uint8_t)(pos >> 8));
    outb(VGA_CTRL_PORT, VGA_CURSOR_LOW);
    outb(VGA_DATA_PORT, (uint8_t)(pos & 0xFF));
}

// Scrolls the terminal up by one line
static void vga_scroll(void) {
    // Move every line up by one
    for (int row = 1; row < VGA_ROWS; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            VGA_BUFFER[(row - 1) * VGA_COLS + col] =
                VGA_BUFFER[row * VGA_COLS + col];
        }
    }
    // Clear the last line
    uint16_t blank = vga_make_entry(' ', term_color);
    for (int col = 0; col < VGA_COLS; col++) {
        VGA_BUFFER[(VGA_ROWS - 1) * VGA_COLS + col] = blank;
    }
    term_row = VGA_ROWS - 1;
}

// ============================================================
// Public API
// ============================================================

void vga_init(void) {
    term_color = vga_make_attr(VGA_LIGHT_GREY, VGA_BLACK);
    term_col   = 0;
    term_row   = 0;
    vga_clear();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    term_color = vga_make_attr(fg, bg);
}

void vga_clear(void) {
    uint16_t blank = vga_make_entry(' ', term_color);
    for (int i = 0; i < VGA_ROWS * VGA_COLS; i++) {
        VGA_BUFFER[i] = blank;
    }
    term_col = 0;
    term_row = 0;
    vga_update_cursor();
}

void vga_set_cursor(uint8_t col, uint8_t row) {
    if (col < VGA_COLS && row < VGA_ROWS) {
        term_col = col;
        term_row = row;
        vga_update_cursor();
    }
}

void vga_putchar(char c) {
    int erased = 0;   /* a '\b' that really blanked a cell (see the serial mirror below) */
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\r') {
        term_col = 0;
    } else if (c == '\t') {
        // Tab = next column that's a multiple of 4
        term_col = (uint8_t)((term_col + 4) & ~3);
        if (term_col >= VGA_COLS) {
            term_col = 0;
            term_row++;
        }
    } else if (c == '\b') {
        if (term_col > 0) {
            term_col--;
            VGA_BUFFER[term_row * VGA_COLS + term_col] =
                vga_make_entry(' ', term_color);
            erased = 1;
        }
    } else {
        VGA_BUFFER[term_row * VGA_COLS + term_col] =
            vga_make_entry(c, term_color);
        term_col++;
        if (term_col >= VGA_COLS) {
            term_col = 0;
            term_row++;
        }
    }

    // Scroll if we went past the last line
    if (term_row >= VGA_ROWS) {
        vga_scroll();
    }

    /* Mirror to the serial console. A raw '\b' only moves a terminal's cursor
       left without erasing, so the next characters typed over a longer,
       already-echoed word leave its tail behind ("shutdown" retyped as
       "reboot" showed as "rebootdows"). VGA blanks the cell itself; the
       serial side needs the classic "\b \b" to do the same. A '\b' that
       erased nothing (column 0) sends nothing, matching what VGA did. */
    if (c == '\b') {
        if (erased) {
            serial_putchar('\b');
            serial_putchar(' ');
            serial_putchar('\b');
        }
    } else {
        serial_putchar(c);
    }
    vga_update_cursor();
}

void vga_puts(const char *str) {
    if (!str) return;
    while (*str) {
        vga_putchar(*str++);
    }
}

void vga_puthex(uint32_t value) {
    const char *hex = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        vga_putchar(hex[(value >> i) & 0xF]);
    }
}

void vga_putdec(uint32_t value) {
    if (value == 0) {
        vga_putchar('0');
        return;
    }
    char buf[12];
    int  idx = 0;
    while (value > 0) {
        buf[idx++] = '0' + (value % 10);
        value /= 10;
    }
    // Print in reverse
    for (int i = idx - 1; i >= 0; i--) {
        vga_putchar(buf[i]);
    }
}
