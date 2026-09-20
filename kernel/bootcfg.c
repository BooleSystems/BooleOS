// nullos/kernel/bootcfg.c — boot configuration sector (see bootcfg.h).

#include "bootcfg.h"
#include "hal.h"

#define SECTOR_SIZE 512

static const char MAGIC[] = "# nullos-config v1\n";
#define MAGIC_LEN ((int)(sizeof(MAGIC) - 1))

#define KEY_MAX 31

// In-memory copy of the sector: MAGIC, then key=value lines, then NULs.
// Always NUL-terminated (the last byte is never data).
static char g_cfg[SECTOR_SIZE];
static int  g_available = 0;

// ── tiny helpers (no libc in the kernel) ─────────────────────────────

static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

static void cfg_reset(void) {
    for (int i = 0; i < SECTOR_SIZE; i++) g_cfg[i] = 0;
    for (int i = 0; i < MAGIC_LEN; i++) g_cfg[i] = MAGIC[i];
}

static int has_magic(const char *buf) {
    for (int i = 0; i < MAGIC_LEN; i++)
        if (buf[i] != MAGIC[i]) return 0;
    return 1;
}

static int key_valid(const char *key) {
    int n = str_len(key);
    if (n == 0 || n > KEY_MAX) return 0;
    for (int i = 0; i < n; i++) {
        char c = key[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

// Finds `key` among the lines after the magic. On success returns 1 and sets
// *line_start/*line_end (line_end points at the '\n', or at the NUL) and
// *val_start to the first char after '='.
static int find_key(const char *key, int *line_start, int *val_start, int *line_end) {
    int klen = str_len(key);
    int i = MAGIC_LEN;
    while (i < SECTOR_SIZE && g_cfg[i]) {
        int ls = i;
        while (i < SECTOR_SIZE && g_cfg[i] && g_cfg[i] != '\n') i++;
        int le = i;                       // at '\n' or NUL
        if (le - ls > klen && g_cfg[ls + klen] == '=') {
            int j = 0;
            while (j < klen && g_cfg[ls + j] == key[j]) j++;
            if (j == klen) {
                *line_start = ls; *val_start = ls + klen + 1; *line_end = le;
                return 1;
            }
        }
        if (i < SECTOR_SIZE && g_cfg[i] == '\n') i++;
    }
    return 0;
}

// Used bytes = everything up to the terminating NUL.
static int cfg_used(void) {
    int n = MAGIC_LEN;
    while (n < SECTOR_SIZE && g_cfg[n]) n++;
    return n;
}

// ── availability guard ───────────────────────────────────────────────

// The disk's boot sector (LBA 0) must describe a reserved region of at least
// 2 sectors, so that LBA 1 is not part of the FAT. Field offsets (BPB):
// bytes/sector at 11, reserved sectors at 14.
static int guard_ok(void) {
    uint8_t s0[SECTOR_SIZE];
    if (block_read_sector(0, s0) < 0) return 0;
    uint32_t bps      = (uint32_t)s0[11] | ((uint32_t)s0[12] << 8);
    uint32_t reserved = (uint32_t)s0[14] | ((uint32_t)s0[15] << 8);
    return bps == SECTOR_SIZE && reserved > BOOTCFG_LBA;
}

// ── public API ───────────────────────────────────────────────────────

int bootcfg_read(void) {
    cfg_reset();
    g_available = 0;

    if (!guard_ok()) return -1;
    g_available = 1;

    char raw[SECTOR_SIZE];
    if (block_read_sector(BOOTCFG_LBA, raw) < 0) { g_available = 0; return -1; }

    if (!has_magic(raw)) return 0;     // never written, or garbage: empty config

    raw[SECTOR_SIZE - 1] = '\0';       // never trust the terminator
    for (int i = 0; i < SECTOR_SIZE; i++) g_cfg[i] = raw[i];
    return 1;
}

int bootcfg_is_available(void) { return g_available; }

int bootcfg_write(void) {
    if (!g_available) return -1;
    g_cfg[SECTOR_SIZE - 1] = '\0';
    return block_write_sector(BOOTCFG_LBA, g_cfg) < 0 ? -1 : 0;
}

int bootcfg_get(const char *key, char *out, int max) {
    int ls, vs, le;
    if (!key || !out || max <= 0 || !key_valid(key)) return -1;
    if (!find_key(key, &ls, &vs, &le)) return -1;
    int n = 0;
    while (vs + n < le && n < max - 1) { out[n] = g_cfg[vs + n]; n++; }
    out[n] = '\0';
    return n;
}

uint32_t bootcfg_get_u32(const char *key, uint32_t def) {
    char v[16];
    int n = bootcfg_get(key, v, (int)sizeof(v));
    if (n <= 0) return def;
    uint32_t r = 0;
    for (int i = 0; i < n; i++) {
        if (v[i] < '0' || v[i] > '9') return def;
        uint32_t d = (uint32_t)(v[i] - '0');
        if (r > (0xFFFFFFFFu - d) / 10) return def;   // would overflow
        r = r * 10 + d;
    }
    return r;
}

int bootcfg_set(const char *key, const char *value) {
    if (!key || !value || !key_valid(key)) return -1;
    int vlen = str_len(value);
    for (int i = 0; i < vlen; i++) if (value[i] == '\n') return -1;

    int klen = str_len(key);
    int ls, vs, le;
    int have = find_key(key, &ls, &vs, &le);

    // Size after the change: drop the old line (with its '\n'), add the new one.
    int used = cfg_used();
    int old_len = have ? (le - ls) + (g_cfg[le] == '\n' ? 1 : 0) : 0;
    int new_len = klen + 1 + vlen + 1;
    if (used - old_len + new_len > SECTOR_SIZE - 1) return -1;   // keep the NUL

    if (have) {                        // remove the old line by shifting the tail down
        int tail = ls + old_len;
        int i = 0;
        while (tail + i < SECTOR_SIZE) { g_cfg[ls + i] = g_cfg[tail + i]; i++; }
        while (ls + i < SECTOR_SIZE) g_cfg[ls + i++] = 0;
    }

    int at = cfg_used();               // append at the end
    for (int i = 0; i < klen; i++) g_cfg[at++] = key[i];
    g_cfg[at++] = '=';
    for (int i = 0; i < vlen; i++) g_cfg[at++] = value[i];
    g_cfg[at++] = '\n';
    g_cfg[at] = '\0';
    return 0;
}

int bootcfg_set_u32(const char *key, uint32_t value) {
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (value == 0) buf[--i] = '0';
    while (value) { buf[--i] = (char)('0' + value % 10); value /= 10; }
    return bootcfg_set(key, &buf[i]);
}
