#include "exec.h"
#include "ramfs.h"
#include "elf.h"
#include "scheduler.h"
#include "memory/vmm.h"
#include "memory/pmm.h"
#include "drivers/vga.h"

#define USER_STACK_VIRT  0x02000000U   /* virtual base of user stack */
#define USER_STACK_PAGES 2             /* 8 KB user stack */

process_t *exec(const char *name) {
    uint32_t file_offset = 0, file_size = 0;

    if (!ramfs_find(name, &file_offset, &file_size)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("[EXEC] nao encontrado: ");
        vga_puts(name);
        vga_puts("\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return 0;
    }

    /* ramfs_find returns offset relative to image start; the image base was
       passed to ramfs_init(), which we recover via the global below.          */
    extern uint8_t *ramfs_base;   /* defined in ramfs.c */
    const void *elf_data = (const void *)(ramfs_base + file_offset);

    uint32_t cr3 = vmm_create_directory();
    if (!cr3) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("[EXEC] falha ao criar page directory\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return 0;
    }

    uint32_t entry = 0;
    if (elf_load(cr3, elf_data, &entry) != 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("[EXEC] elf_load falhou: ");
        vga_puts(name);
        vga_puts("\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return 0;
    }

    /* Allocate and map user stack pages */
    for (uint32_t i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t va   = USER_STACK_VIRT + i * PAGE_SIZE;
        uint32_t phys = pmm_alloc_page();
        if (!phys) {
            vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
            vga_puts("[EXEC] sem memoria para user stack\n");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            return 0;
        }
        vmm_map_user_page(cr3, va, phys);
    }

    uint32_t user_esp = USER_STACK_VIRT + USER_STACK_PAGES * PAGE_SIZE;

    process_t *p = scheduler_spawn_user(name, entry, user_esp, cr3);
    if (!p) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("[EXEC] scheduler_spawn_user falhou\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return 0;
    }

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[EXEC] spawned: ");
    vga_puts(name);
    vga_puts(" entry=0x");
    vga_puthex(entry);
    vga_puts(" esp=0x");
    vga_puthex(user_esp);
    vga_puts("\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    return p;
}
