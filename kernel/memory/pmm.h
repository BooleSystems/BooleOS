// nullos/kernel/memory/pmm.h
// Physical Memory Manager — public interface

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include "../hal.h"

#define PAGE_SIZE 4096  // 4KB per page

// Initializes the PMM from the bootloader's memory map (boot_get_memory_map()).
// Only BOOT_MEM_USABLE regions become allocatable, and only the part of them
// below PMM_LIMIT_ADDR (see pmm.c): the kernel can only touch physical pages
// through its 0-8MB identity map. nregions <= 0 (no map) falls back to
// assuming 1MB-8MB is usable, with a warning. The caller still has to reserve
// what the bootloader placed in RAM (ramfs module, boot info) with
// pmm_mark_used().
void pmm_init(const boot_mem_region_t *map, int nregions);

// Allocates a physical page (returns physical address or 0 if out of memory)
uint32_t pmm_alloc_page(void);

// Allocates the physical page at exactly `addr` (page aligned), if it is free.
// Returns addr, or 0 if it is out of range or already in use. The heap needs
// this: its virtual addresses ARE the physical ones (see heap.c).
uint32_t pmm_alloc_page_at(uint32_t addr);

// Frees a physical page
void pmm_free_page(uint32_t addr);

// Returns the number of free pages
uint32_t pmm_free_pages(void);

// Returns the total number of pages
uint32_t pmm_total_pages(void);

// Marks a memory region as used (e.g. kernel, IDT)
void pmm_mark_used(uint32_t addr, uint32_t size);

// Marks a memory region as free
void pmm_mark_free(uint32_t addr, uint32_t size);

// Debug: prints PMM state via VGA
void pmm_dump(void);

#endif // PMM_H
