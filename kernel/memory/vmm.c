// nullos/kernel/memory/vmm.c
// Virtual Memory Manager
// Configura paginação x86 (32-bit, sem PAE)
// Page Directory (1024 entradas) -> Page Tables (1024 entradas cada)
// Cada página = 4KB, espaço total = 4GB

#include "vmm.h"
#include "pmm.h"
#include "../drivers/vga.h"
#include <stdint.h>

// ============================================================
// Estruturas de paginação x86
// ============================================================

// Uma entrada do Page Directory ou Page Table
typedef uint32_t pde_t;
typedef uint32_t pte_t;

// Page Directory: 1024 entradas de 4 bytes = 4KB
// Colocado num endereço fixo seguro: 0x201000 (logo após a IDT)
#define PAGE_DIR_ADDR   0x201000
#define PAGE_TABLE_POOL 0x202000  // Pool de page tables: 0x202000 - 0x302000 (256 tables * 4KB)

static uint32_t page_table_pool_next = PAGE_TABLE_POOL;

// ============================================================
// Funções internas
// ============================================================

static pde_t *get_page_dir(void) {
    return (pde_t *)PAGE_DIR_ADDR;
}

static pte_t *get_or_create_page_table(uint32_t dir_idx) {
    pde_t *page_dir = get_page_dir();
    pde_t  entry    = page_dir[dir_idx];

    if (entry & VMM_PRESENT) {
        // Page table já existe — retorna endereço dela
        return (pte_t *)(entry & 0xFFFFF000);
    }

    // Aloca nova page table do pool
    if (page_table_pool_next >= PAGE_TABLE_POOL + 256 * PAGE_SIZE) {
        vga_puts("vmm: ERRO pool de page tables esgotado!\n");
        return 0;
    }

    pte_t *new_table = (pte_t *)page_table_pool_next;
    page_table_pool_next += PAGE_SIZE;

    // Zera a page table
    uint32_t i;
    for (i = 0; i < 1024; i++)
        new_table[i] = 0;

    // Registra no page directory
    page_dir[dir_idx] = (uint32_t)new_table | VMM_KERNEL;

    return new_table;
}

static inline void flush_tlb(uint32_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

// ============================================================
// API pública
// ============================================================

void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t dir_idx  = virt >> 22;          // Bits 31-22
    uint32_t tbl_idx  = (virt >> 12) & 0x3FF; // Bits 21-12

    pte_t *table = get_or_create_page_table(dir_idx);
    if (!table) return;

    table[tbl_idx] = (phys & 0xFFFFF000) | (flags & 0xFFF) | VMM_PRESENT;
    flush_tlb(virt);
}

void vmm_unmap_page(uint32_t virt) {
    uint32_t dir_idx = virt >> 22;
    uint32_t tbl_idx = (virt >> 12) & 0x3FF;
    pde_t   *page_dir = get_page_dir();

    if (!(page_dir[dir_idx] & VMM_PRESENT)) return;

    pte_t *table = (pte_t *)(page_dir[dir_idx] & 0xFFFFF000);
    table[tbl_idx] = 0;
    flush_tlb(virt);
}

uint32_t vmm_get_phys(uint32_t virt) {
    uint32_t dir_idx  = virt >> 22;
    uint32_t tbl_idx  = (virt >> 12) & 0x3FF;
    pde_t   *page_dir = get_page_dir();

    if (!(page_dir[dir_idx] & VMM_PRESENT)) return 0;

    pte_t *table = (pte_t *)(page_dir[dir_idx] & 0xFFFFF000);
    if (!(table[tbl_idx] & VMM_PRESENT)) return 0;

    return (table[tbl_idx] & 0xFFFFF000) | (virt & 0xFFF);
}

void vmm_init(void) {
    uint32_t i;
    pde_t *page_dir = get_page_dir();

    vga_puts("   vmm: zerando page directory em ");
    vga_puthex(PAGE_DIR_ADDR);
    vga_puts("\n");

    // Zera o page directory
    for (i = 0; i < 1024; i++)
        page_dir[i] = 0;

    vga_puts("   vmm: mapeando kernel (identity map 0-8MB)\n");

    // Identity map dos primeiros 8MB (kernel + estruturas)
    // Virtual == Físico para o kernel não quebrar após ativar paginação
    uint32_t addr;
    for (addr = 0; addr < 0x800000; addr += PAGE_SIZE) {
        vmm_map_page(addr, addr, VMM_KERNEL);
    }

    vga_puts("   vmm: carregando CR3 e ativando paginacao\n");

    // Carrega o page directory no CR3
    __asm__ volatile ("mov %0, %%cr3" : : "r"(PAGE_DIR_ADDR));

    // Ativa paginação (bit 31 do CR0)
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    vga_puts("   vmm: paginacao ativa!\n");
}

void vmm_dump(void) {
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("[VMM] ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("Page dir: ");
    vga_puthex(PAGE_DIR_ADDR);
    vga_puts(" | Pool usado: ");
    vga_putdec((page_table_pool_next - PAGE_TABLE_POOL) / PAGE_SIZE);
    vga_puts(" page tables\n");
}
