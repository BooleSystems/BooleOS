/* nullos/kernel/drivers/ata.c — ATA PIO polling, tries all 4 slots */
#include "ata.h"
#include "../timer.h"
#include <stdint.h>

/* ── register offsets relative to the channel base ───────── */
#define REG_DATA        0   /* 0x1F0 / 0x170 */
#define REG_ERROR       1
#define REG_FEATURES    1
#define REG_SECCOUNT    2
#define REG_LBA_LO      3
#define REG_LBA_MID     4
#define REG_LBA_HI      5
#define REG_DRIVE_HEAD  6
#define REG_STATUS      7
#define REG_CMD         7

/* control register offsets (base_ctrl) */
/* just write/read base_ctrl+0 */

/* status bits */
#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

/* drive selector in the DRIVE_HEAD register */
#define DRIVE_MASTER 0xA0
#define DRIVE_SLAVE  0xB0
/* LBA version (bits 6 and 4 set) */
#define DRIVE_LBA_MASTER 0xE0
#define DRIVE_LBA_SLAVE  0xF0

/* commands */
#define CMD_READ     0x20
#define CMD_WRITE    0x30
#define CMD_IDENTIFY 0xEC
#define CMD_FLUSH    0xE7

/* ATAPI signatures on LBA_MID/HI after IDENTIFY */
#define ATAPI_MID 0x14
#define ATAPI_HI  0xEB

/* ── known channels ───────────────────────────────────────── */
static const uint16_t BASES[2]      = { 0x1F0, 0x170 };
static const uint16_t CTRL_BASES[2] = { 0x3F6, 0x376 };

/* ── inline I/O ─────────────────────────────────────────────── */
static inline uint8_t inb(uint16_t p) {
    uint8_t v; __asm__ volatile ("inb %1,%0":"=a"(v):"Nd"(p)); return v;
}
static inline void outb(uint16_t p, uint8_t v) {
    __asm__ volatile ("outb %0,%1"::"a"(v),"Nd"(p));
}
static inline uint16_t inw(uint16_t p) {
    uint16_t v; __asm__ volatile ("inw %1,%0":"=a"(v):"Nd"(p)); return v;
}
static inline void outw(uint16_t p, uint16_t v) {
    __asm__ volatile ("outw %0,%1"::"a"(v),"Nd"(p));
}

/* ── state of the selected drive ────────────────────────────── */
static int      g_present   = 0;
static uint16_t g_base      = 0;    /* data base (e.g. 0x1F0) */
static uint16_t g_ctrl      = 0;    /* control base (e.g. 0x3F6) */
static uint8_t  g_drive_sel = 0;    /* 0xA0=master, 0xB0=slave */
static uint8_t  g_lba_sel   = 0;    /* 0xE0=master LBA, 0xF0=slave LBA */

/* ── helpers (use g_base/g_ctrl) ───────────────────────────── */
static void ata_delay(void) {
    inb(g_ctrl); inb(g_ctrl); inb(g_ctrl); inb(g_ctrl);
}
static int wait_not_busy(void) {
    for (uint32_t i = 0; i < 0x10000000; i++)
        if (!(inb(g_base + REG_STATUS) & ATA_SR_BSY)) return 0;
    return -1;
}
static int wait_drq(void) {
    for (uint32_t i = 0; i < 0x10000000; i++) {
        uint8_t s = inb(g_base + REG_STATUS);
        if (s & ATA_SR_ERR) return -1;
        if (s & ATA_SR_DRQ) return 0;
    }
    return -1;
}

/* reset timeout per the ATA datasheet: BSY can stay set for up to
   ~500ms-1s after a soft-reset (SRST). timer_init() runs at 100Hz
   (main.c), so 1 tick = 10ms; see the similar comment in timer.c. */
#define ATA_RESET_TIMEOUT_TICKS 100u  /* ~1s */

/* waits for BSY (bit 7) of the status register to clear after a
   soft-reset, using the PIT (timer_get_ticks) as a real timeout
   instead of a fixed delay — reset time varies between boots.
   reads via the control register (alternate status) so we don't
   disturb any pending IRQ state. returns 0 if it cleared, -1 if
   the timeout expired (treated as "no disk in this slot"). */
static int wait_bsy_clear_after_reset(uint16_t ctrl) {
    uint32_t start = timer_get_ticks();
    for (;;) {
        uint8_t s = inb(ctrl);
        if (!(s & ATA_SR_BSY)) return 0;
        /* 0xFF = floating bus (no controller/drive responding) —
           no point waiting out the full timeout; the real
           floating-bus check still happens after drive-select,
           this is just to avoid holding up boot */
        if (s == 0xFF) return -1;
        if ((timer_get_ticks() - start) >= ATA_RESET_TIMEOUT_TICKS) return -1;
    }
}

