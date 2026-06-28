// nullos/kernel/main.c
// kmain() - Fase 5: ramfs + ELF loader + exec()

#include <stdint.h>
#include "drivers/vga.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"
#include "scheduler.h"
#include "multiboot2.h"
#include "ramfs.h"
#include "exec.h"

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

static void heartbeat_task(void *arg) {
    (void)arg;
    uint32_t beats = 0;
    for (;;) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("[task:heartbeat] beat ");
        vga_putdec(++beats);
        vga_puts("\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        scheduler_sleep_current(100);
    }
}

static void heap_watch_task(void *arg) {
    (void)arg;
    uint32_t samples = 0;
    for (;;) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_puts("[task:heap-watch] sample ");
        vga_putdec(++samples);
        vga_puts("\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        heap_dump();
        scheduler_sleep_current(250);
    }
}

void kmain(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    serial_init();
    vga_init();

    // Banner
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("  _   _       _ _  ___  ____  \n");
    vga_puts(" | \\ | |_   _| | |/ _ \\/ ___| \n");
    vga_puts(" |  \\| | | | | | | | | \\___ \\ \n");
    vga_puts(" | |\\  | |_| | | | |_| |___) |\n");
    vga_puts(" |_| \\_|\\__,_|_|_|\\___/|____/ \n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts(" NullOS v0.5.0 - Fase 5: ramfs + ELF loader\n\n");

    print_separator();

    // Multiboot
    print_tag("[BOOT] ");
    if (multiboot_magic != MULTIBOOT2_MAGIC) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("Multiboot2 magic invalido!\n");
        goto hang;
    }
    vga_puts("Multiboot2: "); print_ok();

    // Módulo ramfs (opcional)
    uint32_t mod_start = 0, mod_end = 0;
    int has_module = multiboot2_find_module((void *)multiboot_info_addr,
                                            &mod_start, &mod_end);
    {
        print_tag("[BOOT] ");
        if (has_module) {
            vga_puts("modulo ramfs: 0x");
            vga_puthex(mod_start);
            vga_puts(" - 0x");
            vga_puthex(mod_end);
            vga_puts(" (");
            vga_putdec(mod_end - mod_start);
            vga_puts(" bytes)\n");
        } else {
            vga_puts("nenhum modulo — modo sem ramfs\n");
        }
    }

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

    __asm__ volatile ("sti");

    print_separator();

    // PMM
    print_tag("[PMM]  ");
    vga_puts("Inicializando...\n");
    pmm_init(64 * 1024);
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

    // Scheduler + tasks de kernel
    print_tag("[SCHED]");
    vga_puts("Inicializando scheduler... ");
    scheduler_init();
    process_t *heartbeat = scheduler_spawn("heartbeat", heartbeat_task, 0);
    process_t *heap_watch = scheduler_spawn("heap-watch", heap_watch_task, 0);
    if (!heartbeat || !heap_watch) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("ERRO ao criar tasks\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        goto hang;
    }
    print_ok();

    // ramfs + exec("init") — só se o GRUB passou um módulo
    if (has_module) {
        print_tag("[RAMFS]");
        vga_puts("Montando imagem...\n");
        ramfs_init((void *)mod_start, mod_end - mod_start);
        print_tag("       ");
        print_ok();

        print_tag("[EXEC] ");
        vga_puts("Carregando init...\n");
        if (!exec("init")) {
            vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
            vga_puts("ERRO ao carregar init\n");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            /* não fatal — continua sem processo de usuário */
        }
    }

    scheduler_dump();
    print_separator();

    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n Fase 5 iniciada!\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts(" ramfs + ELF loader + exec() prontos.\n\n");

    for (;;) {
        scheduler_run_once();
        __asm__ volatile ("sti; hlt");
    }

hang:
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
