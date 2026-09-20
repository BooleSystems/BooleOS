/* nullos/kernel/fs/vfs.c — filesystem dispatcher */
#include "vfs.h"
#include "fat16.h"
#include "../ramfs.h"
#include "../pipe.h"
#include <stdint.h>

static void vfs_store_name(vfs_fd_t *fd, const char *name) {
    uint32_t i = 0;
    while (i < sizeof(fd->name) - 1 && name[i]) { fd->name[i] = name[i]; i++; }
    fd->name[i] = '\0';
}

/* Stores only the LAST "/"-separated component of path into fd->name —
   see the vfs_fd_t.name comment in vfs.h for why (vfs_write must re-find
   the entry without depending on the process's cwd at write time). */
static void vfs_store_final_component(vfs_fd_t *fd, const char *path) {
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/') last = p + 1;
    }
    vfs_store_name(fd, last);
}

int vfs_open(uint32_t cwd_cluster, const char *name, vfs_fd_t *fd) {
    if (!name || !fd) return -1;

    /* try ramfs first (flat namespace, no subdirectories) */
    uint32_t off, sz;
    if (ramfs_find(name, &off, &sz)) {
        fd->used    = 1;
        fd->backend = VFS_RAMFS;
        fd->first   = off;
        fd->size    = sz;
        fd->pos     = 0;
        fd->parent_cluster = 0;   /* unused for ramfs */
        vfs_store_name(fd, name);
        return 0;
    }

    /* then try FAT16 */
    if (fat16_available()) {
        uint32_t cluster = 0, size = 0, parent = 0;
        int found = fat16_find(cwd_cluster, name, &cluster, &size, &parent);
        if (found == 1) {
            fd->used    = 1;
            fd->backend = VFS_FAT16;
            fd->first   = cluster;
            fd->size    = size;
            fd->pos     = 0;
            fd->parent_cluster = parent;
            vfs_store_final_component(fd, name);
            return 0;
        }
    }

    return -1;
}

int vfs_create(uint32_t cwd_cluster, const char *name, vfs_fd_t *fd) {
    if (!name || !fd) return -1;

    /* already exists (ramfs or FAT16)? just open it */
    if (vfs_open(cwd_cluster, name, fd) == 0) return 0;

    if (!fat16_available()) return -1;

    uint32_t parent = 0;
    if (fat16_create(cwd_cluster, name, &parent) < 0) return -1;

    uint32_t cluster = 0, size = 0;
    if (fat16_find(cwd_cluster, name, &cluster, &size, 0) != 1) return -1;

    fd->used    = 1;
    fd->backend = VFS_FAT16;
    fd->first   = cluster;
    fd->size    = size;
    fd->pos     = 0;
    fd->parent_cluster = parent;
    vfs_store_final_component(fd, name);
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

    if (fd->backend == VFS_PIPE_READ)
        return pipe_read(fd->first, buf, len);

    return -1;
}

int vfs_write(vfs_fd_t *fd, const char *buf, uint32_t len) {
    if (!fd || !fd->used) return -1;

    if (fd->backend == VFS_PIPE_WRITE)
        return pipe_write(fd->first, buf, len);

    if (fd->backend != VFS_FAT16) return -1;  /* ramfs is read-only */
    if (len == 0) return 0;

    /* stream write: goes to fd->pos and advances it, so consecutive
       calls accumulate instead of each replacing the whole file */
    uint32_t first = fd->first, size = fd->size;
    int r = fat16_write_at(fd->parent_cluster, fd->name, fd->pos, buf, len, &first, &size);
    if (r < 0) return -1;

    fd->first = first;
    fd->size  = size;
    fd->pos  += (uint32_t)r;
    return ((uint32_t)r == len) ? 0 : -1;   /* short write = disk full */
}

int vfs_write_all(vfs_fd_t *fd, const char *buf, uint32_t len) {
    if (!fd || !fd->used) return -1;
    if (fd->backend != VFS_FAT16) return -1;
    return fat16_write_file(fd->parent_cluster, fd->name, buf, len);
}

void vfs_close(vfs_fd_t *fd) {
    if (!fd) return;
    if (fd->used) {
        if (fd->backend == VFS_PIPE_READ)  pipe_release_read(fd->first);
        if (fd->backend == VFS_PIPE_WRITE) pipe_release_write(fd->first);
    }
    fd->used = 0;
}

void vfs_dup(vfs_fd_t *fd) {
    if (!fd || !fd->used) return;
    if (fd->backend == VFS_PIPE_READ)  pipe_add_read_ref(fd->first);
    if (fd->backend == VFS_PIPE_WRITE) pipe_add_write_ref(fd->first);
}
