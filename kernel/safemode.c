// nullos/kernel/safemode.c — Safe Mode, tier 1 (see safemode.h).
//
// Tier 1 runs with nothing but the HAL and bootcfg: NO kmalloc, PMM, VMM,
// scheduler or anything that depends on them — it is entered before they
// exist. Every buffer is static or a fixed-size local. Tier 2 (menu item 5)
// initializes the PMM/VMM/heap/FAT16 on demand, once, when chosen. All text
// goes through msg().
//
// Screens: main menu (redrawn every time you come back to it), Reboot submenu,
// Disk info, Sector hexdump. There is deliberately no "continue booting":
// Safe Mode is left only by rebooting (in-place resume would mean trusting the
// very subsystems that may be why we are here).

#include "safemode.h"
#include "hal.h"
#include "bootcfg.h"
#include "messages.h"
#include "safeshell.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"
#include "fs/fat16.h"

#define KEY_ESC 27

static uint8_t  g_sector[512];   // scratch for raw sector reads (disk info, hexdump)
static boot_mem_region_t g_regions[64];   // memory map, for the tier-2 PMM init
static uint32_t g_entry_fail_count;
static safemode_reason_t g_reason;

// ── small helpers ────────────────────────────────────────────────────

// Blocks until a key is available. Interrupts are on, so hlt wakes on the
// keyboard IRQ (or the timer).
static int read_key(void) {
    for (;;) {
        int c = input_poll_key();
        if (c != -1) return c;
        __asm__ volatile ("hlt");
    }
}

static void wait_any_key(void) {
    console_puts(msg(MSG_SAFE_ANY_KEY));
    read_key();
}

static void put_hex_byte(uint8_t v) {
    static const char digits[] = "0123456789ABCDEF";
    console_putc(digits[v >> 4]);
    console_putc(digits[v & 0xF]);
}

static void put_hex_offset(uint32_t v) {   // 3 hex digits: enough for 0..511
    static const char digits[] = "0123456789ABCDEF";
    console_putc(digits[(v >> 8) & 0xF]);
    console_putc(digits[(v >> 4) & 0xF]);
    console_putc(digits[v & 0xF]);
}

static void put_title(msg_id_t title) {
    console_clear();
    console_set_color(CONSOLE_WHITE, CONSOLE_RED);
    console_puts(msg(MSG_SAFE_TITLE));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    console_puts(msg(title));
}

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// ── reboot actions ───────────────────────────────────────────────────

// Resets the failure counter, then reboots. Returns only if the reboot failed
// (power_reboot() prints why).
static void reboot_normally(void) {
    bootcfg_set_u32(BOOTCFG_KEY_FAIL_COUNT, 0);
    if (bootcfg_write() < 0)
        console_puts(msg(MSG_SAFE_RESET_FAILED));
    console_puts(msg(MSG_SAFE_REBOOTING));
    power_reboot();
    wait_any_key();
}

// Reboots WITHOUT touching the counter, so the next boot re-enters Safe Mode
// as long as the counter is at or above the limit.
static void reboot_keep_counter(void) {
    console_puts(msg(MSG_SAFE_REBOOTING));
    power_reboot();
    wait_any_key();
}

static void screen_reboot(void) {
    for (;;) {
        put_title(MSG_SAFE_REBOOT_TITLE);
        console_puts(msg(MSG_SAFE_REBOOT_1));
        console_puts(msg(MSG_SAFE_REBOOT_2));
        // GUI debug / Text mode entries join this submenu when the GUI exists (Phase 26).
        console_puts(msg(MSG_SAFE_REBOOT_BACK));
        if (bootcfg_get_u32(BOOTCFG_KEY_FAIL_COUNT, 0) < BOOTCFG_FAIL_THRESHOLD)
            console_puts(msg(MSG_SAFE_REBOOT_NOTE_LOW));

        int c = read_key();
        if (c == '1') reboot_normally();
        else if (c == '2') reboot_keep_counter();
        else if (c == '0' || c == KEY_ESC) return;
        // anything else: ignored, redraw
    }
}

// ── disk info ────────────────────────────────────────────────────────

static void put_label(msg_id_t label, uint32_t value) {
    console_puts(msg(label));
    console_put_dec(value);
    console_puts("\n");
}

// Prints `n` bytes of the sector starting at `off`, printable ASCII only
// (the extended boot record's label/type fields are space-padded text).
static void put_text_field(uint32_t off, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        uint8_t c = g_sector[off + i];
        console_putc((c >= 32 && c < 127) ? (char)c : '.');
    }
    console_puts("\n");
}

