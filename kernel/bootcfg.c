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

static void cfg_reset(char *cfg) {
    for (int i = 0; i < SECTOR_SIZE; i++) cfg[i] = 0;
    for (int i = 0; i < MAGIC_LEN; i++) cfg[i] = MAGIC[i];
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
static int find_key(const char *cfg, const char *key, int *line_start, int *val_start, int *line_end) {
    int klen = str_len(key);
    int i = MAGIC_LEN;
    while (i < SECTOR_SIZE && cfg[i]) {
        int ls = i;
        while (i < SECTOR_SIZE && cfg[i] && cfg[i] != '\n') i++;
        int le = i;                       // at '\n' or NUL
        if (le - ls > klen && cfg[ls + klen] == '=') {
            int j = 0;
            while (j < klen && cfg[ls + j] == key[j]) j++;
            if (j == klen) {
                *line_start = ls; *val_start = ls + klen + 1; *line_end = le;
                return 1;
            }
        }
        if (i < SECTOR_SIZE && cfg[i] == '\n') i++;
    }
    return 0;
}

// Used bytes = everything up to the terminating NUL.
static int cfg_used(const char *cfg) {
    int n = MAGIC_LEN;
    while (n < SECTOR_SIZE && cfg[n]) n++;
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

// ── the text store on an explicit buffer ─────────────────────────────
//
// These work on any 512-byte buffer and touch NO disk, heap or global state,
// which is what lets the crash path (crashdump.c) build a config sector in its
// own static buffer without going through the normal I/O. The bootcfg_* API
// below is these functions applied to g_cfg.

void bootcfg_buf_init(char *buf) { cfg_reset(buf); }

int bootcfg_buf_valid(const char *buf) { return has_magic(buf); }

int bootcfg_buf_get(const char *buf, const char *key, char *out, int max) {
    int ls, vs, le;
    if (!buf || !key || !out || max <= 0 || !key_valid(key)) return -1;
    if (!find_key(buf, key, &ls, &vs, &le)) return -1;
    int n = 0;
    while (vs + n < le && n < max - 1) { out[n] = buf[vs + n]; n++; }
    out[n] = '\0';
    return n;
}

// Parses an unsigned number: decimal, or hexadecimal with a "0x" prefix.
static uint32_t parse_u32(const char *v, int n, uint32_t def) {
    uint32_t r = 0;
    int i = 0, base = 10;
    if (n > 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) { base = 16; i = 2; }
    for (; i < n; i++) {
        uint32_t d;
        if (v[i] >= '0' && v[i] <= '9') d = (uint32_t)(v[i] - '0');
        else if (base == 16 && v[i] >= 'a' && v[i] <= 'f') d = (uint32_t)(v[i] - 'a' + 10);
        else if (base == 16 && v[i] >= 'A' && v[i] <= 'F') d = (uint32_t)(v[i] - 'A' + 10);
        else return def;
        if (r > (0xFFFFFFFFu - d) / (uint32_t)base) return def;   // would overflow
        r = r * (uint32_t)base + d;
    }
    return r;
}

uint32_t bootcfg_buf_get_u32(const char *buf, const char *key, uint32_t def) {
    char v[16];
    int n = bootcfg_buf_get(buf, key, v, (int)sizeof(v));
    if (n <= 0) return def;
    return parse_u32(v, n, def);
}

int bootcfg_buf_set(char *buf, const char *key, const char *value) {
    if (!buf || !key || !value || !key_valid(key)) return -1;
    int vlen = str_len(value);
    for (int i = 0; i < vlen; i++) if (value[i] == '\n') return -1;

    int klen = str_len(key);
    int ls, vs, le;
    int have = find_key(buf, key, &ls, &vs, &le);

    // Size after the change: drop the old line (with its '\n'), add the new one.
    int used = cfg_used(buf);
    int old_len = have ? (le - ls) + (buf[le] == '\n' ? 1 : 0) : 0;
    int new_len = klen + 1 + vlen + 1;
    if (used - old_len + new_len > SECTOR_SIZE - 1) return -1;   // keep the NUL

    if (have) {                        // remove the old line by shifting the tail down
        int tail = ls + old_len;
        int i = 0;
        while (tail + i < SECTOR_SIZE) { buf[ls + i] = buf[tail + i]; i++; }
        while (ls + i < SECTOR_SIZE) buf[ls + i++] = 0;
    }

    int at = cfg_used(buf);            // append at the end
    for (int i = 0; i < klen; i++) buf[at++] = key[i];
    buf[at++] = '=';
    for (int i = 0; i < vlen; i++) buf[at++] = value[i];
    buf[at++] = '\n';
    buf[at] = '\0';
    return 0;
}

int bootcfg_buf_remove(char *buf, const char *key) {
    int ls, vs, le;
    if (!buf || !key || !key_valid(key)) return -1;
    if (!find_key(buf, key, &ls, &vs, &le)) return 0;    // nothing to remove
    int old_len = (le - ls) + (buf[le] == '\n' ? 1 : 0);
    int tail = ls + old_len, i = 0;
    while (tail + i < SECTOR_SIZE) { buf[ls + i] = buf[tail + i]; i++; }
    while (ls + i < SECTOR_SIZE) buf[ls + i++] = 0;
    return 1;
}

static void fmt_u32(char *out, uint32_t value, int hex) {
    char tmp[11];
    int i = 10, n = 0;
    tmp[i] = '\0';
    if (value == 0) tmp[--i] = '0';
    while (value) {
        uint32_t d = hex ? (value & 0xF) : (value % 10);
        tmp[--i] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
        value = hex ? (value >> 4) : (value / 10);
    }
    if (hex) { out[n++] = '0'; out[n++] = 'x'; }
    while (tmp[i]) out[n++] = tmp[i++];
    out[n] = '\0';
}

int bootcfg_buf_set_u32(char *buf, const char *key, uint32_t value) {
    char v[16];
    fmt_u32(v, value, 0);
    return bootcfg_buf_set(buf, key, v);
}

// Same, but stored as "0x..." hex (readable in the sector hexdump for
// addresses); bootcfg_get_u32() reads either form.
int bootcfg_buf_set_hex32(char *buf, const char *key, uint32_t value) {
    char v[16];
    fmt_u32(v, value, 1);
    return bootcfg_buf_set(buf, key, v);
}

// ── public API (the store in g_cfg, backed by the config sector) ─────

int bootcfg_read(void) {
    cfg_reset(g_cfg);
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

int bootcfg_get(const char *key, char *out, int max)       { return bootcfg_buf_get(g_cfg, key, out, max); }
uint32_t bootcfg_get_u32(const char *key, uint32_t def)    { return bootcfg_buf_get_u32(g_cfg, key, def); }
int bootcfg_set(const char *key, const char *value)        { return bootcfg_buf_set(g_cfg, key, value); }
int bootcfg_set_u32(const char *key, uint32_t value)       { return bootcfg_buf_set_u32(g_cfg, key, value); }
int bootcfg_remove(const char *key)                        { return bootcfg_buf_remove(g_cfg, key); }
