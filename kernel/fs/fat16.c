/* nullos/kernel/fs/fat16.c — FAT16 read-only, on top of ATA PIO */
#include "fat16.h"
#include "../drivers/ata.h"
#include "../memory/heap.h"
#include <stdint.h>

/* ── BPB (BIOS Parameter Block) — fixed on-disk layout ─────── */
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_sectors;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} bpb_t;

/* ── FAT dir entry (32 bytes) ───────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attr;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t first_cluster;
    uint32_t size;
} fat16_dirent_t;

#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define DIRENT_EMPTY   0x00
#define DIRENT_DELETED 0xE5

/* ── global state ──────────────────────────────────────────── */
static int      g_ready             = 0;
static uint32_t g_fat_start_lba     = 0;
static uint32_t g_root_start_lba    = 0;
static uint32_t g_data_start_lba    = 0;
static uint32_t g_root_sector_count = 0;
static uint32_t g_sectors_per_cluster = 0;
static uint16_t *g_fat              = 0;   /* FAT cached in the heap */
static uint32_t  g_fat_size_sectors = 0;
static uint32_t  g_total_clusters   = 0;
static uint8_t   g_num_fats         = 0;

static uint8_t dir_buf[512];   /* separate buffer for dir entry operations */

/* ── helpers ────────────────────────────────────────────────── */

static uint8_t sector_buf[512];   /* working buffer (static) */

static uint8_t fat16_toupper(uint8_t c) {
    return (c >= 'a' && c <= 'z') ? (uint8_t)(c - 32) : c;
}

/* Converts "name.ext" → 11-byte FAT-style array (spaces, uppercase) */
static void to_8_3(const char *name, uint8_t out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';
    int i = 0;
    /* name */
    while (*name && *name != '.' && i < 8)
        out[i++] = fat16_toupper((uint8_t)*name++);
    /* extension */
    if (*name == '.') {
        name++;
        int k = 8;
        while (*name && k < 11)
            out[k++] = fat16_toupper((uint8_t)*name++);
    }
}

/* ── public API ────────────────────────────────────────────── */

int fat16_init(void) {
    g_ready = 0;

    if (ata_read_sector(0, sector_buf) < 0) return 0;

    bpb_t *bpb = (bpb_t *)sector_buf;

    /* minimal sanity check */
    if (bpb->bytes_per_sector != 512) return 0;
    if (bpb->fat_size_sectors == 0)   return 0;
    if (bpb->num_fats == 0)           return 0;

    g_sectors_per_cluster = bpb->sectors_per_cluster;
    g_num_fats            = bpb->num_fats;
    g_fat_start_lba       = bpb->reserved_sectors;
    g_fat_size_sectors    = bpb->fat_size_sectors;
    g_root_start_lba      = g_fat_start_lba + (uint32_t)bpb->num_fats * bpb->fat_size_sectors;
    g_root_sector_count   = ((uint32_t)bpb->root_entry_count * 32 + 511) / 512;
    g_data_start_lba      = g_root_start_lba + g_root_sector_count;
    {
        uint32_t total_sec = bpb->total_sectors_16 ? bpb->total_sectors_16
                                                    : bpb->total_sectors_32;
        g_total_clusters = (total_sec - g_data_start_lba) / g_sectors_per_cluster + 2;
    }

    /* cache the entire FAT in the heap */
    uint32_t fat_bytes = g_fat_size_sectors * 512;
    g_fat = (uint16_t *)kmalloc(fat_bytes);
    if (!g_fat) return 0;

    uint8_t *dst = (uint8_t *)g_fat;
    for (uint32_t s = 0; s < g_fat_size_sectors; s++) {
        if (ata_read_sector(g_fat_start_lba + s, dst + s * 512) < 0) {
            kfree(g_fat); g_fat = 0;
            return 0;
        }
    }

    g_ready = 1;
    return 1;
}

int fat16_available(void) { return g_ready; }

uint32_t fat16_bytes_per_cluster(void) {
    return g_sectors_per_cluster * 512;
}

