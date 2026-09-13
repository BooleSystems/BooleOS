// nullos/kernel/drivers/vga.h
// VGA text mode 80x25 driver — public interface

#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

// Standard VGA terminal dimensions
#define VGA_COLS 80
#define VGA_ROWS 25

// VGA colors (4 bits — foreground and background)
typedef enum {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
} vga_color_t;

// Initializes and clears the terminal
void vga_init(void);

// Sets the current color (fg = foreground, bg = background)
void vga_set_color(vga_color_t fg, vga_color_t bg);

// Prints a character at the current cursor position
void vga_putchar(char c);

// Prints a null-terminated string
void vga_puts(const char *str);

// Clears the screen
void vga_clear(void);

// Moves the cursor to (col, row)
void vga_set_cursor(uint8_t col, uint8_t row);

// Prints an integer in hex (e.g. 0xDEADBEEF)
void vga_puthex(uint32_t value);

// Prints an integer in decimal
void vga_putdec(uint32_t value);

#endif // VGA_H
