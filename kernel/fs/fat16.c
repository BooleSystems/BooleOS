/* nullos/kernel/fs/fat16.c — FAT16 read/write, subdirectories, over ATA PIO */
#include "fat16.h"
#include "../drivers/ata.h"
#include "../memory/heap.h"
#include "../drivers/vga.h"
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

/* ── helpers ────────────────────────────────────────────────── */

static uint8_t sector_buf[512];   /* working buffer (static): raw file data */
static uint8_t dir_buf[512];      /* working buffer (static): directory sectors —
                                      reused (never nested) by dir_lookup, dir_insert,
                                      fat16_readdir, fat16_mkdir and fat16_write_file.
                                      Kept as a module-level static rather than a
                                      per-function local: several of these functions
                                      build/zero a whole 512-byte sector at once, and
                                      this file runs on the small per-process kernel
                                      stack (PROCESS_STACK_SIZE, kernel/process.h) —
                                      stacking several such locals across a nested
                                      call chain (e.g. fat16_mkdir -> dir_insert) adds
                                      up fast on bare metal. Safe to share: every use
                                      writes the sector it needs and either returns or
                                      moves on before any other function touches it —
                                      none of these call chains need two live sectors
                                      of *this* buffer at once (fat16_write_file's own
                                      dirent-vs-payload split instead keeps sector_buf
                                      separate from dir_buf, since those two truly are
                                      needed at the same time — see that function). */

static uint8_t fat16_toupper(uint8_t c) {
    return (c >= 'a' && c <= 'z') ? (uint8_t)(c - 32) : c;
}

/* Converts "name.ext" → 11-byte FAT-style array (spaces, uppercase) */
static void to_8_3(const char *name, uint8_t out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';

    /* "." and ".." are reserved directory-navigation names, not a
       filename with an empty base and a "." extension — encode them
       directly instead of running them through the name/extension
       split below, which treats the very first '.' as the extension
       separator and would otherwise silently produce the WRONG 8.3
       bytes for both (all-spaces for ".", garbage for ".."). That
       would make dir_lookup() never find the "." / ".." entries
       fat16_mkdir writes with this same direct encoding, breaking
       "cd ." and "cd .." (and any path that walks through them)
       inside every subdirectory, not just at the root. */
    if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
        out[0] = '.';
        if (name[1] == '.') out[1] = '.';
        return;
    }

    int i = 0;
    /* base name: up to 8 chars, stopping early at '.' if there is one */
    while (*name && *name != '.' && i < 8)
        out[i++] = fat16_toupper((uint8_t)*name++);

    /* If the base name was longer than 8 characters, the loop above
       stopped at the 8-char cap with `name` sitting mid-string (e.g.
       on the '_' of "selftest_tmp.txt") instead of on the real '.' —
       it never got a chance to see whether an extension exists further
       along. Skip ahead to find it (or confirm there isn't one) before
       deciding: without this, two names that only differ after the
       8th character before a '.' (like "selftest_tmp.txt" and
       "selftest_dir", both "selftest" for their first 8 chars) would
       silently produce the SAME 8.3 name and collide on disk — the
       extension wasn't just truncated, it was never even looked for. */
    while (*name && *name != '.') name++;

    /* extension: up to 3 chars after the '.', if any */
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
    if (bpb->sectors_per_cluster == 0) {
        /* would divide by zero in the cluster count below and break every
           cluster -> LBA computation afterward; fail before any state is
           set. The caller prints "no FAT16 disk" right after this. */
        vga_puts("invalid BPB (sectors_per_cluster=0), ");
        return 0;
    }

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

/* ── write helpers (used by dir_insert/fat16_mkdir below, so they must
      come before those) ───────────────────────────────────────────── */

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

/* ── directory iteration (shared between the root and subdirectories) ──
   The root directory is a fixed-size region at a fixed LBA with no
   cluster chain and no "." / ".." entries. A normal subdirectory is an
   entry with ATTR_DIRECTORY whose first_cluster starts a regular FAT
   chain, exactly like a file's data — it grows the same way a file's
   chain does. dir_cluster == 0 means "the root"; anything else is a
   subdirectory's first_cluster. Every place in this file that needs to
   scan a directory's entries — find-by-name, listing, or find-a-free-
   slot-to-insert — goes through this single stepper, so "how do I walk
   this directory" exists in exactly one place. */