uint32_t fat16_next_cluster(uint32_t cluster) {
    if (!g_fat || cluster < 2) return 0xFFFF;
    return g_fat[cluster];
}

uint32_t fat16_cluster_to_lba(uint32_t cluster) {
    return g_data_start_lba + (cluster - 2) * g_sectors_per_cluster;
}

/* returns 1=found, 0=doesn't exist (end of directory), -1=I/O error.
   A read error must NOT turn into "0" — that would make callers
   (fat16_create, vfs_open, vfs_create) treat a transient disk
   failure as "file doesn't exist" and create duplicate entries. */
int fat16_find(const char *name, uint32_t *first_cluster, uint32_t *size) {
    if (!g_ready || !name) return 0;

    uint8_t name83[11];
    to_8_3(name, name83);

    for (uint32_t s = 0; s < g_root_sector_count; s++) {
        if (ata_read_sector(g_root_start_lba + s, sector_buf) < 0) return -1;

        fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
        uint32_t per_sector = 512 / sizeof(fat16_dirent_t);

        for (uint32_t i = 0; i < per_sector; i++) {
            fat16_dirent_t *e = &entries[i];
            if (e->name[0] == DIRENT_EMPTY)   return 0;  /* end of dir */
            if (e->name[0] == DIRENT_DELETED)  continue;
            if (e->attr & (ATTR_VOLUME_ID | ATTR_DIRECTORY)) continue;

            /* name[8]+ext[3] are two separate fields in the struct —
               build the combined 11 bytes to compare and log,
               instead of indexing e->name[] past its declared 8
               bytes (that read garbage/UB at indices 8..10, making
               the match fail even for identical names) */
            uint8_t e_name83[11];
            for (int j = 0; j < 8; j++) e_name83[j]     = e->name[j];
            for (int j = 0; j < 3; j++) e_name83[8 + j] = e->ext[j];

            int match = 1;
            for (int j = 0; j < 11; j++) {
                if (e_name83[j] != name83[j]) { match = 0; break; }
            }
            if (match) {
                if (first_cluster) *first_cluster = e->first_cluster;
                if (size)          *size          = e->size;
                return 1;
            }
        }
    }
    return 0;
}

int fat16_readdir(uint32_t idx, char name[13], uint32_t *size) {
    if (!g_ready) return 0;

    uint32_t found = 0;
    for (uint32_t s = 0; s < g_root_sector_count; s++) {
        if (ata_read_sector(g_root_start_lba + s, sector_buf) < 0) return 0;

        fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
        uint32_t per_sector = 512 / sizeof(fat16_dirent_t);

        for (uint32_t i = 0; i < per_sector; i++) {
            fat16_dirent_t *e = &entries[i];
            if (e->name[0] == DIRENT_EMPTY)   return 0;
            if (e->name[0] == DIRENT_DELETED)  continue;
            if (e->attr & (ATTR_VOLUME_ID | ATTR_DIRECTORY)) continue;

            if (found == idx) {
                /* converts 8.3 back to "name.ext" */
                int p = 0;
                for (int j = 0; j < 8 && e->name[j] != ' '; j++)
                    name[p++] = (char)e->name[j];
                int has_ext = 0;
                for (int j = 0; j < 3; j++) if (e->ext[j] != ' ') { has_ext = 1; break; }
                if (has_ext) {
                    name[p++] = '.';
                    for (int j = 0; j < 3 && e->ext[j] != ' '; j++)
                        name[p++] = (char)e->ext[j];
                }
                name[p] = '\0';
                if (size) *size = e->size;
                return 1;
            }
            found++;
        }
    }
    return 0;
}

/* ── file creation ─────────────────────────────────────── */

