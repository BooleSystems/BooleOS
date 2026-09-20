#include "exec.h"
#include "ramfs.h"
#include "elf.h"
#include "scheduler.h"
#include "memory/vmm.h"
#include "memory/pmm.h"
#include "hal.h"
#include "messages.h"

#define USER_STACK_VIRT  0x02000000U   /* virtual base of user stack */
#define USER_STACK_PAGES 2             /* 8 KB user stack */

process_t *exec(const char *name, uint32_t cwd_cluster, int start_blocked) {
    uint32_t file_offset = 0, file_size = 0;

    if (!ramfs_find(name, &file_offset, &file_size)) {
        console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
        console_puts(msg(MSG_EXEC_NOT_FOUND));
        console_puts(name);
        console_puts("\n");
        console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
        return 0;
    }

    /* ramfs_find returns offset relative to image start; the image base was
       passed to ramfs_init(), which we recover via the global below.          */
    extern uint8_t *ramfs_base;   /* defined in ramfs.c */
    const void *elf_data = (const void *)(ramfs_base + file_offset);

    uint32_t cr3 = vmm_create_directory();
    if (!cr3) {
        console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
        console_puts(msg(MSG_EXEC_FAILED_TO_CREATE_PAGE));
        console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
        return 0;
    }

    uint32_t entry = 0;
    if (elf_load(cr3, elf_data, &entry) != 0) {
        console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
        console_puts(msg(MSG_EXEC_ELF_LOAD_FAILED));
        console_puts(name);
        console_puts("\n");
        console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
        return 0;
    }

    /* Allocate and map user stack pages */
    for (uint32_t i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t va   = USER_STACK_VIRT + i * PAGE_SIZE;
        uint32_t phys = pmm_alloc_page();
        if (!phys) {
            console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
            console_puts(msg(MSG_EXEC_OUT_OF_MEMORY_FOR));
            console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
            return 0;
        }
        if (vmm_map_user_page(cr3, va, phys) != 0) {
            pmm_free_page(phys);
            console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
            console_puts(msg(MSG_EXEC_FAILED_TO_MAP_USER));
            console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
            return 0;
        }
    }

    uint32_t user_esp = USER_STACK_VIRT + USER_STACK_PAGES * PAGE_SIZE;

    process_t *p = scheduler_spawn_user(name, entry, user_esp, cr3, cwd_cluster, start_blocked);
    if (!p) {
        console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
        console_puts(msg(MSG_EXEC_SCHEDULER_SPAWN_USER_FAILED));
        console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
        return 0;
    }

    console_set_color(CONSOLE_LIGHT_GREEN, CONSOLE_BLACK);
    console_puts(msg(MSG_EXEC_SPAWNED));
    console_puts(name);
    console_puts(msg(MSG_EXEC_ENTRY_0X));
    console_put_hex(entry);
    console_puts(msg(MSG_EXEC_ESP_0X));
    console_put_hex(user_esp);
    console_puts("\n");
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    return p;
}
