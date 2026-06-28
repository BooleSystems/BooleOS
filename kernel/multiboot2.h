#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include <stdint.h>

/* MBI fixed header (comes right after the magic/info_addr passed to kmain) */
typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) mb2_header_t;

/* Every tag starts with these two fields */
typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) mb2_tag_t;

/* Tag type 3 — module loaded by GRUB */
#define MB2_TAG_MODULE 3
#define MB2_TAG_END    0

typedef struct {
    uint32_t type;      /* = 3 */
    uint32_t size;
    uint32_t mod_start; /* physical address */
    uint32_t mod_end;   /* physical address (exclusive) */
    char     cmdline[]; /* null-terminated module name/args */
} __attribute__((packed)) mb2_tag_module_t;

/*
 * multiboot2_find_module - scan MBI tags for the first module.
 *
 * mbi       : value of multiboot_info_addr passed to kmain (physical addr)
 * mod_start : out — physical start address of the module
 * mod_end   : out — physical end address of the module (exclusive)
 *
 * Returns 1 if a module was found, 0 otherwise.
 */
static inline int multiboot2_find_module(void *mbi,
                                         uint32_t *mod_start,
                                         uint32_t *mod_end)
{
    mb2_header_t *hdr = (mb2_header_t *)mbi;
    uint8_t      *p   = (uint8_t *)mbi + sizeof(mb2_header_t);
    uint8_t      *end = (uint8_t *)mbi + hdr->total_size;

    while (p < end) {
        mb2_tag_t *tag = (mb2_tag_t *)p;

        if (tag->type == MB2_TAG_END)
            break;

        if (tag->type == MB2_TAG_MODULE) {
            mb2_tag_module_t *m = (mb2_tag_module_t *)p;
            *mod_start = m->mod_start;
            *mod_end   = m->mod_end;
            return 1;
        }

        /* Tags are aligned to 8 bytes */
        uint32_t aligned = (tag->size + 7) & ~7u;
        p += aligned ? aligned : 8;
    }

    return 0;
}

#endif /* MULTIBOOT2_H */
