// nullos/kernel/hal.h — hardware abstraction layer (Phase 18-A).
//
// Everything the rest of the kernel needs from "the machine" goes through
// these functions: text output, key input, block I/O, power control and
// the boot memory map. The signatures are pure C and architecture-neutral
// on purpose — nothing x86-specific (VGA cells, port I/O, PS/2 scancodes
// as a concept, Multiboot2 tags) appears here; that lives in hal.c and in
// the drivers behind it. A future port (arch/arm/, ...) implements this
// same interface its own way without touching any caller.
//
// This layer is deliberately thin: hal.c only forwards to the existing
// drivers (vga.c, keyboard.c, ata.c, power.c). See docs/hal.md.
//
// Not covered (deliberately, see docs/hal.md): driver bring-up
// (vga_init/keyboard_init/ata_init are called directly from kmain) and serial
// debug output. The exception handler in idt.c does use this layer: the HAL
// holds no state of its own, so it adds no risk there.

#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>

// power_reboot()/power_shutdown() already live in power.c with
// arch-neutral signatures; including the header is what makes them part of
// the HAL interface (no wrapper — a second function with the same name
// cannot exist).
#include "power.h"

// ── console (text output) ────────────────────────────────────────────
// Every character sent here is also mirrored to the serial port by the
// backend, exactly as before — callers must NOT write to serial again.

// Same 16-color palette and numeric values as the VGA text palette, so the
// x86 backend passes them straight through; another backend maps them.
typedef enum {
    CONSOLE_BLACK         = 0,
    CONSOLE_BLUE          = 1,
    CONSOLE_GREEN         = 2,
    CONSOLE_CYAN          = 3,
    CONSOLE_RED           = 4,
    CONSOLE_MAGENTA       = 5,
    CONSOLE_BROWN         = 6,
    CONSOLE_LIGHT_GREY    = 7,
    CONSOLE_DARK_GREY     = 8,
    CONSOLE_LIGHT_BLUE    = 9,
    CONSOLE_LIGHT_GREEN   = 10,
    CONSOLE_LIGHT_CYAN    = 11,
    CONSOLE_LIGHT_RED     = 12,
    CONSOLE_LIGHT_MAGENTA = 13,
    CONSOLE_YELLOW        = 14,
    CONSOLE_WHITE         = 15,
} console_color_t;

void console_putc(char c);
void console_puts(const char *str);
void console_put_hex(uint32_t value);   // "0x" + hex digits
void console_put_dec(uint32_t value);
void console_set_color(console_color_t fg, console_color_t bg);
void console_clear(void);
void console_set_cursor(uint8_t col, uint8_t row);

// ── input (keyboard) ─────────────────────────────────────────────────
// All non-blocking: the caller decides how to wait.

int  input_poll_key(void);   // next character, or -1 if none
int  input_poll_raw(void);   // next raw key event (bit 8 = Ctrl, bit 9 = Shift), or -1
void input_flush(void);      // drops everything buffered

// ── block device ─────────────────────────────────────────────────────
// 512-byte sectors, LBA addressing. 0 on success, -1 on error. A write
// that reached the media but whose final cache flush could not be
// confirmed still returns 0 (see docs/filesystem.md).

int block_read_sector(uint32_t lba, void *buf);
int block_write_sector(uint32_t lba, const void *buf);

// ── boot information ─────────────────────────────────────────────────

#define BOOT_MEM_USABLE       1
#define BOOT_MEM_RESERVED     2
#define BOOT_MEM_ACPI_RECLAIM 3
#define BOOT_MEM_ACPI_NVS     4
#define BOOT_MEM_BAD          5

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;   // BOOT_MEM_*
} boot_mem_region_t;

// Hands the HAL what the bootloader passed to the kernel entry point
// (x86: the Multiboot2 magic and info pointer). Called once, first thing
// in kmain, before boot_get_memory_map(). Returns 0 if the boot
// information is valid, -1 otherwise.
int hal_boot_init(uint32_t boot_magic, uintptr_t boot_info);

// Fills out[] with up to `max` physical memory regions and returns how
// many were written, or -1 if the bootloader gave no memory map. The boot
// information must still be readable (x86: the identity-mapped low memory
// where GRUB puts it).
int boot_get_memory_map(boot_mem_region_t *out, int max);

// The module the bootloader loaded next to the kernel (x86: the ramfs image
// GRUB passes with module2). Returns 1 and fills [*start, *end) (physical
// addresses) if there is one, 0 otherwise.
int boot_get_module(uint32_t *start, uint32_t *end);

// Where the bootloader's own information structure lives (x86: the Multiboot2
// info block). Returns 1 and fills *addr / *size, 0 if not initialized. A
// caller that hands out physical memory must keep this region reserved.
int boot_get_info_region(uint32_t *addr, uint32_t *size);

// Copies the boot command line (x86: the text after the kernel path in the
// GRUB entry) into out, NUL-terminated and truncated to max. Returns its
// length, or -1 if there is none (or boot info is not initialized).
int boot_get_cmdline(char *out, int max);

// True if `flag` is one of the whitespace-separated words of the boot command
// line (exact match, e.g. "safemode"). False if there is no command line.
int boot_has_flag(const char *flag);

#endif // HAL_H
