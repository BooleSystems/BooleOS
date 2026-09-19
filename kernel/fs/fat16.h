#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

/* Reads the BPB from sector 0, caches the FAT in the heap.
   Returns 1 if a disk is present and FAT16 is valid, 0 otherwise. */
int fat16_init(void);

/* Looks up a file by path (case-insensitive 8.3 components, "/"-separated,
   e.g. "docs/notes.txt"). dir_cluster is the base directory a relative
   path (one that doesn't start with "/") is resolved against — 0 means
   the root directory; pass the caller's cwd_cluster for a normal lookup.
   An absolute path (starting with "/") always resolves from the root
   regardless of dir_cluster. Fills *first_cluster and *size, and, if
   out_parent_cluster is non-NULL, the cluster of the directory that
   directly contains the entry (0 = root) — callers that need to write
   the file later (see vfs_fd_t.parent_cluster) cache this instead of
   re-resolving the whole path on every write.
   Returns 1 if found, 0 if it doesn't exist (or names a directory, not
   a file), -1 if an I/O error prevented confirming (the caller must NOT
   treat -1 as "doesn't exist", or it risks creating duplicate entries). */
int fat16_find(uint32_t dir_cluster, const char *path,
                uint32_t *first_cluster, uint32_t *size,
                uint32_t *out_parent_cluster);

/* Reads len bytes from the file starting at byte offset pos in the cluster chain.
   Returns bytes read, or -1 on error.                                             */
int fat16_read_at(uint32_t first_cluster, uint32_t pos,
                  char *buf, uint32_t len, uint32_t file_size);

/* Returns the next cluster in the chain (>= 0xFFF8 = end). */
uint32_t fat16_next_cluster(uint32_t cluster);

/* Size of a cluster in bytes. */
uint32_t fat16_bytes_per_cluster(void);

/* 1 if the driver initialized successfully. */
int fat16_available(void);

/* Creates an empty entry (size=0) for path, if it doesn't exist yet, in
   the directory it names (same dir_cluster/path convention as
   fat16_find). Idempotent: if a FILE by that name already exists,
   returns 0 without changing anything. Fails if a DIRECTORY already
   exists under that name, or if an intermediate path component doesn't
   exist / isn't a directory. out_parent_cluster (may be NULL) receives
   the cluster of the containing directory, same as fat16_find.
   Returns 0 on success, -1 on error (parent directory full, an
   intermediate component missing, name collides with a directory, or
   the disk is unavailable). */
int fat16_create(uint32_t dir_cluster, const char *path, uint32_t *out_parent_cluster);

/* Creates a new, empty subdirectory named by path (same dir_cluster/path
   convention as fat16_find), with freshly zeroed "." (points to itself)
   and ".." (points to the parent, 0 if the parent is the root) entries
   already written. Idempotent: if a DIRECTORY by that name already
   exists, returns 0 without changing anything. Fails if a FILE already
   exists under that name, or if an intermediate path component doesn't
   exist / isn't a directory.
   Returns 0 on success, -1 on error (parent directory full, disk full,
   an intermediate component missing, name collides with a file, or the
   disk is unavailable). */
int fat16_mkdir(uint32_t dir_cluster, const char *path);

/* Writes buf (len bytes) to the file named `name` (a single path
   component — no "/" — already resolved to live directly inside
   parent_cluster; 0 = root). Must already exist there.
   Frees the old chain, allocates a new one, writes the data, updates the FAT and dir entry.
   Returns 0 on success, -1 on error.                                                        */
int fat16_write_file(uint32_t parent_cluster, const char *name, const char *buf, uint32_t len);

/* Iterates valid entries of the directory at dir_cluster (0-based; 0 =
   root). "." and ".." are never returned. Fills name (up to 12 chars +
   '\0'), *size, and, if is_dir is non-NULL, whether the entry is itself
   a directory (in which case *size is always 0 and should be ignored).
   Returns 1 if an entry was found at index idx, 0 if there are no more. */
int fat16_readdir(uint32_t dir_cluster, uint32_t idx, char name[13],
                   uint32_t *size, uint8_t *is_dir);

/* Resolves path (same dir_cluster/path convention as fat16_find) to the
   cluster of the DIRECTORY it names — used by both "cd" (SYS_CHDIR) and
   "ls <path>" (SYS_READDIR). "" and "/" resolve to the root (cluster 0)
   without touching disk.
   Returns 1 (and fills *out_cluster) on success, 0 if it doesn't exist,
   -1 on I/O error, -2 if it exists but names a file, not a directory. */
int fat16_resolve_dir(uint32_t cwd_cluster, const char *path, uint32_t *out_cluster);

#endif