typedef struct {
    uint32_t dir_cluster;
    uint32_t root_sector;     /* next root sector index (root only) */
    uint32_t cluster;         /* current cluster (subdirectory only) */
    uint32_t cluster_sector;  /* next sector within the current cluster */
} dir_iter_t;

static void dir_iter_init(dir_iter_t *it, uint32_t dir_cluster) {
    it->dir_cluster    = dir_cluster;
    it->root_sector    = 0;
    it->cluster        = dir_cluster;
    it->cluster_sector = 0;
}

/* Loads the next 512-byte sector of dirents into buf. Returns 1 on
   success (*out_lba set, if non-NULL, to the sector's LBA — so a caller
   that needs to rewrite this exact sector can do so without re-deriving
   it), 0 when the directory has no more sectors (root: fixed region
   exhausted; subdirectory: end of the FAT chain), -1 on I/O error. */
static int dir_iter_next_sector(dir_iter_t *it, uint8_t *buf, uint32_t *out_lba) {
    if (it->dir_cluster == 0) {
        if (it->root_sector >= g_root_sector_count) return 0;
        uint32_t lba = g_root_start_lba + it->root_sector;
        if (ata_read_sector(lba, buf) < 0) return -1;
        if (out_lba) *out_lba = lba;
        it->root_sector++;
        return 1;
    }

    if (it->cluster < 2 || it->cluster >= 0xFFF8) return 0;
    if (it->cluster_sector >= g_sectors_per_cluster) {
        uint32_t next = fat16_next_cluster(it->cluster);
        if (next < 2 || next >= 0xFFF8) return 0;
        it->cluster = next;
        it->cluster_sector = 0;
    }
    uint32_t lba = fat16_cluster_to_lba(it->cluster) + it->cluster_sector;
    if (ata_read_sector(lba, buf) < 0) return -1;
    if (out_lba) *out_lba = lba;
    it->cluster_sector++;
    return 1;
}

/* Searches directory dir_cluster for an entry whose name matches
   name83 (files AND directories both match — the caller decides based
   on out_entry->attr). This is the ONLY place in the file that builds
   the combined 11-byte name to compare (name[8] and ext[3] are separate
   struct fields — indexing past name[7] to compare the extension inline
   is undefined behavior and was the root cause of a Phase 10 bug; see
   docs/filesystem.md). Returns 1=found (fills *out_entry / *out_lba /
   *out_index, whichever are non-NULL), 0=not found (end of directory),
   -1=I/O error. */
static int dir_lookup(uint32_t dir_cluster, const uint8_t name83[11],
                       fat16_dirent_t *out_entry, uint32_t *out_lba, uint32_t *out_index) {
    dir_iter_t it;
    dir_iter_init(&it, dir_cluster);
    uint32_t lba;
    int r;

    while ((r = dir_iter_next_sector(&it, dir_buf, &lba)) == 1) {
        fat16_dirent_t *entries = (fat16_dirent_t *)dir_buf;
        uint32_t per_sector = 512 / sizeof(fat16_dirent_t);

        for (uint32_t i = 0; i < per_sector; i++) {
            fat16_dirent_t *e = &entries[i];
            if (e->name[0] == DIRENT_EMPTY)   return 0;  /* end of dir */
            if (e->name[0] == DIRENT_DELETED)  continue;
            if (e->attr & ATTR_VOLUME_ID)      continue;

            uint8_t e_name83[11];
            for (int j = 0; j < 8; j++) e_name83[j]     = e->name[j];
            for (int j = 0; j < 3; j++) e_name83[8 + j] = e->ext[j];

            int match = 1;
            for (int j = 0; j < 11; j++) {
                if (e_name83[j] != name83[j]) { match = 0; break; }
            }
            if (match) {
                if (out_entry) *out_entry = *e;
                if (out_lba)   *out_lba   = lba;
                if (out_index) *out_index = i;
                return 1;
            }
        }
    }
    return r;  /* 0 (end of dir) or -1 (I/O error) */
}

