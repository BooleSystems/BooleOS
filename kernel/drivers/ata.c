/* nullos/kernel/drivers/ata.c — ATA PIO, tries all 4 slots. Sector I/O
   (ata_read_sector/ata_write_sector) waits for command completion via
   IRQ14/15 + the scheduler's block/wake primitive instead of busy-wait
   polling, whenever it's called from a scheduled process. probe() and
   the reset/detect path stay pure polling — that only ever runs once
   at boot, before there's anything else to schedule. */
#include "ata.h"
#include "../timer.h"
#include "../process.h"
#include "../scheduler.h"
#include "../idt.h"
#include "../pic.h"
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

/* ── IRQ-driven completion wait ──────────────────────────────
   Only one ATA command is ever in flight at a time (serialized by
   the exclusion gate below), so a single flag/waiter pair is enough
   — there is never more than one process legitimately waiting here. */
static volatile int  g_irq_fired  = 0;
static process_t    *g_irq_waiter = 0;

/* Short by design: just acknowledges the drive's IRQ line and flips
   the waiter (if any) back to READY. Never touches the scheduler or
   does a context switch, so it can't race with one being in progress. */
static void ata_irq_handler(uint32_t int_no) {
    (void)int_no;
    (void)inb(g_base + REG_STATUS);   /* reading Status clears the drive's IRQ line */
    g_irq_fired = 1;
    if (g_irq_waiter) {
        /* only wake it if it's still the same blocked wait we set up —
           if it was killed (and its slot possibly reused) while
           waiting, process_exit() already moved it out of BLOCKED */
        if (g_irq_waiter->state == PROCESS_BLOCKED)
            g_irq_waiter->state = PROCESS_READY;
        g_irq_waiter = 0;
    }
}

/* Registers the IRQ handler for whichever channel was detected and
   unmasks it (plus the master's cascade line IRQ2, required for any
   slave-PIC IRQ — 8-15 — to ever reach the CPU). channel: 0=primary
   (IRQ14/vector 46), 1=secondary (IRQ15/vector 47). */
static void ata_irq_init(int channel) {
    idt_register_handler(channel == 0 ? 46 : 47, ata_irq_handler);
    pic_unmask_irq(2);
    pic_unmask_irq(channel == 0 ? 14 : 15);
}

/* Waits for the ATA IRQ that signals the current command finished.
   Must only be called with a current process (the caller is
   responsible for falling back to polling otherwise — see the
   process_current() checks in ata_read_sector/ata_write_sector).

   Race-free by construction: the "did it already fire?" check and
   the "go to sleep" transition happen in the same cli/sti section,
   so an IRQ that arrives early (before we decided to block) is never
   missed — we just see the flag already set and skip blocking. */
static void ata_wait_irq(void) {
    process_t *self = process_current();
    if (!self) return;

    __asm__ volatile ("cli");
    if (g_irq_fired) {
        g_irq_fired = 0;
        __asm__ volatile ("sti");
        return;
    }
    g_irq_waiter = self;
    self->state  = PROCESS_BLOCKED;
    __asm__ volatile ("sti");

    scheduler_block_current();   /* resumes once ata_irq_handler wakes us */
    g_irq_fired = 0;             /* consume it for the next operation */
}

/* ── exclusion gate ───────────────────────────────────────────
   Serializes access to the (single, global) ATA controller state
   across processes: with IRQ-driven waits a process no longer holds
   the CPU for the whole operation, so a second process could
   otherwise issue a competing command on the same registers while
   the first is still waiting on its IRQ. Waiters block for real
   (no polling) and recheck the flag after being woken, since more
   than one waiter can be released at once and only one can win it. */
static volatile int g_ata_busy               = 0;
static process_t   *g_gate_waiters[PROCESS_MAX];
static uint32_t     g_gate_waiter_count      = 0;

static void ata_gate_acquire(void) {
    for (;;) {
        __asm__ volatile ("cli");
        if (!g_ata_busy) {
            g_ata_busy = 1;
            __asm__ volatile ("sti");
            return;
        }

        process_t *self = process_current();
        if (!self) {
            /* boot path: single-threaded at this point, so this is not
               expected to actually happen — but don't hot-spin if it does */
            __asm__ volatile ("sti; hlt");
            continue;
        }

        if (g_gate_waiter_count < PROCESS_MAX)
            g_gate_waiters[g_gate_waiter_count++] = self;
        self->state = PROCESS_BLOCKED;
        __asm__ volatile ("sti");

        scheduler_block_current();   /* woken by ata_gate_release(); recheck above */
    }
}

