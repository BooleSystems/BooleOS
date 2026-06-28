#ifndef RAMFS_H
#define RAMFS_H

#include <stdint.h>

typedef struct {
    char     name[32];
    uint32_t offset;  /* byte offset from start of image */
    uint32_t size;    /* byte size of file data */
} ramfs_entry_t;

/* Image layout:
 *   [uint32_t num_entries]
 *   [ramfs_entry_t entries[num_entries]]
 *   [file data...]                        <- offsets are relative to image start
 */

void ramfs_init(void *data, uint32_t size);
int  ramfs_find(const char *name, uint32_t *out_offset, uint32_t *out_size);

#endif