// Raw BPB fields only — FAT16 itself is not initialized (it needs the heap).
static void screen_disk_info(void) {
    put_title(MSG_SAFE_DISK_TITLE);

    if (block_read_sector(0, g_sector) < 0) {
        console_puts(msg(MSG_SAFE_DISK_READ_FAILED));
    } else {
        uint32_t total = le16(&g_sector[19]);
        if (total == 0) total = le32(&g_sector[32]);

        put_label(MSG_SAFE_DISK_BPS,      le16(&g_sector[11]));
        put_label(MSG_SAFE_DISK_SPC,      g_sector[13]);
        put_label(MSG_SAFE_DISK_RESERVED, le16(&g_sector[14]));
        put_label(MSG_SAFE_DISK_FATS,     g_sector[16]);
        put_label(MSG_SAFE_DISK_ROOT,     le16(&g_sector[17]));
        put_label(MSG_SAFE_DISK_TOTAL,    total);
        put_label(MSG_SAFE_DISK_SPF,      le16(&g_sector[22]));

        if (g_sector[38] == 0x29) {       // extended boot record signature
            console_puts(msg(MSG_SAFE_DISK_LABEL));  put_text_field(43, 11);
            console_puts(msg(MSG_SAFE_DISK_FSTYPE)); put_text_field(54, 8);
        } else {
            console_puts(msg(MSG_SAFE_DISK_NO_EXT));
        }

        console_puts(msg(MSG_SAFE_DISK_SIG));
        console_puts(msg((g_sector[510] == 0x55 && g_sector[511] == 0xAA)
                             ? MSG_SAFE_DISK_SIG_OK : MSG_SAFE_DISK_SIG_BAD));
    }

    console_puts(msg(MSG_SAFE_DISK_CFG));
    console_puts(msg(bootcfg_is_available() ? MSG_SAFE_DISK_CFG_YES : MSG_SAFE_DISK_CFG_NO));
    put_label(MSG_SAFE_DISK_COUNT, bootcfg_get_u32(BOOTCFG_KEY_FAIL_COUNT, 0));

    wait_any_key();
}

// ── sector hexdump ───────────────────────────────────────────────────

#define LBA_DIGITS_MAX 10   // 4294967295 has 10 digits

// Reads a decimal LBA from the keyboard. Returns 1 with *out set, or 0 if the
// user pressed ESC. Loops until the input is a valid number; empty, too large
// and non-digit input are rejected with a message.
static int read_lba(uint32_t *out) {
    static char buf[LBA_DIGITS_MAX + 1];
    int len = 0;

    console_puts(msg(MSG_SAFE_HEX_PROMPT));
    for (;;) {
        int c = read_key();

        if (c == KEY_ESC) return 0;

        if (c == '\b') {
            if (len > 0) { len--; console_putc('\b'); }
            continue;
        }

        if (c == '\n') {
            if (len == 0) {
                console_puts(msg(MSG_SAFE_HEX_EMPTY));
                console_puts(msg(MSG_SAFE_HEX_PROMPT));
                continue;
            }
            uint32_t v = 0;
            int ok = 1;
            for (int i = 0; i < len; i++) {
                uint32_t d = (uint32_t)(buf[i] - '0');
                if (v > (0xFFFFFFFFu - d) / 10) { ok = 0; break; }   // would overflow
                v = v * 10 + d;
            }
            if (!ok) {
                console_puts(msg(MSG_SAFE_HEX_TOO_BIG));
                len = 0;
                console_puts(msg(MSG_SAFE_HEX_PROMPT));
                continue;
            }
            *out = v;
            console_puts("\n");
            return 1;
        }

        if (c >= '0' && c <= '9') {
            if (len < LBA_DIGITS_MAX) { buf[len++] = (char)c; console_putc((char)c); }
            continue;
        }

        if (c >= 32 && c < 127) {           // a printable non-digit: say so, keep the input
            console_puts(msg(MSG_SAFE_HEX_DIGITS_ONLY));
            console_puts(msg(MSG_SAFE_HEX_PROMPT));
            for (int i = 0; i < len; i++) console_putc(buf[i]);
        }
        // other keys (arrows, function keys, ...): ignored
    }
}

// One page = 16 lines of 16 bytes (256 bytes); a sector is two pages because
// 512 bytes at 16 per line would not fit on the 25-line screen.
static void hexdump_page(uint32_t lba, uint32_t first_byte) {
    put_title(MSG_SAFE_HEX_TITLE);
    console_puts(msg(MSG_SAFE_HEX_SECTOR));
    console_put_dec(lba);
    console_puts(msg(MSG_SAFE_HEX_BYTES));
    console_put_dec(first_byte);
    console_puts("-");
    console_put_dec(first_byte + 255);
    console_puts("\n\n");

    for (uint32_t line = 0; line < 16; line++) {
        uint32_t off = first_byte + line * 16;
        put_hex_offset(off);
        console_puts("  ");
        for (uint32_t i = 0; i < 16; i++) {
            put_hex_byte(g_sector[off + i]);
            console_putc(' ');
        }
        console_putc(' ');
        for (uint32_t i = 0; i < 16; i++) {
            uint8_t c = g_sector[off + i];
            console_putc((c >= 32 && c < 127) ? (char)c : '.');
        }
        console_putc('\n');
    }
}

