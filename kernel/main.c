	// nullos/kernel/main.c
// kmain() — Fase 2: PMM + VMM + Heap

#include <stdint.h>
#include "drivers/vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"

#define MULTIBOOT2_MAGIC 0x36d76289

static void print_ok(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("OK\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void print_tag(const char *tag) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts(tag);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void print_separator(void) {
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    for (int i = 0; i < 60; i++) vga_putchar('-');
    vga_puts("\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

// Teste básico do kmalloc/kfree
static void test_heap(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n[TESTE HEAP]\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    // Aloca alguns blocos
    void *a = kmalloc(64);
    vga_puts(" kmalloc(64)  = "); vga_puthex((uint32_t)a); vga_puts("\n");

    void *b = kmalloc(128);
    vga_puts(" kmalloc(128) = "); vga_puthex((uint32_t)b); vga_puts("\n");

    void *c = kmalloc(256);
    vga_puts(" kmalloc(256) = "); vga_puthex((uint32_t)c); vga_puts("\n");

    heap_dump();

    // Libera e realoca
    vga_puts(" kfree(b)\n");
    kfree(b);

    void *d = kmalloc(64);
    vga_puts(" kmalloc(64)  = "); vga_puthex((uint32_t)d); vga_puts("\n");

    heap_dump();

    // Libera tudo
    kfree(a);
    kfree(c);
    kfree(d);

    vga_puts(" kfree(a, c, d)\n");
    heap_dump();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts(" Heap OK!\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

void kmain(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    vga_init();

    // Banner
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("  _   _       _ _  ___  ____  \n");
    vga_puts(" | \\ | |_   _| | |/ _ \\/ ___| \n");
    vga_puts(" |  \\| | | | | | | | | \\___ \\ \n");
    vga_puts(" | |\\  | |_| | | | |_| |___) |\n");
    vga_puts(" |_| \\_|\\__,_|_|_|\\___/|____/ \n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts(" NullOS v0.2.0 - Fase 2: Memory Management\n\n");

    print_separator();

    // Multiboot
    print_tag("[BOOT] ");
    if (multiboot_magic != MULTIBOOT2_MAGIC) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("Multiboot2 magic invalido!\n");
        goto hang;
    }
    vga_puts("Multiboot2: "); print_ok();

    // GDT
    print_tag("[GDT]  ");
    vga_puts("Inicializando... ");
    gdt_init();
    print_ok();

    // PIC
    print_tag("[PIC]  ");
    vga_puts("Remapeando IRQs... ");
    pic_init();
    for (int i = 0; i < 16; i++) pic_mask_irq((uint8_t)i);
    print_ok();

    // IDT
    print_tag("[IDT]  ");
    vga_puts("Instalando vetores...\n");
    idt_init();
    print_tag("       ");
    print_ok();

    // Timer
    print_tag("[TIMER]");
    vga_puts("PIT @ 100Hz... ");
    timer_init(100);
    print_ok();

    // Teclado
    print_tag("[KB]   ");
    vga_puts("PS/2 keyboard... ");
    keyboard_init();
    print_ok();

    // Habilita interrupções
    __asm__ volatile ("sti");

    print_separator();

    // PMM
    print_tag("[PMM]  ");
    vga_puts("Inicializando...\n");
    // multiboot_info_addr+8 tem mem_upper em KB (offset padrão Multiboot1-compat)
    // Usamos 64MB como padrão seguro para QEMU
    pmm_init(64 * 1024);  // 64MB
    print_tag("       ");
    print_ok();
    pmm_dump();

    // VMM
    print_tag("[VMM]  ");
    vga_puts("Ativando paginacao...\n");
    vmm_init();
    print_tag("       ");
    print_ok();
    vmm_dump();

    // Heap
    print_tag("[HEAP] ");
    vga_puts("Inicializando kmalloc...\n");
    heap_init();
    print_tag("       ");
    print_ok();

    print_separator();

    // Testa a heap
    //test_heap();

    print_separator();

    // Status final
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n Fase 2 concluida!\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts(" PMM + VMM + kmalloc funcionando.\n");
    vga_puts(" Proximos passos: Processos + Scheduler (Fase 3)\n\n");

    vga_puts(" Digite algo:\n > ");

    for (;;) {
        __asm__ volatile ("hlt");
    }

hang:
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
