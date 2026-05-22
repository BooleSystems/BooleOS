// nullos/kernel/drivers/vga.h
// Driver VGA modo texto 80x25 — interface pública

#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

// Dimensões do terminal VGA padrão
#define VGA_COLS 80
#define VGA_ROWS 25

// Cores VGA (4 bits — foreground e background)
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

// Inicializa e limpa o terminal
void vga_init(void);

// Define cor atual (fg = foreground, bg = background)
void vga_set_color(vga_color_t fg, vga_color_t bg);

// Imprime um caractere na posição atual do cursor
void vga_putchar(char c);

// Imprime uma string null-terminated
void vga_puts(const char *str);

// Limpa a tela
void vga_clear(void);

// Move o cursor para (col, row)
void vga_set_cursor(uint8_t col, uint8_t row);

// Imprime um número inteiro em hex (ex: 0xDEADBEEF)
void vga_puthex(uint32_t value);

// Imprime um número inteiro em decimal
void vga_putdec(uint32_t value);

#endif // VGA_H