/* Finds a free (or deleted) slot in dir_cluster and writes *entry into
   it, extending the subdirectory's cluster chain (freshly zeroed) if
   it's completely full — the root directory can't grow, so it still
   fails with -1 when full, matching the original fat16_create's
   behavior. This is the ONLY place that inserts a new dirent, shared by
   fat16_create and fat16_mkdir. */
static int dir_insert(uint32_t dir_cluster, const fat16_dirent_t *entry) {
    dir_iter_t it;
    dir_iter_init(&it, dir_cluster);
    uint32_t lba;
    int r;
    const uint8_t *entry_bytes = (const uint8_t *)entry;

    while ((r = dir_iter_next_sector(&it, dir_buf, &lba)) == 1) {
        fat16_dirent_t *entries = (fat16_dirent_t *)dir_buf;
        uint32_t per_sector = 512 / sizeof(fat16_dirent_t);

        for (uint32_t i = 0; i < per_sector; i++) {
            fat16_dirent_t *e = &entries[i];
            if (e->name[0] == DIRENT_EMPTY || e->name[0] == DIRENT_DELETED) {
                /* byte-for-byte copy instead of a whole-struct assignment
                   through the type-punned dir_buf pointer — GCC's
                   strict-aliasing analysis at -O2 flags that form as
                   undefined behavior (the object's effective type is
                   uint8_t[512]) even though field-by-field access
                   through the same cast (used elsewhere in this file,
                   matching the original pre-Phase-17 code) doesn't
                   trigger it. */
                uint8_t *dst = (uint8_t *)e;
                for (uint32_t b = 0; b < sizeof(fat16_dirent_t); b++) dst[b] = entry_bytes[b];
                if (ata_write_sector(lba, dir_buf) < 0) return -1;
                return 0;
            }
        }
    }
    if (r < 0) return -1;             /* I/O error mid-scan */
    if (dir_cluster == 0) return -1;  /* root directory full, can't grow */

    /* subdirectory is full: allocate + zero a new cluster, link it at
       the end of the chain, and write the entry as its first slot. The
       scan above already consumed dir_buf's old content, so reusing it
       here (first for the zero-fill, then for the new entry's sector)
       is safe. */
    uint32_t new_cluster = fat16_alloc_cluster();
    if (new_cluster == 0) return -1;  /* disk full */

    for (int i = 0; i < 512; i++) dir_buf[i] = 0;
    uint32_t new_lba = fat16_cluster_to_lba(new_cluster);
    for (uint32_t s = 0; s < g_sectors_per_cluster; s++) {
        if (ata_write_sector(new_lba + s, dir_buf) < 0) return -1;
    }

    for (uint32_t b = 0; b < sizeof(fat16_dirent_t); b++) dir_buf[b] = entry_bytes[b];
    if (ata_write_sector(new_lba, dir_buf) < 0) return -1;

    uint32_t last_cluster = it.cluster;   /* last cluster visited above */
    g_fat[last_cluster] = (uint16_t)new_cluster;
    g_fat[new_cluster]  = 0xFFFF;
    return fat16_flush_fat();
}

/* ── path resolution ──────────────────────────────────────────────── */

/* Splits the next '/'-separated component off path into comp (raw text,
   up to comp_size-1 bytes, NUL-terminated). Returns a pointer to what
   follows this component — an empty string if it was the last one.
   Assumes path is non-empty with no leading '/' (callers strip that). */
static const char *path_next_component(const char *path, char *comp, uint32_t comp_size) {
    uint32_t i = 0;
    while (*path && *path != '/' && i < comp_size - 1) comp[i++] = *path++;
    comp[i] = '\0';
    while (*path == '/') path++;
    return path;
}

