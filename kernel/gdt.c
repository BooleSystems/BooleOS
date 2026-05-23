// nullos/kernel/gdt.c
// Global Descriptor Table
// Define os segmentos de memória do sistema (código/dados, ring 0/3)

#include "gdt.h"
#include <stdint.h>

// ============================================================
// Estruturas
// ============================================================

// Uma entrada da GDT (8 bytes)
typedef struct {
    uint16_t limit_low;     // Bits 0-15 do limite
    uint16_t base_low;      // Bits 0-15 da base
    uint8_t  base_mid;      // Bits 16-23 da base
    uint8_t  access;        // Byte de acesso (tipo, DPL, presente)
    uint8_t  granularity;   // Flags + bits 16-19 do limite
    uint8_t  base_high;     // Bits 24-31 da base
} __attribute__((packed)) gdt_entry_t;

// GDTR — registrador que o processador lê com lgdt
typedef struct {
    uint16_t limit;         // Tamanho da GDT - 1
    uint32_t base;          // Endereço da GDT
} __attribute__((packed)) gdt_ptr_t;

// ============================================================
// Dados estáticos
// ============================================================

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

// ============================================================
// Funções internas
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

// Carrega a GDT e recarrega os registradores de segmento
extern void gdt_flush(uint32_t gdt_ptr_addr);

// ============================================================
// API pública
// ============================================================

void gdt_init(void) {
    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base  = (uint32_t)&gdt;

    // Segmento nulo (obrigatório — primeiro entry sempre zero)
    gdt_set_entry(GDT_NULL_SEG,    0, 0x00000000, 0x00, 0x00);

    // Código do kernel: base=0, limite=4GB, ring 0, executável
    // Access: presente(1) | DPL=00 | tipo=1 | executável(1) | leitura(1)
    gdt_set_entry(GDT_KERNEL_CODE, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // Dados do kernel: base=0, limite=4GB, ring 0, leitura/escrita
    gdt_set_entry(GDT_KERNEL_DATA, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // Código do usuário: ring 3, executável
    gdt_set_entry(GDT_USER_CODE,   0, 0xFFFFFFFF, 0xFA, 0xCF);

    // Dados do usuário: ring 3, leitura/escrita
    gdt_set_entry(GDT_USER_DATA,   0, 0xFFFFFFFF, 0xF2, 0xCF);

    // Carrega a GDT e recarrega segmentos
    gdt_flush((uint32_t)&gdt_ptr);
}