static void ata_gate_release(void) {
    __asm__ volatile ("cli");
    g_ata_busy = 0;
    for (uint32_t i = 0; i < g_gate_waiter_count; i++) {
        /* same guard as ata_irq_handler: don't resurrect a killed/reused slot */
        if (g_gate_waiters[i] && g_gate_waiters[i]->state == PROCESS_BLOCKED)
            g_gate_waiters[i]->state = PROCESS_READY;
    }
    g_gate_waiter_count = 0;
    __asm__ volatile ("sti");
}

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
                ata_irq_init(ch);
                return 1;
            }
        }
    }
    return 0;
}

int ata_read_sector(uint32_t lba, void *buf) {
    if (!g_present) return -1;
    ata_gate_acquire();

    if (wait_not_busy() < 0) { ata_gate_release(); return -1; }

    outb(g_base + REG_DRIVE_HEAD, g_lba_sel | ((lba >> 24) & 0x0F));
    outb(g_base + REG_SECCOUNT,   1);
    outb(g_base + REG_LBA_LO,     (uint8_t)(lba));
    outb(g_base + REG_LBA_MID,    (uint8_t)(lba >> 8));
    outb(g_base + REG_LBA_HI,     (uint8_t)(lba >> 16));
    outb(g_base + REG_CMD,        CMD_READ);

    /* READ SECTORS asserts an IRQ once the sector is ready (BSY=0,
       DRQ=1 on success). If we're running inside a scheduled process,
       sleep for that IRQ instead of polling; otherwise (early boot,
       before the scheduler runs anything) fall back to the original
       polling wait — there's no process to block/wake yet. */
    if (process_current()) {
        ata_wait_irq();
        uint8_t st = inb(g_base + REG_STATUS);
        if ((st & ATA_SR_ERR) || !(st & ATA_SR_DRQ)) { ata_gate_release(); return -1; }
    } else {
        ata_delay();
        if (wait_not_busy() < 0) { ata_gate_release(); return -1; }
        if (wait_drq()      < 0) { ata_gate_release(); return -1; }
    }

    uint16_t *dst = (uint16_t *)buf;
    for (int i = 0; i < 256; i++) dst[i] = inw(g_base + REG_DATA);

    ata_gate_release();
    return 0;
}

int ata_write_sector(uint32_t lba, const void *buf) {
    if (!g_present) return -1;
    ata_gate_acquire();

    if (wait_not_busy() < 0) { ata_gate_release(); return -1; }

    outb(g_base + REG_DRIVE_HEAD, g_lba_sel | ((lba >> 24) & 0x0F));
    outb(g_base + REG_SECCOUNT,   1);
    outb(g_base + REG_LBA_LO,     (uint8_t)(lba));
    outb(g_base + REG_LBA_MID,    (uint8_t)(lba >> 8));
    outb(g_base + REG_LBA_HI,     (uint8_t)(lba >> 16));
    outb(g_base + REG_CMD,        CMD_WRITE);
    ata_delay();

    /* WRITE SECTORS' initial "ready for data" transition (BSY->0,
       DRQ->1) is NOT IRQ-signaled per the ATA spec — the drive expects
       the host to already be watching for it, so this part always
       polls, with or without IRQ mode. It's a short, bounded wait. */
    if (wait_not_busy() < 0) { ata_gate_release(); return -1; }
    if (wait_drq()      < 0) { ata_gate_release(); return -1; }

    const uint16_t *src = (const uint16_t *)buf;
    for (int i = 0; i < 256; i++) outw(g_base + REG_DATA, src[i]);

    /* command completion (BSY=0), on the other hand, IS signaled by
       IRQ — this is the real, physically-written payload, BEFORE any
       flush. Issuing another command (FLUSH) while BSY is still set
       would be invalid per the ATA protocol. */
    if (process_current()) {
        ata_wait_irq();
    } else {
        if (wait_not_busy() < 0) { ata_gate_release(); return -1; }
    }
    if (inb(g_base + REG_STATUS) & ATA_SR_ERR) { ata_gate_release(); return -1; }

    /* cache flush: best-effort. The data is already confirmed
       written by the WRITE above (checked right before this); a
       timeout here only means the drive didn't confirm the cache
       was committed to media within the deadline — it does NOT
       mean the data is gone, so this is NOT propagated as a write
       failure. */
    outb(g_base + REG_CMD, CMD_FLUSH);
    if (process_current()) {
        ata_wait_irq();
    } else {
        wait_not_busy();
    }

    ata_gate_release();
    return 0;
}
