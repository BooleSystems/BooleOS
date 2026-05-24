// nullos/kernel/memory/vmm.c
#include "vmm.h"
#include "pmm.h"
#include "../drivers/vga.h"
#include <stdint.h>

typedef uint32_t pde_t;
typedef uint32_t pte_t;

#define PAGE_DIR_ADDR    0x10A000
#define PAGE_TABLE_START 0x10B000
#define PAGE_TABLE_END   0x14B000

static uint32_t pt_next      = PAGE_TABLE_START;
static uint8_t  paging_active = 0;

static void zero_4kb(uint32_t addr) {
    uint32_t *p = (uint32_t *)addr;
    uint32_t i;
    for (i = 0; i < 1024; i++) p[i] = 0;
}

static void map_page_early(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t di = virt >> 22;
    uint32_t ti = (virt >> 12) & 0x3FF;
    pde_t *pd = (pde_t *)PAGE_DIR_ADDR;
    if (!(pd[di] & VMM_PRESENT)) {
        if (pt_next >= PAGE_TABLE_END) return;
        zero_4kb(pt_next);
        pd[di] = pt_next | VMM_KERNEL;
        pt_next += PAGE_SIZE;
    }
    pte_t *pt = (pte_t *)(pd[di] & 0xFFFFF000);
    pt[ti] = (phys & 0xFFFFF000) | (flags & 0xFFF) | VMM_PRESENT;
}

void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    map_page_early(virt, phys, flags);
    if (paging_active)
        __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap_page(uint32_t virt) {
    uint32_t di = virt >> 22;
    uint32_t ti = (virt >> 12) & 0x3FF;
    pde_t *pd = (pde_t *)PAGE_DIR_ADDR;
    if (!(pd[di] & VMM_PRESENT)) return;
    pte_t *pt = (pte_t *)(pd[di] & 0xFFFFF000);
    pt[ti] = 0;
    if (paging_active)
        __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uint32_t vmm_get_phys(uint32_t virt) {
    uint32_t di = virt >> 22;
    uint32_t ti = (virt >> 12) & 0x3FF;
    pde_t *pd = (pde_t *)PAGE_DIR_ADDR;
    if (!(pd[di] & VMM_PRESENT)) return 0;
    pte_t *pt = (pte_t *)(pd[di] & 0xFFFFF000);
    if (!(pt[ti] & VMM_PRESENT)) return 0;
    return (pt[ti] & 0xFFFFF000) | (virt & 0xFFF);
}

void vmm_init(void) {
    uint32_t i;
    uint32_t *pd  = (uint32_t *)PAGE_DIR_ADDR;
    uint32_t *pt0 = (uint32_t *)PAGE_TABLE_START;
    uint32_t *pt1 = (uint32_t *)(PAGE_TABLE_START + PAGE_SIZE);

    vga_puts("   vmm: [1] zerando PD e PTs\n");
    for (i = 0; i < 1024; i++) {
        pd[i]  = 0;
        pt0[i] = 0;
        pt1[i] = 0;
    }

    vga_puts("   vmm: [2] mapeando 0-4MB\n");
    for (i = 0; i < 1024; i++)
        pt0[i] = (i * PAGE_SIZE) | VMM_KERNEL | VMM_PRESENT;

    vga_puts("   vmm: [3] mapeando 4MB-8MB\n");
    for (i = 0; i < 1024; i++)
        pt1[i] = (0x400000 + i * PAGE_SIZE) | VMM_KERNEL | VMM_PRESENT;

    pd[0] = PAGE_TABLE_START | VMM_KERNEL | VMM_PRESENT;
    pd[1] = (PAGE_TABLE_START + PAGE_SIZE) | VMM_KERNEL | VMM_PRESENT;

    vga_puts("   vmm: [4] CR3 + CR0.PG\n");
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)PAGE_DIR_ADDR) : "memory");
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");

    paging_active = 1;
    vga_puts("   vmm: [5] ok!\n");
}

void vmm_dump(void) {
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("[VMM] ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("PTs usadas: ");
    vga_putdec((pt_next - PAGE_TABLE_START) / PAGE_SIZE);
    vga_puts("\n");
}
