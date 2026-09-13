#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

/* Reads the BPB from sector 0, caches the FAT in the heap.
   Returns 1 if a disk is present and FAT16 is valid, 0 otherwise. */
int fat16_init(void);

/* Looks up a file by name in the root directory (case-insensitive, supports "a.b").
   Fills *first_cluster and *size. Returns 1 if found, 0 if it doesn't
   exist, -1 if an I/O error prevented confirming (the caller must NOT
   treat -1 as "doesn't exist", or it risks creating duplicate entries). */
int fat16_find(const char *name, uint32_t *first_cluster, uint32_t *size);

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

/* Creates an empty entry (size=0) for name in the root dir, if it doesn't exist yet.
   Idempotent: if the file already exists, returns 0 without changing anything.
   Returns 0 on success, -1 if the root directory is full or the disk is unavailable. */
int fat16_create(const char *name);

/* Writes buf (len bytes) to file name (must already exist in the root dir).
   Frees the old chain, allocates a new one, writes the data, updates the FAT and dir entry.
   Returns 0 on success, -1 on error.                                                        */
int fat16_write_file(const char *name, const char *buf, uint32_t len);

/* Iterates valid entries in the root directory (0-based).
   Fills name (up to 12 chars + '\0') and *size.
   Returns 1 if an entry was found at index idx, 0 if there are no more. */
int fat16_readdir(uint32_t idx, char name[13], uint32_t *size);

#endif
