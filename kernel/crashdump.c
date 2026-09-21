// nullos/kernel/crashdump.c — the crash path (see crashdump.h).

#include "crashdump.h"
#include "hal.h"
#include "bootcfg.h"
#include "power.h"
#include "timer.h"

#define K_PENDING "crash_pending"
#define K_TYPE    "crash_type"
#define K_EIP     "crash_eip"
#define K_ERR     "crash_err"
#define K_CR2     "crash_cr2"
#define K_TICKS   "crash_ticks"

// The sector being built. Static (not the stack: a stack overflow is one of the
// crashes this must survive) and not the heap (which may be what broke).
static char g_sector[512];

// Set by crash_begin(); never cleared: after a crash the only way out is the reset.
static volatile int g_crash_active = 0;

int crash_begin(void) {
    if (g_crash_active) return 0;
    g_crash_active = 1;
    return 1;
}

int crash_save(const crash_info_t *info) {
    // The config sector is usable only if the boot-time guard found it so
    // (a disk with a FAT16 reserved region): nowhere else is safe to write.
    if (!bootcfg_is_available()) return 0;

    // Start from the sector as it is on disk so boot_fail_count and any other
    // key survive; a sector without the magic line is a fresh config.
    if (block_read_sector_polled(BOOTCFG_LBA, g_sector) < 0 || !bootcfg_buf_valid(g_sector))
        bootcfg_buf_init(g_sector);
    g_sector[511] = '\0';

    if (bootcfg_buf_set_u32  (g_sector, K_PENDING, 1)             < 0 ||
        bootcfg_buf_set_u32  (g_sector, K_TYPE,    info->int_no)  < 0 ||
        bootcfg_buf_set_hex32(g_sector, K_EIP,     info->eip)     < 0 ||
        bootcfg_buf_set_hex32(g_sector, K_ERR,     info->err_code) < 0 ||
        bootcfg_buf_set_hex32(g_sector, K_CR2,     info->cr2)     < 0 ||
        bootcfg_buf_set_u32  (g_sector, K_TICKS,   info->ticks)   < 0)
        return 0;                                    // no room: better no record than a torn one

    return block_write_sector_polled(BOOTCFG_LBA, g_sector) == 0;
}

void crash_pause_ms(uint32_t ms) { timer_poll_delay_ms(ms); }

// With the IDT empty the CPU cannot deliver the interrupt below (nor the fault
// that raises, nor the double fault after it): a triple fault, which resets it.
static void triple_fault(void) {
    static const struct { uint16_t limit; uint32_t base; } __attribute__((packed)) empty_idt = { 0, 0 };
    __asm__ volatile ("lidt %0\n\tint3" : : "m"(empty_idt));
    for (;;) __asm__ volatile ("hlt");
}

void crash_reset(void) {
    __asm__ volatile ("cli");
    power_reboot_request();          // the 8042 reset pulse (what `reboot` uses)
    timer_poll_delay_ms(500);        // give it time, counted without IRQs
    triple_fault();                  // still here: force the reset
    for (;;) __asm__ volatile ("hlt");
}

// ── boot / Safe Mode side ────────────────────────────────────────────

uint32_t crash_record_load(crash_record_t *out) {
    uint32_t state = bootcfg_get_u32(K_PENDING, 0);
    if (state != 1 && state != 2) return 0;
    if (out) {
        out->state        = state;
        out->info.int_no  = bootcfg_get_u32(K_TYPE,  0);
        out->info.eip     = bootcfg_get_u32(K_EIP,   0);
        out->info.err_code = bootcfg_get_u32(K_ERR,  0);
        out->info.cr2     = bootcfg_get_u32(K_CR2,   0);
        out->info.ticks   = bootcfg_get_u32(K_TICKS, 0);
    }
    return state;
}

void crash_record_acknowledge(void) {
    if (bootcfg_get_u32(K_PENDING, 0) == 1) bootcfg_set_u32(K_PENDING, 2);
}

int crash_record_clear_if_acknowledged(void) {
    if (bootcfg_get_u32(K_PENDING, 0) != 2) return 0;
    bootcfg_remove(K_PENDING);
    bootcfg_remove(K_TYPE);
    bootcfg_remove(K_EIP);
    bootcfg_remove(K_ERR);
    bootcfg_remove(K_CR2);
    bootcfg_remove(K_TICKS);
    return 1;
}
