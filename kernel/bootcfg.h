// nullos/kernel/bootcfg.h — the boot configuration sector.
//
// A tiny key=value store that lives in ONE raw sector outside the
// filesystem (LBA 1, inside FAT16's reserved region), read and written only
// through the HAL's block I/O. It must not depend on FAT16, the VFS or the
// heap: Safe Mode relies on it exactly when those may be broken. One sector
// is also what makes a write atomic (it lands whole or not at all), which is
// why the file is kept small on purpose; if it ever needs more than a sector,
// that is the moment to solve atomic writes properly, not before.
//
// Format (text, NUL-padded to 512 bytes):
//     # nullos-config v1
//     boot_fail_count=0
// A sector that does not start with the magic line (a never-written disk,
// garbage, corruption) is read as an EMPTY config: every get returns its
// default. Only a valid config is ever written back.
//
// Availability guard: the sector is used only if the disk's boot sector says
// FAT16's reserved region is at least 2 sectors (sector 1 belongs to no
// filesystem structure). Otherwise the store is "unavailable": gets return
// defaults and writes fail, so a foreign or unformatted disk is never
// scribbled on.

#ifndef BOOTCFG_H
#define BOOTCFG_H

#include <stdint.h>

#define BOOTCFG_LBA 1

// The boot failure counter (see docs/safemode.md): incremented early in every
// boot, reset once the system is considered up. At BOOTCFG_FAIL_THRESHOLD
// consecutive failures the next boot enters Safe Mode.
#define BOOTCFG_KEY_FAIL_COUNT     "boot_fail_count"
#define BOOTCFG_FAIL_THRESHOLD     3

// Reads the config sector into memory. Returns:
//   1  a valid config was found
//   0  no valid config (empty/garbage sector): in-memory config is empty
//  -1  unavailable (no disk, read error, or the reserved-region guard failed)
int bootcfg_read(void);

// Writes the in-memory config to the sector. Returns 0 on success, -1 if the
// store is unavailable or the write failed. Call bootcfg_read() first.
int bootcfg_write(void);

// Value of `key` parsed as an unsigned decimal number, or `def` if the key is
// missing or not a number.
uint32_t bootcfg_get_u32(const char *key, uint32_t def);

// Copies the value of `key` into out (NUL-terminated, truncated to max).
// Returns its length, or -1 if the key is missing.
int bootcfg_get(const char *key, char *out, int max);

// Sets `key` to `value` in memory (not written until bootcfg_write()).
// Keys are [a-z0-9_]+; values must not contain a newline. Returns 0, or -1
// if the arguments are invalid or the sector would overflow.
int bootcfg_set(const char *key, const char *value);
int bootcfg_set_u32(const char *key, uint32_t value);

// Removes `key` from memory (not written until bootcfg_write()). Returns 1 if it
// was there, 0 if not, -1 for an invalid key.
int bootcfg_remove(const char *key);

// 1 if the last bootcfg_read() found the store usable (guard passed).
int bootcfg_is_available(void);

// The same text store on an EXPLICIT 512-byte buffer: no disk, no heap, no global
// state. The crash path (crashdump.c) builds the config sector in its own static
// buffer with these, so it never depends on the normal I/O path. Numbers may be
// stored decimal (bootcfg_buf_set_u32) or as "0x..." hex (bootcfg_buf_set_hex32);
// every u32 getter reads both.
void     bootcfg_buf_init(char *buf);                 // magic line + NULs
int      bootcfg_buf_valid(const char *buf);          // starts with the magic line?
int      bootcfg_buf_get(const char *buf, const char *key, char *out, int max);
uint32_t bootcfg_buf_get_u32(const char *buf, const char *key, uint32_t def);
int      bootcfg_buf_set(char *buf, const char *key, const char *value);
int      bootcfg_buf_set_u32(char *buf, const char *key, uint32_t value);
int      bootcfg_buf_set_hex32(char *buf, const char *key, uint32_t value);
int      bootcfg_buf_remove(char *buf, const char *key);

#endif // BOOTCFG_H
