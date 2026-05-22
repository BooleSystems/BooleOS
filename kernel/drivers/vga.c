// nullos/kernel/drivers/vga.c
// Driver VGA modo texto 80x25
// O buffer VGA fica em 0xB8000. Cada célula = 2 bytes: [atributo | char]

#include "vga.h"
#include <stdint.h>
#include <stddef.h>

// Endereço do framebuffer VGA em modo texto
#define VGA_BUFFER ((volatile uint16_t *)0xB8000)

// Portas I/O do controlador VGA (para mover cursor via hardware)
#define VGA_CTRL_PORT   0x3D4
#define VGA_DATA_PORT   0x3D5
#define VGA_CURSOR_HIGH 0x0E
#define VGA_CURSOR_LOW  0x0F

// Estado interno do terminal
static uint8_t  term_col   = 0;
static uint8_t  term_row   = 0;
static uint8_t  term_color = 0;

// ============================================================
// Funções internas
// ============================================================

// Escreve num port I/O (precisamos disso para mover o cursor)
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Monta o byte de atributo VGA: [bg(4) | fg(4)]
static inline uint8_t vga_make_attr(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)((bg << 4) | (fg & 0x0F));
}

// Monta uma entry VGA: [atributo(8) | char(8)]
static inline uint16_t vga_make_entry(char c, uint8_t attr) {
    return (uint16_t)((uint16_t)attr << 8) | (uint8_t)c;
}

// Atualiza o cursor de hardware para a posição atual
static void vga_update_cursor(void) {
    uint16_t pos = (uint16_t)(term_row * VGA_COLS + term_col);
    outb(VGA_CTRL_PORT, VGA_CURSOR_HIGH);
    outb(VGA_DATA_PORT, (uint8_t)(pos >> 8));
    outb(VGA_CTRL_PORT, VGA_CURSOR_LOW);
    outb(VGA_DATA_PORT, (uint8_t)(pos & 0xFF));
}

// Rola o terminal uma linha pra cima
static void vga_scroll(void) {
    // Move todas as linhas uma pra cima
    for (int row = 1; row < VGA_ROWS; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            VGA_BUFFER[(row - 1) * VGA_COLS + col] =
                VGA_BUFFER[row * VGA_COLS + col];
        }
    }
    // Limpa a última linha
    uint16_t blank = vga_make_entry(' ', term_color);
    for (int col = 0; col < VGA_COLS; col++) {
        VGA_BUFFER[(VGA_ROWS - 1) * VGA_COLS + col] = blank;
    }
    term_row = VGA_ROWS - 1;
}

// ============================================================
// API pública
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
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\r') {
        term_col = 0;
    } else if (c == '\t') {
        // Tab = próxima coluna múltipla de 4
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

    // Scroll se passou da última linha
    if (term_row >= VGA_ROWS) {
        vga_scroll();
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
    // Imprime ao contrário
    for (int i = idx - 1; i >= 0; i--) {
        vga_putchar(buf[i]);
    }
}