int fat16_create(const char *name) {
    if (!g_ready || !name) return -1;

    uint8_t name83[11];
    to_8_3(name, name83);

    /* idempotent: if it already exists, that's not an error. If the
       check fails due to an I/O error, abort without writing anything
       — don't risk creating a duplicate entry from not knowing
       whether it already exists. */
    uint32_t fc, sz;
    int existing = fat16_find(name, &fc, &sz);
    if (existing == 1) return 0;
    if (existing < 0)  return -1;

    for (uint32_t s = 0; s < g_root_sector_count; s++) {
        if (ata_read_sector(g_root_start_lba + s, dir_buf) < 0) return -1;

        fat16_dirent_t *entries = (fat16_dirent_t *)dir_buf;
        uint32_t per_sector = 512 / sizeof(fat16_dirent_t);

        for (uint32_t i = 0; i < per_sector; i++) {
            fat16_dirent_t *e = &entries[i];
            if (e->name[0] == DIRENT_EMPTY || e->name[0] == DIRENT_DELETED) {
                for (int j = 0; j < 8; j++) e->name[j] = name83[j];
                for (int j = 0; j < 3; j++) e->ext[j]  = name83[8 + j];
                e->attr = ATTR_ARCHIVE;
                for (int j = 0; j < 10; j++) e->reserved[j] = 0;
                e->time = 0;
                e->date = 0;
                e->first_cluster = 0;
                e->size = 0;

                if (ata_write_sector(g_root_start_lba + s, dir_buf) < 0) return -1;
                return 0;
            }
        }
    }
    return -1;  /* root directory full */
}

/* ── write helpers ─────────────────────────────────────── */

static int fat16_flush_fat(void) {
    uint8_t *src = (uint8_t *)g_fat;
    for (uint8_t f = 0; f < g_num_fats; f++) {
        uint32_t base = g_fat_start_lba + (uint32_t)f * g_fat_size_sectors;
        for (uint32_t s = 0; s < g_fat_size_sectors; s++) {
            if (ata_write_sector(base + s, src + s * 512) < 0) return -1;
        }
    }
    return 0;
}

static void fat16_free_chain(uint32_t cluster) {
    while (cluster >= 2 && cluster < 0xFFF8) {
        uint32_t next = g_fat[cluster];
        g_fat[cluster] = 0x0000;
        cluster = next;
    }
}

/* allocates a free cluster, marks it as end-of-chain, returns the cluster or 0 */
static uint32_t fat16_alloc_cluster(void) {
    for (uint32_t c = 2; c < g_total_clusters; c++) {
        if (g_fat[c] == 0x0000) {
            g_fat[c] = 0xFFFF;
            return c;
        }
    }
    return 0;  /* disk full */
}

/* ── fat16_write_file ───────────────────────────────────────── */

