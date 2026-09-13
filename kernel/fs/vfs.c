/* nullos/kernel/fs/vfs.c — despachante de filesystem */
#include "vfs.h"
#include "fat16.h"
#include "../ramfs.h"
#include <stdint.h>

static void vfs_store_name(vfs_fd_t *fd, const char *name) {
    uint32_t i = 0;
    while (i < sizeof(fd->name) - 1 && name[i]) { fd->name[i] = name[i]; i++; }
    fd->name[i] = '\0';
}

int vfs_open(const char *name, vfs_fd_t *fd) {
    if (!name || !fd) return -1;

    /* tenta ramfs primeiro */
    uint32_t off, sz;
    if (ramfs_find(name, &off, &sz)) {
        fd->used    = 1;
        fd->backend = VFS_RAMFS;
        fd->first   = off;
        fd->size    = sz;
        fd->pos     = 0;
        vfs_store_name(fd, name);
        return 0;
    }

    /* tenta FAT16 */
    if (fat16_available()) {
        uint32_t cluster = 0, size = 0;
        int found = fat16_find(name, &cluster, &size);
        if (found == 1) {
            fd->used    = 1;
            fd->backend = VFS_FAT16;
            fd->first   = cluster;
            fd->size    = size;
            fd->pos     = 0;
            vfs_store_name(fd, name);
            return 0;
        }
    }

    return -1;
}

int vfs_create(const char *name, vfs_fd_t *fd) {
    if (!name || !fd) return -1;

    /* já existe (ramfs ou FAT16)? só abre */
    if (vfs_open(name, fd) == 0) return 0;

    if (!fat16_available()) return -1;
    if (fat16_create(name) < 0) return -1;

    uint32_t cluster = 0, size = 0;
    if (fat16_find(name, &cluster, &size) != 1) return -1;

    fd->used    = 1;
    fd->backend = VFS_FAT16;
    fd->first   = cluster;
    fd->size    = size;
    fd->pos     = 0;
    vfs_store_name(fd, name);
    return 0;
}

int vfs_read(vfs_fd_t *fd, char *buf, uint32_t len) {
    if (!fd || !fd->used || !buf || len == 0) return -1;

    if (fd->backend == VFS_RAMFS) {
        if (!ramfs_base) return -1;
        uint32_t avail = fd->size - fd->pos;
        if (avail == 0) return 0;
        if (len > avail) len = avail;
        uint8_t *src = ramfs_base + fd->first + fd->pos;
        for (uint32_t i = 0; i < len; i++) buf[i] = (char)src[i];
        fd->pos += len;
        return (int)len;
    }

    if (fd->backend == VFS_FAT16) {
        int r = fat16_read_at(fd->first, fd->pos, buf, len, fd->size);
        if (r > 0) fd->pos += (uint32_t)r;
        return r;
    }

    return -1;
}

int vfs_write(vfs_fd_t *fd, const char *buf, uint32_t len) {
    if (!fd || !fd->used) return -1;
    if (fd->backend != VFS_FAT16) return -1;  /* ramfs é read-only */
    return fat16_write_file(fd->name, buf, len);
}

void vfs_close(vfs_fd_t *fd) {
    if (fd) fd->used = 0;
}
