// nullos/kernel/hal.c — x86 implementation of the HAL (see hal.h).
//
// Thin forwarding layer: each function calls the driver that already
// existed. No behavior of its own, so what a caller sees is unchanged from
// calling the driver directly.

#include "hal.h"
#include "drivers/vga.h"
#include "drivers/ata.h"
#include "keyboard.h"
#include "multiboot2.h"

#define MULTIBOOT2_BOOT_MAGIC 0x36d76289

// ── console ──────────────────────────────────────────────────────────
// console_color_t and vga_color_t share numeric values (see hal.h).

void console_putc(char c)                { vga_putchar(c); }
void console_puts(const char *str)       { vga_puts(str); }
void console_put_hex(uint32_t value)     { vga_puthex(value); }
void console_put_dec(uint32_t value)     { vga_putdec(value); }
void console_clear(void)                 { vga_clear(); }
void console_set_cursor(uint8_t col, uint8_t row) { vga_set_cursor(col, row); }

void console_set_color(console_color_t fg, console_color_t bg) {
    vga_set_color((vga_color_t)fg, (vga_color_t)bg);
}

// ── input ────────────────────────────────────────────────────────────

int  input_poll_key(void) { return keyboard_getchar_nowait(); }
int  input_poll_raw(void) { return keyboard_raw_nowait(); }
void input_flush(void)    { keyboard_flush(); }

// ── block device ─────────────────────────────────────────────────────

int block_read_sector(uint32_t lba, void *buf)        { return ata_read_sector(lba, buf); }
int block_write_sector(uint32_t lba, const void *buf) { return ata_write_sector(lba, buf); }

// ── boot information ─────────────────────────────────────────────────

static uintptr_t g_boot_info = 0;   // 0 = not initialized / invalid

int hal_boot_init(uint32_t boot_magic, uintptr_t boot_info) {
    if (boot_magic != MULTIBOOT2_BOOT_MAGIC) {
        g_boot_info = 0;
        return -1;
    }
    g_boot_info = boot_info;
    return 0;
}

int boot_get_cmdline(char *out, int max) {
    if (!g_boot_info || !out || max <= 0) return -1;

    mb2_tag_cmdline_t *tag = multiboot2_find_cmdline((void *)g_boot_info);
    if (!tag || tag->size <= sizeof(mb2_tag_t)) return -1;

    int room = (int)(tag->size - sizeof(mb2_tag_t));   // bytes of string in the tag
    int n = 0;
    while (n < room && n < max - 1 && tag->string[n]) {
        out[n] = tag->string[n];
        n++;
    }
    out[n] = '\0';
    return n;
}

int boot_has_flag(const char *flag) {
    char cmd[128];
    if (!flag || !*flag) return 0;
    int len = boot_get_cmdline(cmd, (int)sizeof(cmd));
    if (len <= 0) return 0;

    int i = 0;
    while (i < len) {
        while (i < len && cmd[i] == ' ') i++;      // skip separators
        int start = i;
        while (i < len && cmd[i] != ' ') i++;      // one word
        int wlen = i - start;
        if (wlen <= 0) continue;

        int j = 0;
        while (j < wlen && flag[j] && cmd[start + j] == flag[j]) j++;
        if (j == wlen && flag[j] == '\0') return 1;   // whole word equals the whole flag
    }
    return 0;
}

int boot_get_memory_map(boot_mem_region_t *out, int max) {
    if (!g_boot_info || !out || max <= 0) return -1;

    mb2_tag_mmap_t *tag = multiboot2_find_mmap((void *)g_boot_info);
    if (!tag || tag->entry_size < sizeof(mb2_mmap_entry_t)) return -1;

    uint8_t *p   = (uint8_t *)tag + sizeof(mb2_tag_mmap_t);
    uint8_t *end = (uint8_t *)tag + tag->size;
    int n = 0;

    while (p + tag->entry_size <= end && n < max) {
        const mb2_mmap_entry_t *e = (const mb2_mmap_entry_t *)p;
        out[n].base   = e->base_addr;
        out[n].length = e->length;
        out[n].type   = e->type;
        n++;
        p += tag->entry_size;
    }
    return n;
}
