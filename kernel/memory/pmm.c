// nullos/kernel/memory/pmm.c
#include "pmm.h"
#include "../drivers/vga.h"
#include <stdint.h>

// Bitmap em endereco fixo seguro: 0x202000 (logo apos IDT em 0x200000)
#define PMM_BITMAP_ADDR 0x202000
#define PMM_MAX_PAGES   8192
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

void pmm_free_page(uint32_t addr) {
    uint32_t page = addr / PAGE_SIZE;
    if (page >= pmm_total || !bitmap_test(page)) return;
    bitmap_clear(page); pmm_used--;
}

void pmm_init(uint32_t mem_upper) {
    uint32_t i;
    uint32_t total_pages = (1024 + mem_upper) * 1024 / PAGE_SIZE;
    if (total_pages > PMM_MAX_PAGES) total_pages = PMM_MAX_PAGES;
    pmm_total = total_pages;
    pmm_used  = 0;

    vga_puts("   pmm: [1] bitmap em "); vga_puthex(PMM_BITMAP_ADDR); vga_puts("\n");
    for (i = 0; i < PMM_BITMAP_SIZE; i++) get_bitmap()[i] = 0xFFFFFFFF;

    vga_puts("   pmm: [2] liberando mem alta\n");
    pmm_mark_free(0x100000, (total_pages - 256) * PAGE_SIZE);

    vga_puts("   pmm: [3] marcando regioes usadas\n");
    pmm_mark_used(0x100000, 0x10000);
    pmm_mark_used(0x200000, 0x3000);
    pmm_mark_used(0x0F0000, 0x10000);

    vga_puts("   pmm: [4] livre="); vga_putdec(pmm_free_pages()); vga_puts(" pages\n");
}

void pmm_dump(void) {
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("[PMM] ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("Total: "); vga_putdec(pmm_total_pages() * 4);
    vga_puts("KB Livre: "); vga_putdec(pmm_free_pages() * 4); vga_puts("KB\n");
}
