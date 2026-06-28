#include "ramfs.h"

static uint8_t        *ramfs_base    = 0;
static uint32_t        ramfs_size    = 0;
static uint32_t        ramfs_nfiles  = 0;
static ramfs_entry_t  *ramfs_entries = 0;

void ramfs_init(void *data, uint32_t size) {
    if (!data || size < sizeof(uint32_t))
        return;

    ramfs_base   = (uint8_t *)data;
    ramfs_size   = size;
    ramfs_nfiles = *(uint32_t *)data;
    ramfs_entries = (ramfs_entry_t *)(ramfs_base + sizeof(uint32_t));
}

int ramfs_find(const char *name, uint32_t *out_offset, uint32_t *out_size) {
    if (!ramfs_base || !name)
        return 0;

    for (uint32_t i = 0; i < ramfs_nfiles; i++) {
        ramfs_entry_t *e = &ramfs_entries[i];

        /* strcmp sem libc */
        const char *a = name, *b = e->name;
        while (*a && *a == *b) { a++; b++; }
        if (*a != *b)
            continue;

        if (e->offset + e->size > ramfs_size)
            return 0;  /* entry corrompida */

        if (out_offset) *out_offset = e->offset;
        if (out_size)   *out_size   = e->size;
        return 1;
    }

    return 0;
}