/* Walks path component by component via dir_lookup(), stopping right
   before the LAST component. cwd_cluster is the base for a relative
   path (one that doesn't start with '/'); an absolute path always
   starts from the root regardless of cwd_cluster. On success (1),
   *out_parent_cluster is the cluster of the directory that directly
   contains the final component (0 = root) and out_final_name83 is its
   8.3-encoded name — the final component itself is deliberately NOT
   looked up here, since callers need it for different things
   (fat16_find looks it up as a file, fat16_create/fat16_mkdir insert
   it, fat16_resolve_dir requires it to already be a directory).
   Returns 0 if an intermediate component doesn't exist, isn't a
   directory, or the path has no final component at all (e.g. "", "/"),
   -1 on I/O error. */
static int resolve_path(uint32_t cwd_cluster, const char *path,
                         uint32_t *out_parent_cluster, uint8_t out_final_name83[11]) {
    /* Defensive zero-init: every return-0/-1 path below leaves these
       untouched, and callers must never read them on failure — but a
       future bug in a caller that forgets to check the return value
       should see 0/all-spaces instead of raw stack garbage. */
    if (out_parent_cluster) *out_parent_cluster = 0;
    if (out_final_name83)   for (int j = 0; j < 11; j++) out_final_name83[j] = ' ';

    if (!path) return 0;

    uint32_t dir = cwd_cluster;
    if (*path == '/') { dir = 0; path++; }
    while (*path == '/') path++;
    if (!*path) return 0;   /* "", "/", "///" — no final component */

    /* PATH_COMPONENT_MAX, not FAT's 8.3 on-disk limit (8+1+3=12): this
       holds a RAW path component before to_8_3() converts it, and a
       raw component can be longer than 12 characters (e.g.
       "selftest_sub.txt" — 16 chars, with a 12-char base name). Sizing
       this at 13 caused path_next_component() to truncate a long
       component at its length cap instead of at the next '/' — with
       no way to tell those two cases apart afterward, resolve_path()
       then misread the truncated remainder ("_sub.txt"'s tail) as an
       extra path component and looked it up as an intermediate
       directory that doesn't exist, failing the whole resolve. Sized
       to match kernel/syscall.c's USER_STR_MAX (64) — a component can
       never be longer than the whole path copied from userland. */
    char comp[64];
    path = path_next_component(path, comp, sizeof(comp));

    while (*path) {
        /* comp is an intermediate component: must exist and be a directory */
        uint8_t name83[11];
        to_8_3(comp, name83);
        fat16_dirent_t e;
        int r = dir_lookup(dir, name83, &e, 0, 0);
        if (r != 1) return r;
        if (!(e.attr & ATTR_DIRECTORY)) return 0;
        dir = e.first_cluster;

        path = path_next_component(path, comp, sizeof(comp));
    }

    if (out_parent_cluster) *out_parent_cluster = dir;
    if (out_final_name83)   to_8_3(comp, out_final_name83);
    return 1;
}

int fat16_resolve_dir(uint32_t cwd_cluster, const char *path, uint32_t *out_cluster) {
    if (!g_ready) return -1;
    if (!path || !*path || (path[0] == '/' && path[1] == '\0')) {
        if (out_cluster) *out_cluster = 0;
        return 1;
    }

    uint32_t parent = 0;
    uint8_t name83[11] = {0};
    int r = resolve_path(cwd_cluster, path, &parent, name83);
    if (r != 1) return r;

    fat16_dirent_t e;
    r = dir_lookup(parent, name83, &e, 0, 0);
    if (r != 1) return r;
    if (!(e.attr & ATTR_DIRECTORY)) return -2;

    if (out_cluster) *out_cluster = e.first_cluster;
    return 1;
}

/* ── lookup / listing ─────────────────────────────────────────────── */

