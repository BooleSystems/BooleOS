#include "exec.h"
#include "ramfs.h"
#include "elf.h"
#include "scheduler.h"
#include "memory/vmm.h"
#include "memory/pmm.h"
#include "hal.h"
#include "fs/vfs.h"
#include "memory/heap.h"
#include "messages.h"

#define USER_STACK_VIRT  0x02000000U   /* virtual base of user stack */
#define USER_STACK_PAGES 2             /* 8 KB user stack */

/* A program file bigger than this is refused (it is read whole into a kernel
   heap buffer before loading). Generous for today's programs (~10-20 KB). */
#define EXEC_MAX_FILE_SIZE 0x100000u

static void exec_fail(msg_id_t id, const char *name) {
    console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
    console_puts(msg(id));
    console_puts(name);
    console_puts("\n");
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
}

process_t *exec(const char *name, uint32_t cwd_cluster, int start_blocked) {
    /* The program is found by the SAME lookup every file open uses,
       vfs_open(): the ramfs first (flat, read-only, system programs — a disk
       file can never shadow one), then FAT16 resolved against the caller's
       cwd (so "run tools/hello.elf" works like "cat tools/x.txt"). There is
       no second lookup loop here on purpose: a duplicated dirent lookup is
       exactly how earlier FAT16 bugs happened. The vfs_fd_t is a local, never
       entered in any process's fd table. */
    vfs_fd_t file;
    if (vfs_open(cwd_cluster, name, &file) < 0) {
        exec_fail(MSG_EXEC_NOT_FOUND, name);
        return 0;
    }

    const uint8_t *elf_data;
    uint32_t       elf_size = file.size;
    void          *disk_copy = 0;      /* heap buffer for a FAT16 program, freed below */

    if (file.backend == VFS_RAMFS) {
        /* The ramfs image is already in memory: load straight from it. */
        elf_data = ramfs_base + file.first;
    } else {
        /* FAT16: the size is the directory entry's, never assumed. The file
           is READ from disk into a heap buffer (nothing about it is in
           memory yet), then elf_load() copies its segments into the new
           address space. */
        if (elf_size == 0 || elf_size > EXEC_MAX_FILE_SIZE) {
            vfs_close(&file);
            exec_fail(MSG_EXEC_BAD_SIZE, name);
            return 0;
        }
        disk_copy = kmalloc(elf_size);
        if (!disk_copy) {
            vfs_close(&file);
            exec_fail(MSG_EXEC_OUT_OF_MEMORY_FILE, name);
            return 0;
        }
        uint32_t got = 0;
        while (got < elf_size) {
            int r = vfs_read(&file, (char *)disk_copy + got, elf_size - got);
            if (r <= 0) {                 /* error, or the file ended early */
                kfree(disk_copy);
                vfs_close(&file);
                exec_fail(MSG_EXEC_READ_FAILED, name);
                return 0;
            }
            got += (uint32_t)r;
        }
        elf_data = (const uint8_t *)disk_copy;
    }
    vfs_close(&file);

    uint32_t cr3 = vmm_create_directory();
    if (!cr3) {
        console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
        console_puts(msg(MSG_EXEC_FAILED_TO_CREATE_PAGE));
        console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
        return 0;
    }

    uint32_t entry = 0;
    int loaded = elf_load(cr3, elf_data, elf_size, &entry);
    if (disk_copy) kfree(disk_copy);      /* the segments were copied out (or loading failed) */
    if (loaded != 0) {
        exec_fail(MSG_EXEC_ELF_LOAD_FAILED, name);
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
