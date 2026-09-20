// nullos/kernel/main.c
// kmain() - Phase 5: ramfs + ELF loader + exec()

#include <stdint.h>
#include "version.h"
#include "drivers/vga.h"
#include "messages.h"
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
    console_puts(msg(MSG_BOOT_OK));
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
    console_puts(msg(MSG_BOOT_LOGO_1));
    console_puts(msg(MSG_BOOT_LOGO_2));
    console_puts(msg(MSG_BOOT_LOGO_3));
    console_puts(msg(MSG_BOOT_LOGO_4));
    console_puts(msg(MSG_BOOT_LOGO_5));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    console_puts(" " NULLOS_BANNER "\n\n");

    print_separator();

    // Multiboot
    print_tag(msg(MSG_TAG_BOOT));
    if (hal_boot_init(multiboot_magic, multiboot_info_addr) < 0) {
        console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
        console_puts(msg(MSG_BOOT_INVALID_MULTIBOOT2_MAGIC));
        goto hang;
    }
    console_puts(msg(MSG_BOOT_MULTIBOOT2_OK_PREFIX)); print_ok();

    // ramfs module (optional)
    uint32_t mod_start = 0, mod_end = 0;
    int has_module = multiboot2_find_module((void *)multiboot_info_addr,
                                            &mod_start, &mod_end);
    {
        print_tag(msg(MSG_TAG_BOOT));
        if (has_module) {
            console_puts(msg(MSG_BOOT_RAMFS_MODULE_0X));
            console_put_hex(mod_start);
            console_puts(msg(MSG_BOOT_RAMFS_RANGE_SEP));
            console_put_hex(mod_end);
            console_puts(msg(MSG_BOOT_RAMFS_SIZE_OPEN));
            console_put_dec(mod_end - mod_start);
            console_puts(msg(MSG_BOOT_RAMFS_SIZE_CLOSE));
        } else {
            console_puts(msg(MSG_BOOT_NO_RAMFS_MODULE));
        }
    }

    // GDT
    print_tag(msg(MSG_TAG_GDT));
    console_puts(msg(MSG_BOOT_GDT_INITIALIZING));
    gdt_init();
    print_ok();

    // PIC
    print_tag(msg(MSG_TAG_PIC));
    console_puts(msg(MSG_BOOT_REMAPPING_IRQS));
    pic_init();
    for (int i = 0; i < 16; i++) pic_mask_irq((uint8_t)i);
    print_ok();

    // IDT
    print_tag(msg(MSG_TAG_IDT));
    console_puts(msg(MSG_BOOT_INSTALLING_VECTORS));
    idt_init();
    print_tag("       ");
    print_ok();

    // Timer
    print_tag(msg(MSG_TAG_TIMER));
    console_puts(msg(MSG_BOOT_TIMER_INIT));
    timer_init(100);
    print_ok();

    // Keyboard
    print_tag(msg(MSG_TAG_KB));
    console_puts(msg(MSG_BOOT_KEYBOARD_INIT));
    keyboard_init();
    print_ok();

    __asm__ volatile ("sti");

    print_separator();

    // PMM
    print_tag(msg(MSG_TAG_PMM));
    console_puts(msg(MSG_BOOT_PMM_INITIALIZING));
    pmm_init(64 * 1024);
    print_tag("       ");
    print_ok();
    pmm_dump();

    // VMM
    print_tag(msg(MSG_TAG_VMM));
    console_puts(msg(MSG_BOOT_ENABLING_PAGING));
    vmm_init();
    print_tag("       ");
    print_ok();
    vmm_dump();

    // Heap
    print_tag(msg(MSG_TAG_HEAP));
    console_puts(msg(MSG_BOOT_INITIALIZING_KMALLOC));
    heap_init();
    print_tag("       ");
    print_ok();

    print_separator();

    // Scheduler + kernel tasks
    print_tag(msg(MSG_TAG_SCHED));
    console_puts(msg(MSG_BOOT_SCHED_INITIALIZING));
    scheduler_init();
    print_ok();

    // ATA
    print_tag(msg(MSG_TAG_ATA));
    console_puts(msg(MSG_BOOT_DETECTING_DISK));
    if (ata_init()) {
        print_ok();
    } else {
        console_set_color(CONSOLE_DARK_GREY, CONSOLE_BLACK);
        console_puts(msg(MSG_BOOT_NO_DISK));
        console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    }

    // FAT16
    print_tag(msg(MSG_TAG_FAT16));
    console_puts(msg(MSG_BOOT_FAT16_INITIALIZING));
    if (fat16_init()) {
        print_ok();
    } else {
        console_set_color(CONSOLE_DARK_GREY, CONSOLE_BLACK);
        console_puts(msg(MSG_BOOT_NO_FAT16_DISK));
        console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    }

    // PCI (after ATA/FAT16: bus enumeration is independent hardware
    // discovery for future drivers, not on the disk-mount critical path)
    print_tag(msg(MSG_TAG_PCI));
    console_puts(msg(MSG_BOOT_SCANNING_BUS));
    pci_scan_bus();
    print_ok();
    pci_print_list();

    print_separator();

    // ramfs + exec("init") — only if GRUB passed a module
    if (has_module) {
        print_tag(msg(MSG_TAG_RAMFS));
        console_puts(msg(MSG_BOOT_MOUNTING_IMAGE));
        ramfs_init((void *)mod_start, mod_end - mod_start);
        print_tag("       ");
        print_ok();

        print_tag(msg(MSG_TAG_EXEC));
        console_puts(msg(MSG_BOOT_LOADING_SHELL));
        if (!exec("shell", 0, 0)) {   /* no launcher process at boot — starts at the root */
            console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
            console_puts(msg(MSG_BOOT_ERROR_LOADING_SHELL));
            console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
        }
    }

    scheduler_dump();
    print_separator();

    console_set_color(CONSOLE_YELLOW, CONSOLE_BLACK);
    console_puts(msg(MSG_BOOT_SHELL_STARTED));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);

    for (;;) {
        scheduler_run_once();
        __asm__ volatile ("sti; hlt");
    }

hang:
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