int fat16_find(uint32_t dir_cluster, const char *path,
               uint32_t *first_cluster, uint32_t *size, uint32_t *out_parent_cluster) {
    if (!g_ready || !path) return 0;

    uint32_t parent = 0;
    uint8_t name83[11] = {0};
    int r = resolve_path(dir_cluster, path, &parent, name83);
    if (r != 1) return r;

    fat16_dirent_t e;
    r = dir_lookup(parent, name83, &e, 0, 0);
    if (r != 1) return r;
    if (e.attr & (ATTR_VOLUME_ID | ATTR_DIRECTORY)) return 0;  /* not a plain file */

    if (first_cluster)      *first_cluster      = e.first_cluster;
    if (size)               *size               = e.size;
    if (out_parent_cluster) *out_parent_cluster = parent;
    return 1;
}

int fat16_readdir(uint32_t dir_cluster, uint32_t idx, char name[13],
                   uint32_t *size, uint8_t *is_dir) {
    if (!g_ready) return 0;

    dir_iter_t it;
    dir_iter_init(&it, dir_cluster);
    uint32_t found = 0;
    int r;

    while ((r = dir_iter_next_sector(&it, dir_buf, 0)) == 1) {
        fat16_dirent_t *entries = (fat16_dirent_t *)dir_buf;
        uint32_t per_sector = 512 / sizeof(fat16_dirent_t);

        for (uint32_t i = 0; i < per_sector; i++) {
            fat16_dirent_t *e = &entries[i];
            if (e->name[0] == DIRENT_EMPTY)   return 0;
            if (e->name[0] == DIRENT_DELETED)  continue;
            if (e->attr & ATTR_VOLUME_ID)      continue;
            if (e->name[0] == '.')             continue;  /* skip "." / ".." */

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
                if (size)   *size   = e->size;
                if (is_dir) *is_dir = (e->attr & ATTR_DIRECTORY) ? 1 : 0;
                return 1;
            }
            found++;
        }
    }
    return 0;
}

/* ── file / directory creation ────────────────────────────────────── */

int fat16_create(uint32_t dir_cluster, const char *path, uint32_t *out_parent_cluster) {
    if (!g_ready || !path) return -1;

    uint32_t parent = 0;
    uint8_t name83[11] = {0};
    int r = resolve_path(dir_cluster, path, &parent, name83);
    if (r != 1) return -1;   /* intermediate component missing, or I/O error */

    /* idempotent: if a FILE already exists, that's not an error. A
       DIRECTORY of the same name is a real conflict, unlike before this
       phase (only files existed, so this case couldn't happen). If the
       check fails due to an I/O error, abort without writing anything —
       don't risk creating a duplicate entry from not knowing whether it
       already exists. */
    fat16_dirent_t existing;
    int found = dir_lookup(parent, name83, &existing, 0, 0);
    if (found == 1) {
        if (existing.attr & ATTR_DIRECTORY) return -1;
        if (out_parent_cluster) *out_parent_cluster = parent;
        return 0;
    }
    if (found < 0) return -1;

    fat16_dirent_t e;
    for (int j = 0; j < 8; j++) e.name[j] = name83[j];
    for (int j = 0; j < 3; j++) e.ext[j]  = name83[8 + j];
    e.attr = ATTR_ARCHIVE;
    for (int j = 0; j < 10; j++) e.reserved[j] = 0;
    e.time = 0;
    e.date = 0;
    e.first_cluster = 0;
    e.size = 0;

    if (dir_insert(parent, &e) < 0) return -1;

    if (out_parent_cluster) *out_parent_cluster = parent;
    return 0;
}