int fat16_write_file(const char *name, const char *buf, uint32_t len) {
    if (!g_ready || !name || (!buf && len > 0)) return -1;

    uint8_t name83[11];
    to_8_3(name, name83);

    /* finds the dir entry and remembers its location */
    uint32_t dirent_sector = 0;
    uint32_t dirent_idx    = 0;
    int      found         = 0;

    for (uint32_t s = 0; s < g_root_sector_count && !found; s++) {
        if (ata_read_sector(g_root_start_lba + s, dir_buf) < 0) return -1;
        uint32_t per_sector = 512 / sizeof(fat16_dirent_t);
        for (uint32_t i = 0; i < per_sector; i++) {
            fat16_dirent_t *e = (fat16_dirent_t *)dir_buf + i;
            if (e->name[0] == DIRENT_EMPTY)   break;
            if (e->name[0] == DIRENT_DELETED)  continue;
            if (e->attr & (ATTR_VOLUME_ID | ATTR_DIRECTORY)) continue;
            /* name[8]+ext[3] are separate fields — same bug that was
               already fixed in fat16_find (this was a separate inline
               copy of the name search that got left behind): don't
               index e->name[8..10], build the 11 bytes by combining
               both fields. */
            uint8_t e_name83[11];
            for (int j = 0; j < 8; j++) e_name83[j]     = e->name[j];
            for (int j = 0; j < 3; j++) e_name83[8 + j] = e->ext[j];
            int match = 1;
            for (int j = 0; j < 11; j++)
                if (e_name83[j] != name83[j]) { match = 0; break; }
            if (match) {
                dirent_sector = s;
                dirent_idx    = i;
                found = 1;
                break;
            }
        }
    }
    if (!found) return -1;

    /* re-read the dir entry's sector into dir_buf (it may have changed) */
    if (ata_read_sector(g_root_start_lba + dirent_sector, dir_buf) < 0) return -1;
    fat16_dirent_t *entry = (fat16_dirent_t *)dir_buf + dirent_idx;

    /* free the old chain */
    fat16_free_chain(entry->first_cluster);

    /* allocate a new chain and write the data */
    uint32_t first_new = 0;
    uint32_t prev_cluster = 0;
    uint32_t written = 0;
    uint32_t bpc = g_sectors_per_cluster * 512;

    while (written < len) {
        uint32_t c = fat16_alloc_cluster();
        if (c == 0) return -1;  /* disk full */

        if (prev_cluster == 0) first_new = c;
        else                   g_fat[prev_cluster] = (uint16_t)c;
        g_fat[c] = 0xFFFF;     /* provisional end */
        prev_cluster = c;

        uint32_t lba = fat16_cluster_to_lba(c);
        uint32_t cluster_written = 0;

        for (uint32_t sec = 0; sec < g_sectors_per_cluster; sec++) {
            /* build the sector: copy data or zero-pad */
            for (uint32_t b = 0; b < 512; b++) {
                uint32_t pos = written + cluster_written + b;
                sector_buf[b] = (pos < len) ? (uint8_t)buf[pos] : 0;
            }
            if (ata_write_sector(lba + sec, sector_buf) < 0) return -1;
            cluster_written += 512;
        }
        written += (bpc < (len - written)) ? bpc : (len - written);
    }

    /* update the dir entry: first_cluster and size */
    entry->first_cluster = (len == 0) ? 0 : (uint16_t)first_new;
    entry->size          = len;
    if (ata_write_sector(g_root_start_lba + dirent_sector, dir_buf) < 0) return -1;

    /* flush the FAT to disk */
    return fat16_flush_fat();
}

int fat16_read_at(uint32_t first_cluster, uint32_t pos,
                  char *buf, uint32_t len, uint32_t file_size) {
    if (!g_ready || !buf || len == 0) return -1;
    if (pos >= file_size) return 0;
    if (pos + len > file_size) len = file_size - pos;

    uint32_t bpc = fat16_bytes_per_cluster();
    uint32_t read = 0;

    /* finds the cluster corresponding to byte pos */
    uint32_t cluster     = first_cluster;
    uint32_t cluster_idx = 0;
    uint32_t target_idx  = pos / bpc;

    while (cluster_idx < target_idx) {
        uint32_t next = fat16_next_cluster(cluster);
        if (next >= 0xFFF8) return (int)read;  /* premature end */
        cluster = next;
        cluster_idx++;
    }

    /* offset within the current cluster */
    uint32_t cluster_off = pos % bpc;

    while (read < len && cluster < 0xFFF8) {
        uint32_t lba = fat16_cluster_to_lba(cluster);

        /* sector within the cluster that contains cluster_off */
        uint32_t sec_idx = cluster_off / 512;
        uint32_t sec_off = cluster_off % 512;

        while (sec_idx < g_sectors_per_cluster && read < len) {
            if (ata_read_sector(lba + sec_idx, sector_buf) < 0) {
                return (int)read;
            }

            uint32_t avail = 512 - sec_off;
            uint32_t copy  = len - read;
            if (copy > avail) copy = avail;

            for (uint32_t i = 0; i < copy; i++)
                buf[read + i] = (char)sector_buf[sec_off + i];

            read     += copy;
            cluster_off += copy;
            sec_off   = 0;
            sec_idx++;
        }

        if (read < len) {
            cluster = fat16_next_cluster(cluster);
            cluster_off = 0;
        }
    }

    return (int)read;
}
