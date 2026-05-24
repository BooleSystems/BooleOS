// nullos/kernel/memory/heap.c
// Heap do kernel — kmalloc/kfree
// Implementação simples com lista encadeada de blocos
// Heap começa em 0x400000 (4MB) e cresce para cima

#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../drivers/vga.h"
#include <stdint.h>
#include <stddef.h>

#define HEAP_START  0x400000   // 4MB
#define HEAP_MAX    0x800000   // 8MB (4MB de heap)
#define MAGIC_FREE  0xDEAD1234
#define MAGIC_USED  0xBEEF5678

// Cabeçalho de cada bloco da heap
typedef struct block_header {
    uint32_t             magic;   // MAGIC_FREE ou MAGIC_USED
    uint32_t             size;    // Tamanho do bloco (sem o header)
    struct block_header *next;    // Próximo bloco
    struct block_header *prev;    // Bloco anterior
} block_header_t;

#define HEADER_SIZE sizeof(block_header_t)

static block_header_t *heap_start_ptr = 0;
static uint32_t        heap_end       = HEAP_START;

// ============================================================
// Funções internas
// ============================================================

// Expande a heap alocando novas páginas físicas
static int heap_expand(uint32_t size) {
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t i;

    for (i = 0; i < pages_needed; i++) {
        if (heap_end + PAGE_SIZE > HEAP_MAX) {
            vga_puts("heap: ERRO sem espaco para expandir!\n");
            return 0;
        }
        uint32_t phys = pmm_alloc_page();
        if (!phys) {
            vga_puts("heap: ERRO sem paginas fisicas!\n");
            return 0;
        }
        vmm_map_page(heap_end, phys, VMM_KERNEL);
        heap_end += PAGE_SIZE;
    }
    return 1;
}

// ============================================================
// API pública
// ============================================================

void heap_init(void) {
    vga_puts("   heap: inicializando em ");
    vga_puthex(HEAP_START);
    vga_puts("\n");

    // Aloca primeira página
    if (!heap_expand(PAGE_SIZE)) {
        vga_puts("   heap: ERRO ao inicializar!\n");
        return;
    }

    // Cria bloco inicial cobrindo toda a heap disponível
    heap_start_ptr = (block_header_t *)HEAP_START;
    heap_start_ptr->magic = MAGIC_FREE;
    heap_start_ptr->size  = heap_end - HEAP_START - HEADER_SIZE;
    heap_start_ptr->next  = 0;
    heap_start_ptr->prev  = 0;

    vga_puts("   heap: bloco inicial size=");
    vga_putdec(heap_start_ptr->size);
    vga_puts(" bytes\n");
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;

    // Alinha a 4 bytes
    size = (size + 3) & ~3U;

    block_header_t *cur = heap_start_ptr;

    while (cur) {
        if (cur->magic != MAGIC_FREE && cur->magic != MAGIC_USED) {
            vga_puts("heap: CORRUPCAO DETECTADA!\n");
            return 0;
        }

        if (cur->magic == MAGIC_FREE && cur->size >= size) {
            // Bloco livre grande o suficiente

            // Divide o bloco se sobrar espaço
            if (cur->size >= size + HEADER_SIZE + 4) {
                block_header_t *new_block = (block_header_t *)
                    ((uint8_t *)cur + HEADER_SIZE + size);
                new_block->magic = MAGIC_FREE;
                new_block->size  = cur->size - size - HEADER_SIZE;
                new_block->next  = cur->next;
                new_block->prev  = cur;
                if (cur->next)
                    cur->next->prev = new_block;
                cur->next = new_block;
                cur->size = size;
            }

            cur->magic = MAGIC_USED;
            return (void *)((uint8_t *)cur + HEADER_SIZE);
        }

        cur = cur->next;
    }

    // Sem bloco livre — expande a heap
    if (!heap_expand(size + HEADER_SIZE)) return 0;

    // Recria bloco no final
    block_header_t *new_block = (block_header_t *)(heap_end - PAGE_SIZE);
    new_block->magic = MAGIC_FREE;
    new_block->size  = PAGE_SIZE - HEADER_SIZE;
    new_block->next  = 0;
    new_block->prev  = 0;

    // Tenta alocar de novo
    return kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_header_t *hdr = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);

    if (hdr->magic != MAGIC_USED) {
        vga_puts("kfree: ERRO ponteiro invalido ou duplo free!\n");
        return;
    }

    hdr->magic = MAGIC_FREE;

    // Merge com bloco seguinte se também for livre
    if (hdr->next && hdr->next->magic == MAGIC_FREE) {
        hdr->size += HEADER_SIZE + hdr->next->size;
        hdr->next  = hdr->next->next;
        if (hdr->next)
            hdr->next->prev = hdr;
    }

    // Merge com bloco anterior se também for livre
    if (hdr->prev && hdr->prev->magic == MAGIC_FREE) {
        hdr->prev->size += HEADER_SIZE + hdr->size;
        hdr->prev->next  = hdr->next;
        if (hdr->next)
            hdr->next->prev = hdr->prev;
    }
}

void heap_dump(void) {
    block_header_t *cur = heap_start_ptr;
    uint32_t        free_bytes = 0;
    uint32_t        used_bytes = 0;
    uint32_t        n_blocks   = 0;

    while (cur) {
        if (cur->magic == MAGIC_FREE)
            free_bytes += cur->size;
        else
            used_bytes += cur->size;
        n_blocks++;
        cur = cur->next;
    }

    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("[HEAP] ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("Blocos: ");
    vga_putdec(n_blocks);
    vga_puts(" | Livre: ");
    vga_putdec(free_bytes);
    vga_puts("B | Usado: ");
    vga_putdec(used_bytes);
    vga_puts("B\n");
}