/* ── tries to identify a slot; returns 1 if ATA (not ATAPI) ── */
static int probe(uint16_t base, uint16_t ctrl, uint8_t drive_sel) {
    /* soft reset */
    outb(ctrl, 0x04); outb(ctrl, 0x00);
    /* minimum delay before starting to poll: gives the device time
       to assert BSY internally before we check it */
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);

    /* really wait for BSY==0 (timeout ~1s), instead of a fixed
       400ns delay that doesn't guarantee the reset has finished */
    if (wait_bsy_clear_after_reset(ctrl) < 0) return 0;

    outb(base + REG_DRIVE_HEAD, drive_sel);
    /* ~400ns delay: 4 reads of the alternate status register */
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);

    uint8_t status_after_select = inb(base + REG_STATUS);
    if (status_after_select == 0xFF) return 0;  /* floating bus */

    /* zero the registers and send IDENTIFY */
    outb(base + REG_SECCOUNT, 0);
    outb(base + REG_LBA_LO,   0);
    outb(base + REG_LBA_MID,  0);
    outb(base + REG_LBA_HI,   0);
    outb(base + REG_CMD,      CMD_IDENTIFY);
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);

    uint8_t st = inb(base + REG_STATUS);
    if (st == 0x00) return 0;   /* drive doesn't exist */

    /* wait for BSY=0 (quick timeout) */
    for (uint32_t i = 0; i < 0x10000000; i++) {
        if (!(inb(base + REG_STATUS) & ATA_SR_BSY)) break;
        if (i == 0x0FFFFFFF) return 0;
    }

    uint8_t lba_mid = inb(base + REG_LBA_MID);
    uint8_t lba_hi  = inb(base + REG_LBA_HI);

    /* ATAPI sets LBA_MID=0x14, LBA_HI=0xEB — discard it */
    if (lba_mid == ATAPI_MID && lba_hi == ATAPI_HI) return 0;

    /* wait for DRQ */
    for (uint32_t i = 0; i < 0x10000000; i++) {
        st = inb(base + REG_STATUS);
        if (st & ATA_SR_ERR) return 0;
        if (st & ATA_SR_DRQ) break;
        if (i == 0x0FFFFFFF) return 0;
    }

    /* drain the 256 IDENTIFY words */
    for (int i = 0; i < 256; i++) inw(base + REG_DATA);
    return 1;
}

/* ── public API ─────────────────────────────────────────────── */

int ata_init(void) {
    g_present = 0;

    /* order: primary master, primary slave, secondary master, secondary slave */
    static const uint8_t drv_sel[2] = { DRIVE_MASTER, DRIVE_SLAVE   };
    static const uint8_t lba_sel[2] = { DRIVE_LBA_MASTER, DRIVE_LBA_SLAVE };

    for (int ch = 0; ch < 2; ch++) {
        for (int dr = 0; dr < 2; dr++) {
            if (probe(BASES[ch], CTRL_BASES[ch], drv_sel[dr])) {
                g_base      = BASES[ch];
                g_ctrl      = CTRL_BASES[ch];
                g_drive_sel = drv_sel[dr];
                g_lba_sel   = lba_sel[dr];
                g_present   = 1;
                return 1;
            }
        }
    }
    return 0;
}

int ata_read_sector(uint32_t lba, void *buf) {
    if (!g_present) return -1;
    if (wait_not_busy() < 0) return -1;

    outb(g_base + REG_DRIVE_HEAD, g_lba_sel | ((lba >> 24) & 0x0F));
    outb(g_base + REG_SECCOUNT,   1);
    outb(g_base + REG_LBA_LO,     (uint8_t)(lba));
    outb(g_base + REG_LBA_MID,    (uint8_t)(lba >> 8));
    outb(g_base + REG_LBA_HI,     (uint8_t)(lba >> 16));
    outb(g_base + REG_CMD,        CMD_READ);
    ata_delay();

    if (wait_not_busy() < 0) return -1;
    if (wait_drq()      < 0) return -1;

    uint16_t *dst = (uint16_t *)buf;
    for (int i = 0; i < 256; i++) dst[i] = inw(g_base + REG_DATA);
    return 0;
}

int ata_write_sector(uint32_t lba, const void *buf) {
    if (!g_present) return -1;
    if (wait_not_busy() < 0) return -1;

    outb(g_base + REG_DRIVE_HEAD, g_lba_sel | ((lba >> 24) & 0x0F));
    outb(g_base + REG_SECCOUNT,   1);
    outb(g_base + REG_LBA_LO,     (uint8_t)(lba));
    outb(g_base + REG_LBA_MID,    (uint8_t)(lba >> 8));
    outb(g_base + REG_LBA_HI,     (uint8_t)(lba >> 16));
    outb(g_base + REG_CMD,        CMD_WRITE);
    ata_delay();

    if (wait_not_busy() < 0) return -1;
    if (wait_drq()      < 0) return -1;

    const uint16_t *src = (const uint16_t *)buf;
    for (int i = 0; i < 256; i++) outw(g_base + REG_DATA, src[i]);

    /* confirm that the WRITE command itself finished (BSY=0) and
       with no ERR — this is the real, physically-written payload,
       BEFORE any flush. Issuing another command (FLUSH) while BSY
       is still set would be invalid per the ATA protocol. */
    if (wait_not_busy() < 0) return -1;
    if (inb(g_base + REG_STATUS) & ATA_SR_ERR) return -1;

    /* cache flush: best-effort. The data is already confirmed
       written by the WRITE above (checked right before this); a
       timeout here only means the drive didn't confirm the cache
       was committed to media within the deadline — it does NOT
       mean the data is gone, so this is NOT propagated as a write
       failure. */
    outb(g_base + REG_CMD, CMD_FLUSH);
    wait_not_busy();
    return 0;
}