static void screen_hexdump(void) {
    for (;;) {
        put_title(MSG_SAFE_HEX_TITLE);
        uint32_t lba;
        if (!read_lba(&lba)) return;

        // The ATA driver is LBA28: a larger number would silently alias into
        // the low 28 bits, so refuse it instead of dumping the wrong sector.
        if (lba > 0x0FFFFFFFu || block_read_sector(lba, g_sector) < 0) {
            console_puts(msg(MSG_SAFE_HEX_READ_FAILED));
            wait_any_key();
            continue;                      // try another number
        }

        hexdump_page(lba, 0);
        wait_any_key();
        hexdump_page(lba, 256);
        wait_any_key();
        return;
    }
}

// ── tier 2: restricted shell ─────────────────────────────────────────

// Initialization progress. Each step runs at most once per Safe Mode session:
// the PMM/VMM/heap must not be initialized twice (a second vmm_init() would
// rebuild the page tables under a running kernel), so a step that failed part
// way is retried from the step that did not finish, never from the start.
enum { T2_NONE = 0, T2_PMM, T2_VMM, T2_HEAP };
static int g_t2_stage = T2_NONE;
static int g_t2_fat_ready = 0;

// Brings up what FAT16 needs, in the same order as the normal boot: PMM, VMM
// (paging), heap, then FAT16. (The scheduler is deliberately skipped — Safe
// Mode has no processes.) Returns 1 when everything is ready, 0 after printing
// which step failed.
static int tier2_init(void) {
    if (g_t2_stage < T2_PMM) {
        int n = boot_get_memory_map(g_regions, (int)(sizeof(g_regions) / sizeof(g_regions[0])));
        pmm_init(g_regions, n);

        // Keep the PMM from handing out what the bootloader put in RAM (same
        // reservations kmain makes).
        uint32_t a, b;
        if (boot_get_module(&a, &b))      pmm_mark_used(a, b - a);
        if (boot_get_info_region(&a, &b)) pmm_mark_used(a, b);

        if (pmm_free_pages() == 0) {
            console_puts(msg(MSG_SAFE_T2_PMM_FAILED));
            return 0;
        }
        g_t2_stage = T2_PMM;
    }
    if (g_t2_stage < T2_VMM) {
        vmm_init();                    // no return value: it works or the machine faults
        g_t2_stage = T2_VMM;
    }
    if (g_t2_stage < T2_HEAP) {
        heap_init();                   // returns void; an empty heap means it failed
        if (heap_free_bytes() == 0) {
            console_puts(msg(MSG_SAFE_T2_HEAP_FAILED));
            return 0;
        }
        g_t2_stage = T2_HEAP;
    }
    if (!g_t2_fat_ready) {
        if (!fat16_init()) {
            console_puts(msg(MSG_SAFE_T2_FAT_FAILED));
            return 0;
        }
        g_t2_fat_ready = 1;
    }
    return 1;
}

static void screen_shell(void) {
    put_title(MSG_SAFE_T2_TITLE);

    if (!(g_t2_stage == T2_HEAP && g_t2_fat_ready)) {
        console_puts(msg(MSG_SAFE_T2_INIT));
        if (!tier2_init()) {           // clear message printed: back to the menu
            wait_any_key();
            return;
        }
        console_puts(msg(MSG_SAFE_T2_READY));
    }

    safeshell_run();                   // returns on "back"
}

// ── main menu ────────────────────────────────────────────────────────

static void draw_main_menu(void) {
    console_clear();
    console_set_color(CONSOLE_WHITE, CONSOLE_RED);
    console_puts(msg(MSG_SAFE_TITLE));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);

    if (g_reason == SAFEMODE_REASON_FAIL_COUNT) {
        console_puts(msg(MSG_SAFE_REASON_COUNT_1));
        console_put_dec(g_entry_fail_count);
        console_puts(msg(MSG_SAFE_REASON_COUNT_2));
        console_put_dec(BOOTCFG_FAIL_THRESHOLD);
        console_puts(msg(MSG_SAFE_REASON_COUNT_3));
    } else {
        console_puts(msg(MSG_SAFE_REASON_REQUESTED));
        console_put_dec(g_entry_fail_count);
        console_puts("\n");
    }

    console_puts("\n");
    console_puts(msg(MSG_SAFE_MENU_1));
    console_puts(msg(MSG_SAFE_MENU_2));
    console_puts(msg(MSG_SAFE_MENU_3));
    console_puts(msg(MSG_SAFE_MENU_4));
    console_puts(msg(MSG_SAFE_MENU_5));
    console_puts(msg(MSG_SAFE_MENU_PROMPT));
}

void safemode_enter(safemode_reason_t reason, uint32_t fail_count) {
    g_reason = reason;
    g_entry_fail_count = fail_count;

    for (;;) {
        draw_main_menu();
        int c = read_key();
        switch (c) {
            case '1': reboot_normally(); break;   // returns only if the reboot failed
            case '2': screen_reboot();   break;
            case '3': screen_disk_info(); break;
            case '4': screen_hexdump();  break;
            case '5': screen_shell();    break;
            default: break;                       // invalid key: ignored, menu redrawn
        }
    }
}