int fat16_mkdir(uint32_t dir_cluster, const char *path) {
    if (!g_ready || !path) return -1;

    uint32_t parent = 0;
    uint8_t name83[11] = {0};
    int r = resolve_path(dir_cluster, path, &parent, name83);
    if (r != 1) return -1;

    /* same idempotent/conflict policy as fat16_create, mirrored */
    fat16_dirent_t existing;
    int found = dir_lookup(parent, name83, &existing, 0, 0);
    if (found == 1) {
        if (existing.attr & ATTR_DIRECTORY) return 0;  /* idempotent */
        return -1;                                      /* collides with a file */
    }
    if (found < 0) return -1;

    uint32_t new_cluster = fat16_alloc_cluster();
    if (new_cluster == 0) return -1;  /* disk full */

    /* Zero the WHOLE new cluster before writing "." / ".." — a freshly
       allocated cluster carries whatever was on disk before (leftover
       file data, a stale directory, ...). Without this, anything past
       the two special entries could be misread as real dirents the
       first time this directory is scanned by dir_lookup/fat16_readdir. */
    for (int i = 0; i < 512; i++) dir_buf[i] = 0;
    uint32_t new_lba = fat16_cluster_to_lba(new_cluster);
    for (uint32_t s = 0; s < g_sectors_per_cluster; s++) {
        if (ata_write_sector(new_lba + s, dir_buf) < 0) return -1;
    }

    /* "." and ".." occupy the first two entries of the first sector —
       dir_buf is still all-zero here from the fill loop above, so only
       the two entries below need to be set explicitly. */
    fat16_dirent_t *entries = (fat16_dirent_t *)dir_buf;

    entries[0].name[0] = '.';
    for (int j = 1; j < 8; j++) entries[0].name[j] = ' ';
    for (int j = 0; j < 3; j++) entries[0].ext[j] = ' ';
    entries[0].attr = ATTR_DIRECTORY;
    entries[0].first_cluster = (uint16_t)new_cluster;
    entries[0].size = 0;

    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    for (int j = 2; j < 8; j++) entries[1].name[j] = ' ';
    for (int j = 0; j < 3; j++) entries[1].ext[j] = ' ';
    entries[1].attr = ATTR_DIRECTORY;
    entries[1].first_cluster = (uint16_t)parent;  /* 0 if parent is root */
    entries[1].size = 0;

    if (ata_write_sector(new_lba, dir_buf) < 0) return -1;

    fat16_dirent_t e;
    for (int j = 0; j < 8; j++) e.name[j] = name83[j];
    for (int j = 0; j < 3; j++) e.ext[j]  = name83[8 + j];
    e.attr = ATTR_DIRECTORY;
    for (int j = 0; j < 10; j++) e.reserved[j] = 0;
    e.time = 0;
    e.date = 0;
    e.first_cluster = (uint16_t)new_cluster;
    e.size = 0;

    if (dir_insert(parent, &e) < 0) {
        fat16_free_chain(new_cluster);
        return -1;
    }

    return fat16_flush_fat();
}

/* ── fat16_write_file ───────────────────────────────────────── */

int fat16_write_file(uint32_t parent_cluster, const char *name, const char *buf, uint32_t len) {
    if (!g_ready || !name || (!buf && len > 0)) return -1;

    uint8_t name83[11];
    to_8_3(name, name83);

    fat16_dirent_t existing;
    uint32_t lba, index;
    int found = dir_lookup(parent_cluster, name83, &existing, &lba, &index);
    if (found != 1) return -1;

    /* re-read the dir entry's sector (it may have changed since
       dir_lookup read it) — dir_lookup has already returned by this
       point, so reusing dir_buf here doesn't clobber anything it still
       needs. This sector's content (entry) must stay valid across the
       whole cluster-allocation loop below, which is why that loop uses
       the separate sector_buf for the actual file payload instead. */
    if (ata_read_sector(lba, dir_buf) < 0) return -1;
    fat16_dirent_t *entry = (fat16_dirent_t *)dir_buf + index;

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

        uint32_t clba = fat16_cluster_to_lba(c);
        uint32_t cluster_written = 0;

        for (uint32_t sec = 0; sec < g_sectors_per_cluster; sec++) {
            /* build the sector: copy data or zero-pad */
            for (uint32_t b = 0; b < 512; b++) {
                uint32_t pos = written + cluster_written + b;
                sector_buf[b] = (pos < len) ? (uint8_t)buf[pos] : 0;
            }
            if (ata_write_sector(clba + sec, sector_buf) < 0) return -1;
            cluster_written += 512;
        }
        written += (bpc < (len - written)) ? bpc : (len - written);
    }

    /* update the dir entry: first_cluster and size */
    entry->first_cluster = (len == 0) ? 0 : (uint16_t)first_new;
    entry->size          = len;
    if (ata_write_sector(lba, dir_buf) < 0) return -1;

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
