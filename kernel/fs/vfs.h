#ifndef VFS_H
#define VFS_H

#include <stdint.h>

typedef enum {
    VFS_NONE   = 0,
    VFS_RAMFS  = 1,
    VFS_FAT16  = 2,
} vfs_backend_t;

typedef struct {
    uint8_t       used;
    vfs_backend_t backend;
    uint32_t      first;      /* ramfs: byte offset; fat16: first cluster */
    uint32_t      size;
    uint32_t      pos;
    char          name[32];   /* original name — needed for FAT16 writes */
} vfs_fd_t;

/* Opens a file by name. Tries ramfs first, then FAT16.
   Fills *fd and returns 0 on success, -1 if not found.  */
int  vfs_open (const char *name, vfs_fd_t *fd);

/* Opens the file if it already exists (ramfs or FAT16); otherwise creates
   an empty entry in FAT16 and opens it. Returns 0 on success, -1 if the
   disk is unavailable or the root dir has no free space. */
int  vfs_create(const char *name, vfs_fd_t *fd);

/* Reads up to len bytes starting at fd->pos. Returns bytes read or -1. */
int  vfs_read (vfs_fd_t *fd, char *buf, uint32_t len);

/* Closes the fd (marks it as unused). */
void vfs_close(vfs_fd_t *fd);

/* Writes len bytes from buf into the file referenced by fd (FAT16 only).
   Returns 0 on success, -1 if the backend doesn't support writes or on error. */
int  vfs_write(vfs_fd_t *fd, const char *buf, uint32_t len);

#endif
