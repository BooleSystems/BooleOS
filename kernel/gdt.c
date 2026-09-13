// nullos/kernel/gdt.c
// Global Descriptor Table
// Defines the system's memory segments (code/data, ring 0/3)

#include "gdt.h"
#include "tss.h"
#include <stdint.h>

// ============================================================
// Structures
// ============================================================

// A GDT entry (8 bytes)
typedef struct {
    uint16_t limit_low;     // Bits 0-15 of the limit
    uint16_t base_low;      // Bits 0-15 of the base
    uint8_t  base_mid;      // Bits 16-23 of the base
    uint8_t  access;        // Access byte (type, DPL, present)
    uint8_t  granularity;   // Flags + bits 16-19 of the limit
    uint8_t  base_high;     // Bits 24-31 of the base
} __attribute__((packed)) gdt_entry_t;

// GDTR — register the processor reads with lgdt
typedef struct {
    uint16_t limit;         // GDT size - 1
    uint32_t base;          // GDT address
} __attribute__((packed)) gdt_ptr_t;

// ============================================================
// Static data
// ============================================================

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

// ============================================================
// Internal functions
// ============================================================

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);
    gdt[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[idx].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[idx].access      = access;
}

// Loads the GDT and reloads the segment registers
extern void gdt_flush(uint32_t gdt_ptr_addr);

// ============================================================
// Public API
// ============================================================

void gdt_init(void) {
    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base  = (uint32_t)&gdt;

    // Null segment (mandatory — first entry is always zero)
    gdt_set_entry(GDT_NULL_SEG,    0, 0x00000000, 0x00, 0x00);

    // Kernel code: base=0, limit=4GB, ring 0, executable
    // Access: present(1) | DPL=00 | type=1 | executable(1) | readable(1)
    gdt_set_entry(GDT_KERNEL_CODE, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // Kernel data: base=0, limit=4GB, ring 0, read/write
    gdt_set_entry(GDT_KERNEL_DATA, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // User code: ring 3, executable
    gdt_set_entry(GDT_USER_CODE,   0, 0xFFFFFFFF, 0xFA, 0xCF);

    // User data: ring 3, read/write
    gdt_set_entry(GDT_USER_DATA,   0, 0xFFFFFFFF, 0xF2, 0xCF);

    // TSS
    tss_init(SEG_KERNEL_DATA, 0);
    uint32_t tss_addr = tss_get_addr();
    gdt_set_entry(GDT_TSS, tss_addr, sizeof(tss_entry_t), 0x89, 0x40);

    // Load the GDT and reload segments
    gdt_flush((uint32_t)&gdt_ptr);

    // Load the TSS selector
    __asm__ volatile("ltr %%ax" : : "a" (SEG_TSS));
}
