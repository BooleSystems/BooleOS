#include "elf.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include <stdint.h>

/* Copy len bytes from src to dst without libc. */
static void memcpy8(uint8_t *dst, const uint8_t *src, uint32_t len) {
    for (uint32_t i = 0; i < len; i++)
        dst[i] = src[i];
}

/* Zero len bytes at dst. */
static void memzero8(uint8_t *dst, uint32_t len) {
    for (uint32_t i = 0; i < len; i++)
        dst[i] = 0;
}

int elf_load(uint32_t cr3, const void *elf_data, uint32_t *entry_point) {
    const uint8_t      *base = (const uint8_t *)elf_data;
    const elf32_ehdr_t *ehdr = (const elf32_ehdr_t *)base;

    /* Validate magic */
    if (ehdr->e_ident[0] != ELF_MAGIC0 ||
        ehdr->e_ident[1] != ELF_MAGIC1 ||
        ehdr->e_ident[2] != ELF_MAGIC2 ||
        ehdr->e_ident[3] != ELF_MAGIC3)
        return -1;

    if (ehdr->e_ident[4] != ELF_CLASS32)  return -1;  /* not 32-bit */
    if (ehdr->e_ident[5] != ELF_DATA2LSB) return -1;  /* not little-endian */
    if (ehdr->e_type      != ET_EXEC)      return -1;  /* not executable */
    if (ehdr->e_machine   != EM_386)       return -1;  /* not x86 */

    /* Iterate program headers */
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const elf32_phdr_t *ph = (const elf32_phdr_t *)
            (base + ehdr->e_phoff + i * ehdr->e_phentsize);

        if (ph->p_type != PT_LOAD)
            continue;

        uint32_t vaddr  = ph->p_vaddr;
        uint32_t memsz  = ph->p_memsz;
        uint32_t filesz = ph->p_filesz;

        if (memsz == 0)
            continue;

        /* Map every page covered by [vaddr, vaddr+memsz) */
        uint32_t page_start = vaddr & ~0xFFFu;
        uint32_t page_end   = (vaddr + memsz + 0xFFF) & ~0xFFFu;

        for (uint32_t va = page_start; va < page_end; va += PAGE_SIZE) {
            uint32_t phys = pmm_alloc_page();
            if (!phys)
                return -1;

            /* Zero the physical page (identity-mapped, <8MB) */
            memzero8((uint8_t *)phys, PAGE_SIZE);
            vmm_map_user_page(cr3, va, phys);
        }

        /* Copy file data into mapped virtual pages.
         * Since we're still in the kernel with the process CR3 not yet active,
         * we write through the identity-mapped physical addresses.
         * Walk page by page to resolve vaddr → phys via the page directory. */
        const uint8_t *src = base + ph->p_offset;
        uint32_t       copied = 0;

        while (copied < filesz) {
            uint32_t va      = vaddr + copied;
            uint32_t phys    = vmm_get_phys_from_dir(cr3, va);
            uint32_t pg_off  = va & 0xFFFu;
            uint32_t chunk   = PAGE_SIZE - pg_off;
            if (chunk > filesz - copied)
                chunk = filesz - copied;

            memcpy8((uint8_t *)phys + pg_off, src + copied, chunk);
            copied += chunk;
        }
    }

    *entry_point = ehdr->e_entry;
    return 0;
}
