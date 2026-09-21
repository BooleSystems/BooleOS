// nullos/kernel/memory/pmm.c
#include "pmm.h"
#include "../hal.h"
#include "../messages.h"
#include <stdint.h>

// Bitmap at a fixed safe address: 0x202000 (right after the IDT at 0x200000)
#define PMM_BITMAP_ADDR 0x202000

// The PMM only manages physical memory the kernel can actually touch. The
// kernel reaches a page by its physical address (elf_load() zeroes user pages,
// process_fork() copies them) and only 0-8MB is identity-mapped, so nothing
// above PMM_LIMIT_ADDR may be handed out. This is a MITIGATION of a
// pre-existing bug (see PROGRESS.md, "Known technical debt": identity map): an
// exhausted PMM now fails an allocation instead of faulting in the kernel.
// The real fix (Phase 23) is to stop accessing frames by physical address.
#define PMM_LIMIT_ADDR  0x800000u
#define PMM_MAX_PAGES   (PMM_LIMIT_ADDR / PAGE_SIZE)   // 2048 pages = 8MB
#define PMM_BITMAP_SIZE (PMM_MAX_PAGES / 32)

static uint32_t pmm_total = 0;
static uint32_t pmm_used  = 0;

static uint32_t *get_bitmap(void) {
    return (uint32_t *)PMM_BITMAP_ADDR;
}

static inline void bitmap_set(uint32_t page) {
    get_bitmap()[page / 32] |= (1U << (page % 32));
}
static inline void bitmap_clear(uint32_t page) {
    get_bitmap()[page / 32] &= ~(1U << (page % 32));
}
static inline int bitmap_test(uint32_t page) {
    return (get_bitmap()[page / 32] >> (page % 32)) & 1;
}

void pmm_mark_used(uint32_t addr, uint32_t size) {
    uint32_t page = addr / PAGE_SIZE;
    uint32_t n = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t i;
    for (i = 0; i < n && (page+i) < pmm_total; i++) {
        if (!bitmap_test(page+i)) { bitmap_set(page+i); pmm_used++; }
    }
}

void pmm_mark_free(uint32_t addr, uint32_t size) {
    uint32_t page = addr / PAGE_SIZE;
    uint32_t n = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t i;
    for (i = 0; i < n && (page+i) < pmm_total; i++) {
        if (bitmap_test(page+i)) { bitmap_clear(page+i); pmm_used--; }
    }
}

uint32_t pmm_free_pages(void)  { return pmm_total - pmm_used; }
uint32_t pmm_total_pages(void) { return pmm_total; }

uint32_t pmm_alloc_page(void) {
    uint32_t i, bit;
    for (i = 0; i < PMM_BITMAP_SIZE; i++) {
        if (get_bitmap()[i] == 0xFFFFFFFF) continue;
        for (bit = 0; bit < 32; bit++) {
            uint32_t page = i * 32 + bit;
            if (page >= pmm_total) return 0;
            if (!bitmap_test(page)) {
                bitmap_set(page); pmm_used++;
                return page * PAGE_SIZE;
            }
        }
    }
    return 0;
}

uint32_t pmm_alloc_page_at(uint32_t addr) {
    if (addr & (PAGE_SIZE - 1)) return 0;
    uint32_t page = addr / PAGE_SIZE;
    if (page >= pmm_total || bitmap_test(page)) return 0;
    bitmap_set(page); pmm_used++;
    return addr;
}

void pmm_free_page(uint32_t addr) {
    uint32_t page = addr / PAGE_SIZE;
    if (page >= pmm_total || !bitmap_test(page)) return;
    bitmap_clear(page); pmm_used--;
}

void pmm_init(const boot_mem_region_t *map, int nregions) {
    uint32_t i;

    // Every page starts USED (bitmap all ones, counter = every page); only
    // the usable regions below are then released. The counter used to start
    // at 0 while the bitmap was all ones, so every release drove it negative
    // and pmm_free_pages() over-reported by the total page count.
    pmm_total = PMM_MAX_PAGES;
    pmm_used  = PMM_MAX_PAGES;

    console_puts(msg(MSG_PMM_1_BITMAP_AT)); console_put_hex(PMM_BITMAP_ADDR); console_puts("\n");
    for (i = 0; i < PMM_BITMAP_SIZE; i++) get_bitmap()[i] = 0xFFFFFFFF;

    console_puts(msg(MSG_PMM_2_FREEING_HIGH_MEM));
    if (nregions > 0) {
        uint64_t highest = 0;   // end of the last usable region, capped at the limit
        for (int r = 0; r < nregions; r++) {
            if (map[r].type != BOOT_MEM_USABLE) continue;
            // Round inward to whole pages; a region can start or end mid-page.
            uint64_t start = (map[r].base + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
            uint64_t end   = (map[r].base + map[r].length) & ~(uint64_t)(PAGE_SIZE - 1);
            if (end > PMM_LIMIT_ADDR) end = PMM_LIMIT_ADDR;
            if (start >= end) continue;   // empty, or entirely above the limit
            pmm_mark_free((uint32_t)start, (uint32_t)(end - start));
            if (end > highest) highest = end;
        }
        // On a machine with less than 8MB, the pages past the last usable
        // byte don't exist; don't count them as managed.
        if (highest && highest < PMM_LIMIT_ADDR) {
            uint32_t managed = (uint32_t)(highest / PAGE_SIZE);
            pmm_used -= (PMM_MAX_PAGES - managed);   // those pages were counted as used
            pmm_total = managed;
        }
    } else {
        console_puts(msg(MSG_PMM_NO_MEMORY_MAP));
        pmm_mark_free(0x100000, PMM_LIMIT_ADDR - 0x100000);
    }

    console_puts(msg(MSG_PMM_3_MARKING_USED_REGIONS));
    pmm_mark_used(0x0, 0x100000);       // first MB: real-mode area, BIOS, VGA
    pmm_mark_used(0x100000, 0x300000);  // 1-4MB: kernel image, ramfs module, IDT, bitmap, page tables

    console_puts(msg(MSG_PMM_4_FREE)); console_put_dec(pmm_free_pages()); console_puts(msg(MSG_PMM_PAGES_NL));
}

void pmm_dump(void) {
    console_set_color(CONSOLE_CYAN, CONSOLE_BLACK);
    console_puts(msg(MSG_PMM_DUMP_TAG));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    console_puts(msg(MSG_PMM_TOTAL)); console_put_dec(pmm_total_pages() * 4);
    console_puts(msg(MSG_PMM_KB_FREE)); console_put_dec(pmm_free_pages() * 4); console_puts(msg(MSG_PMM_KB_NL));
}
