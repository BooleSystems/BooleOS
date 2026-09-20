// nullos/kernel/main.c
// kmain() - Phase 5: ramfs + ELF loader + exec()

#include <stdint.h>
#include "version.h"
#include "drivers/vga.h"
#include "hal.h"
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
#include "drivers/ata.h"
#include "fs/fat16.h"
#include "drivers/pci.h"


static void print_ok(void) {
    console_set_color(CONSOLE_LIGHT_GREEN, CONSOLE_BLACK);
    console_puts("OK\n");
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
}

static void print_tag(const char *tag) {
    console_set_color(CONSOLE_WHITE, CONSOLE_BLACK);
    console_puts(tag);
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
}

static void print_separator(void) {
    console_set_color(CONSOLE_DARK_GREY, CONSOLE_BLACK);
    for (int i = 0; i < 60; i++) console_putc('-');
    console_puts("\n");
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
}


void kmain(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    serial_init();
    vga_init();

    // Banner
    console_set_color(CONSOLE_CYAN, CONSOLE_BLACK);
    console_puts("  _   _       _ _  ___  ____  \n");
    console_puts(" | \\ | |_   _| | |/ _ \\/ ___| \n");
    console_puts(" |  \\| | | | | | | | | \\___ \\ \n");
    console_puts(" | |\\  | |_| | | | |_| |___) |\n");
    console_puts(" |_| \\_|\\__,_|_|_|\\___/|____/ \n\n");
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    console_puts(" " NULLOS_BANNER "\n\n");

    print_separator();

    // Multiboot
    print_tag("[BOOT] ");
    if (hal_boot_init(multiboot_magic, multiboot_info_addr) < 0) {
        console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
        console_puts("Invalid Multiboot2 magic!\n");
        goto hang;
    }
    console_puts("Multiboot2: "); print_ok();

    // ramfs module (optional)
    uint32_t mod_start = 0, mod_end = 0;
    int has_module = multiboot2_find_module((void *)multiboot_info_addr,
                                            &mod_start, &mod_end);
    {
        print_tag("[BOOT] ");
        if (has_module) {
            console_puts("ramfs module: 0x");
            console_put_hex(mod_start);
            console_puts(" - 0x");
            console_put_hex(mod_end);
            console_puts(" (");
            console_put_dec(mod_end - mod_start);
            console_puts(" bytes)\n");
        } else {
            console_puts("no module — running without ramfs\n");
        }
    }

    // GDT
    print_tag("[GDT]  ");
    console_puts("Initializing... ");
    gdt_init();
    print_ok();

    // PIC
    print_tag("[PIC]  ");
    console_puts("Remapping IRQs... ");
    pic_init();
    for (int i = 0; i < 16; i++) pic_mask_irq((uint8_t)i);
    print_ok();

    // IDT
    print_tag("[IDT]  ");
    console_puts("Installing vectors...\n");
    idt_init();
    print_tag("       ");
    print_ok();

    // Timer
    print_tag("[TIMER]");
    console_puts("PIT @ 100Hz... ");
    timer_init(100);
    print_ok();

    // Keyboard
    print_tag("[KB]   ");
    console_puts("PS/2 keyboard... ");
    keyboard_init();
    print_ok();

    __asm__ volatile ("sti");

    print_separator();

    // PMM
    print_tag("[PMM]  ");
    console_puts("Initializing...\n");
    pmm_init(64 * 1024);
    print_tag("       ");
    print_ok();
    pmm_dump();

    // VMM
    print_tag("[VMM]  ");
    console_puts("Enabling paging...\n");
    vmm_init();
    print_tag("       ");
    print_ok();
    vmm_dump();

    // Heap
    print_tag("[HEAP] ");
    console_puts("Initializing kmalloc...\n");
    heap_init();
    print_tag("       ");
    print_ok();

    print_separator();

    // Scheduler + kernel tasks
    print_tag("[SCHED]");
    console_puts("Initializing scheduler... ");
    scheduler_init();
    print_ok();

    // ATA
    print_tag("[ATA]  ");
    console_puts("Detecting disk... ");
    if (ata_init()) {
        print_ok();
    } else {
        console_set_color(CONSOLE_DARK_GREY, CONSOLE_BLACK);
        console_puts("no disk\n");
        console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    }

    // FAT16
    print_tag("[FAT16]");
    console_puts(" Initializing... ");
    if (fat16_init()) {
        print_ok();
    } else {
        console_set_color(CONSOLE_DARK_GREY, CONSOLE_BLACK);
        console_puts("no FAT16 disk\n");
        console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    }

    // PCI (after ATA/FAT16: bus enumeration is independent hardware
    // discovery for future drivers, not on the disk-mount critical path)
    print_tag("[PCI]  ");
    console_puts("Scanning bus... ");
    pci_scan_bus();
    print_ok();
    pci_print_list();

    print_separator();

    // ramfs + exec("init") — only if GRUB passed a module
    if (has_module) {
        print_tag("[RAMFS]");
        console_puts("Mounting image...\n");
        ramfs_init((void *)mod_start, mod_end - mod_start);
        print_tag("       ");
        print_ok();

        print_tag("[EXEC] ");
        console_puts("Loading shell...\n");
        if (!exec("shell", 0, 0)) {   /* no launcher process at boot — starts at the root */
            console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
            console_puts("ERROR loading shell\n");
            console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
        }
    }

    scheduler_dump();
    print_separator();

    console_set_color(CONSOLE_YELLOW, CONSOLE_BLACK);
    console_puts("\n Shell started!\n");
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);

    for (;;) {
        scheduler_run_once();
        __asm__ volatile ("sti; hlt");
    }

hang:
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
