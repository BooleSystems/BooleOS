// nullos/kernel/main.c
// kmain() — ponto de entrada do NullOS após o boot.asm
// Fase 0: inicializa VGA e imprime banner de boot

#include <stdint.h>
#include <stddef.h>
#include "drivers/vga.h"

// Magic que o GRUB coloca em eax ao chamar o kernel via Multiboot2
#define MULTIBOOT2_MAGIC 0x36d76289

// ============================================================
// Banner de boot
// ============================================================

static void print_banner(void) {
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("  _   _       _ _  ___  ____  \n");
    vga_puts(" | \\ | |_   _| | |/ _ \\/ ___| \n");
    vga_puts(" |  \\| | | | | | | | | \\___ \\ \n");
    vga_puts(" | |\\  | |_| | | | |_| |___) |\n");
    vga_puts(" |_| \\_|\\__,_|_|_|\\___/|____/ \n");
    vga_puts("\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts(" NullOS v0.0.1 - Experimental AI-generated OS\n");
    vga_puts(" Fase 0: Boot\n");
    vga_puts("\n");
}

static void print_separator(void) {
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    for (int i = 0; i < 60; i++) vga_putchar('-');
    vga_puts("\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

// ============================================================
// kmain
// ============================================================

void kmain(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    // Inicializa o terminal VGA primeiro (precisamos de output)
    vga_init();

    // Banner
    print_banner();
    print_separator();

    // Verifica se o bootloader é compatível
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("[BOOT] ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    if (multiboot_magic == MULTIBOOT2_MAGIC) {
        vga_puts("Multiboot2: ");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("OK\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    } else {
        vga_puts("Multiboot2: ");
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("FALHOU (magic invalido: ");
        vga_puthex(multiboot_magic);
        vga_puts(")\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        goto hang;
    }

    // Info sobre onde o kernel foi carregado
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("[MEM]  ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("Kernel em 0x100000, multiboot_info em ");
    vga_puthex(multiboot_info_addr);
    vga_puts("\n");

    // VGA OK
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("[VGA]  ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Modo texto 80x25 ativo\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    print_separator();

    // Status final
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n Fase 0 concluida. Kernel respondendo.\n");
    vga_puts(" Proximos passos: GDT, IDT, interrupts (Fase 1)\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    // Pisca um cursor simples na tela (loop infinito por enquanto)
    vga_puts(" > ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_putchar('_');

hang:
    // Sem scheduler ainda — apenas trava com interrupções desabilitadas
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
