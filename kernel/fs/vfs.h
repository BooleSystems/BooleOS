#ifndef VFS_H
#define VFS_H

#include <stdint.h>

typedef enum {
    VFS_NONE       = 0,
    VFS_RAMFS      = 1,
    VFS_FAT16      = 2,
    VFS_PIPE_READ  = 3,   /* Phase 16: kernel/pipe.h. `first` is the pipe_table index. */
    VFS_PIPE_WRITE = 4,
} vfs_backend_t;

typedef struct {
    uint8_t       used;
    vfs_backend_t backend;
    uint32_t      first;      /* ramfs: byte offset; fat16: first cluster */
    uint32_t      size;
    uint32_t      pos;
    uint32_t      parent_cluster; /* FAT16 only: cluster of the directory
                                      containing this file's dirent (0 =
                                      root), captured at open/create time
                                      so a later write re-finds the exact
                                      same entry regardless of whatever
                                      the process's cwd has changed to in
                                      the meantime (see vfs_write). Unused
                                      for ramfs. */
    char          name[32];   /* FAT16: the file's own name (last path
                                  component only, no "/") — needed for
                                  FAT16 writes; ramfs: the full (flat)
                                  name, ramfs has no subdirectories. */
} vfs_fd_t;

/* Opens a file by path. Tries ramfs first (always a flat namespace — it
   never gained subdirectories), then FAT16. cwd_cluster is the base a
   relative FAT16 path resolves against (0 = root); an absolute path
   (starting with "/") always resolves from the root regardless.
   Fills *fd and returns 0 on success, -1 if not found in either backend. */
int  vfs_open (uint32_t cwd_cluster, const char *name, vfs_fd_t *fd);

/* Opens the file if it already exists (ramfs or FAT16); otherwise creates
   an empty entry in FAT16 (in the directory named by cwd_cluster/name,
   same convention as vfs_open) and opens it. Returns 0 on success, -1 if
   the disk is unavailable, an intermediate path component is missing, or
   the containing directory has no free space. */
int  vfs_create(uint32_t cwd_cluster, const char *name, vfs_fd_t *fd);

/* Reads up to len bytes starting at fd->pos. Returns bytes read or -1. */
int  vfs_read (vfs_fd_t *fd, char *buf, uint32_t len);

/* Closes the fd (marks it as unused). */
void vfs_close(vfs_fd_t *fd);

/* Stream write: writes len bytes from buf at the fd's current position
   (fd->pos, advanced by the amount written) — a FAT16 file grows as
   needed, a pipe's write end appends to the pipe. Consecutive calls
   ACCUMULATE. For FAT16 this uses fd->parent_cluster/fd->name (captured
   at open/create time), NOT the caller's current cwd — independent of
   any cd() between opening and writing. Returns 0 if all len bytes were
   written, -1 otherwise (backend doesn't support writes, I/O error, disk
   full — in which case the part that did fit is kept — or a pipe whose
   read end has been fully closed, see kernel/pipe.h). */
int  vfs_write(vfs_fd_t *fd, const char *buf, uint32_t len);

/* Whole-file replace (FAT16 only): the file's entire content becomes
   exactly buf[0..len) — the old chain is freed first, len == 0
   truncates. This is what SYS_WRITE_FILE ("save the buffer") needs; it
   is NOT a stream write. Ignores and does not update fd->pos. Returns 0
   on success, -1 on error or a non-FAT16 fd. */
int  vfs_write_all(vfs_fd_t *fd, const char *buf, uint32_t len);

/* Adds one reference to fd's underlying resource, if that resource is
   refcounted (currently: pipe ends only — ramfs/FAT16 have no
   refcounting, so this is a no-op for them). MUST be called whenever
   a vfs_fd_t is copied into a SECOND fd_table slot by anything other
   than vfs_open()/vfs_create() themselves — fork()'s fd_table row
   duplication (sys_fork()), or SYS_EXEC_PIPE seeding the new
   process's fd_table with the caller's pipe end. Skipping this for a
   pipe means a later vfs_close() on just one of the copies could drop
   the pipe's refcount to 0 while another copy is still genuinely
   open, making the pipe look fully closed (EOF/broken-pipe fires) to
   the other end while a real holder is still using it. */
void vfs_dup(vfs_fd_t *fd);

#endif
