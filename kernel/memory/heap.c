// nullos/kernel/memory/heap.c
// Kernel heap — kmalloc/kfree
// Simple implementation using a linked list of blocks
// The heap starts at 0x400000 (4MB) and grows upward

#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../drivers/vga.h"
#include <stdint.h>
#include <stddef.h>

#define HEAP_START  0x400000   // 4MB
#define HEAP_MAX    0x800000   // 8MB (4MB of heap)
#define MAGIC_FREE  0xDEAD1234
#define MAGIC_USED  0xBEEF5678

// Header for each heap block
typedef struct block_header {
    uint32_t             magic;   // MAGIC_FREE or MAGIC_USED
    uint32_t             size;    // Block size (without the header)
    struct block_header *next;    // Next block
    struct block_header *prev;    // Previous block
} block_header_t;

#define HEADER_SIZE sizeof(block_header_t)

static block_header_t *heap_start_ptr = 0;
static uint32_t        heap_end       = HEAP_START;

// ============================================================
// Internal functions
// ============================================================

// Expands the heap by allocating new physical pages
static int heap_expand(uint32_t size) {
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t i;

    for (i = 0; i < pages_needed; i++) {
        if (heap_end + PAGE_SIZE > HEAP_MAX) {
            vga_puts("heap: ERROR no room to expand!\n");
            return 0;
        }
        uint32_t phys = pmm_alloc_page();
        if (!phys) {
            vga_puts("heap: ERROR out of physical pages!\n");
            return 0;
        }
        if (vmm_map_page(heap_end, phys, VMM_KERNEL) != 0) {
            pmm_free_page(phys);
            vga_puts("heap: ERROR out of page tables!\n");
            return 0;
        }
        heap_end += PAGE_SIZE;
    }
    return 1;
}

// ============================================================
// Public API
// ============================================================

void heap_init(void) {
    vga_puts("   heap: initializing at ");
    vga_puthex(HEAP_START);
    vga_puts("\n");

    // Allocate the first page
    if (!heap_expand(PAGE_SIZE)) {
        vga_puts("   heap: ERROR initializing!\n");
        return;
    }

    // Create the initial block covering the whole available heap
    heap_start_ptr = (block_header_t *)HEAP_START;
    heap_start_ptr->magic = MAGIC_FREE;
    heap_start_ptr->size  = heap_end - HEAP_START - HEADER_SIZE;
    heap_start_ptr->next  = 0;
    heap_start_ptr->prev  = 0;

    vga_puts("   heap: initial block size=");
    vga_putdec(heap_start_ptr->size);
    vga_puts(" bytes\n");
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;

    // Reject anything that could never fit before doing any arithmetic on
    // it: a size close to UINT32_MAX would overflow the "align to 4 bytes"
    // step below (size + 3 wrapping past 0), silently handing back a much
    // smaller block than requested. The heap can never satisfy more than
    // HEAP_MAX - HEAP_START bytes anyway, so this is a real limit, not an
    // arbitrary one.
    if (size > (HEAP_MAX - HEAP_START)) return 0;

    // Align to 4 bytes
    size = (size + 3) & ~3U;

    block_header_t *cur = heap_start_ptr;

    while (cur) {
        if (cur->magic != MAGIC_FREE && cur->magic != MAGIC_USED) {
            vga_puts("heap: CORRUPTION DETECTED!\n");
            return 0;
        }

        if (cur->magic == MAGIC_FREE && cur->size >= size) {
            // Free block big enough

            // Split the block if there's leftover space
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

    // No free block — expand the heap and append the new space to the list.
    uint32_t old_end = heap_end;
    if (!heap_expand(size + HEADER_SIZE)) return 0;

    block_header_t *new_block = (block_header_t *)old_end;
    new_block->magic = MAGIC_FREE;
    new_block->size  = heap_end - old_end - HEADER_SIZE;
    new_block->next  = 0;
    new_block->prev  = 0;

    block_header_t *tail = heap_start_ptr;
    while (tail && tail->next)
        tail = tail->next;

    if (!tail) {
        heap_start_ptr = new_block;
    } else {
        tail->next = new_block;
        new_block->prev = tail;

        if (tail->magic == MAGIC_FREE) {
            tail->size += HEADER_SIZE + new_block->size;
            tail->next = 0;
        }
    }

    // Try allocating again now that the expanded block is linked in.
    return kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_header_t *hdr = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);

    if (hdr->magic != MAGIC_USED) {
        vga_puts("kfree: ERROR invalid pointer or double free!\n");
        return;
    }

    hdr->magic = MAGIC_FREE;

    // Merge with the next block if it's also free
    if (hdr->next && hdr->next->magic == MAGIC_FREE) {
        hdr->size += HEADER_SIZE + hdr->next->size;
        hdr->next  = hdr->next->next;
        if (hdr->next)
            hdr->next->prev = hdr;
    }

    // Merge with the previous block if it's also free
    if (hdr->prev && hdr->prev->magic == MAGIC_FREE) {
        hdr->prev->size += HEADER_SIZE + hdr->size;
        hdr->prev->next  = hdr->next;
        if (hdr->next)
            hdr->next->prev = hdr->prev;
    }
}

uint32_t heap_free_bytes(void) {
    block_header_t *cur = heap_start_ptr;
    uint32_t free = 0;
    while (cur) {
        if (cur->magic == MAGIC_FREE) free += cur->size;
        cur = cur->next;
    }
    return free;
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
    vga_puts("Blocks: ");
    vga_putdec(n_blocks);
    vga_puts(" | Free: ");
    vga_putdec(free_bytes);
    vga_puts("B | Used: ");
    vga_putdec(used_bytes);
    vga_puts("B\n");
}
