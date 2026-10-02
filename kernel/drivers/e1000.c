// booleos/kernel/drivers/e1000.c — Intel 8254x (e1000) network driver,
// Phase 24-A: raw Ethernet frames in and out, by polling. Protocols (ARP,
// later IP) live in kernel/net/.
//
// Concurrency: in 24-A everything here runs at boot, from kmain, in a single
// context, before the first process exists, so nothing guards the driver
// state (ring indexes, the TX buffer) or the register sequences; 24-B's ARP
// code also only calls it from kmain. The card's interrupts are all masked
// (IMC) and no IRQ handler is installed. Once a process or an IRQ handler
// can reach e1000_send()/e1000_poll_rx() (Phase 24-C), each call
// must run under irq_save()/irq_restore() or a lock: two callers interleaved
// by the timer would hand the card the same descriptor twice. That is the
// same mistake as the 0.22.1 VGA race.
//
// No struct is laid over hardware memory: descriptors are read and written
// as 32-bit words at fixed offsets.

#include "e1000.h"
#include "pci.h"
#include "../hal.h"
#include "../messages.h"
#include "../timer.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../serial.h"
#include <stdint.h>

/* ── supported devices: add a line to support another 8254x ─────────── */

static const struct { uint16_t vendor, device; } e1000_ids[] = {
    { 0x8086, 0x100E },   /* 82540EM, QEMU's default "e1000" */
};
#define E1000_NIDS (sizeof(e1000_ids) / sizeof(e1000_ids[0]))

/* ── registers (byte offsets into BAR0) ─────────────────────────────── */

#define REG_CTRL    0x0000
#define REG_STATUS  0x0008
#define REG_EERD    0x0014
#define REG_ICR     0x00C0
#define REG_IMC     0x00D8
#define REG_RCTL    0x0100
#define REG_TCTL    0x0400
#define REG_TIPG    0x0410
#define REG_RDBAL   0x2800
#define REG_RDBAH   0x2804
#define REG_RDLEN   0x2808
#define REG_RDH     0x2810
#define REG_RDT     0x2818
#define REG_TDBAL   0x3800
#define REG_TDBAH   0x3804
#define REG_TDLEN   0x3808
#define REG_TDH     0x3810
#define REG_TDT     0x3818
#define REG_MPC     0x4010   /* missed packets (no buffer / FIFO full), clear-on-read */
#define REG_GPRC    0x4074   /* good packets received, clear-on-read */
#define REG_RNBC    0x40A0   /* receive: no buffers available, clear-on-read */
#define REG_MTA     0x5200   /* 128 dwords, multicast table */
#define REG_RAL0    0x5400
#define REG_RAH0    0x5404

#define CTRL_LRST     (1u << 3)
#define CTRL_ASDE     (1u << 5)
#define CTRL_SLU      (1u << 6)
#define CTRL_ILOS     (1u << 7)
#define CTRL_RST      (1u << 26)
#define CTRL_VME      (1u << 30)
#define CTRL_PHY_RST  (1u << 31)

#define STATUS_LU     (1u << 1)

#define EERD_START    (1u << 0)
#define EERD_DONE     (1u << 4)   /* 82540EM position; other parts use bit 1 */

#define RAH_AV        (1u << 31)

#define RCTL_EN       (1u << 1)
#define RCTL_BAM      (1u << 15)
#define RCTL_BSIZE_2048 0u        /* BSIZE = 00 with BSEX = 0 */
#define RCTL_SECRC    (1u << 26)

#define TCTL_EN       (1u << 1)
#define TCTL_PSP      (1u << 3)
#define TCTL_CT(x)    ((uint32_t)(x) << 4)
#define TCTL_COLD(x)  ((uint32_t)(x) << 12)

#define TIPG_COPPER   0x0060200Au   /* IPGT 10, IPGR1 8, IPGR2 6 */

/* Descriptor fields. TX: dword 2 = length | CSO << 16 | CMD << 24,
   dword 3 = STA | CSS << 8 | special << 16. RX: dword 2 = length |
   checksum << 16, dword 3 = status | errors << 8 | special << 16. */
#define TXD_CMD_EOP   0x01
#define TXD_CMD_IFCS  0x02
#define TXD_CMD_RS    0x08
#define TXD_STA_DD    0x01
#define RXD_STA_DD    0x01
#define RXD_STA_EOP   0x02

#define NDESC         16           /* per ring; 16 * 16 bytes = 256 bytes */
#define DESC_DWORDS   4
#define RX_BUF_SIZE   2048
#define TX_BUF_SIZE   2048
#define RX_PAGES      (NDESC * RX_BUF_SIZE / PAGE_SIZE)   /* 8 */

#define MMIO_SIZE     0x20000u     /* 128 KB register window */
/* The registers are mapped at virt == phys in the kernel directory. That
   only works far away from user space (ELF images below 0x02000000, the
   stack right above it); QEMU's BARs sit near 4 GB. */
#define MMIO_MIN_ADDR 0x10000000u

/* Timeouts, in PIT ticks (100 Hz). SPIN_CAP bounds a wait even if the PIT
   stopped counting, so no wait can hang the boot (the Safe Mode boot
   counter would read that as a failed boot). */
#define T_RESET       10
#define T_EEPROM      5
#define T_LINK        100
#define T_TX          10
#define SPIN_CAP_PER_TICK 100000u

/* ── state ──────────────────────────────────────────────────────────── */

static volatile uint32_t *g_mmio = 0;
static uint32_t g_mmio_phys = 0;
static uint32_t g_mmio_mapped = 0;   /* bytes mapped, for the unwind */
static int      g_ready = 0;
static uint8_t  g_mac[6];

static uint32_t g_rx_ring = 0;            /* physical == virtual (PMM frame) */
static uint32_t g_tx_ring = 0;
static uint32_t g_rx_buf[RX_PAGES];       /* each page holds two RX buffers */
static uint32_t g_tx_buf = 0;
static uint32_t g_rx_next = 0;            /* next RX descriptor to look at */
static uint32_t g_tx_tail = 0;
static uint32_t g_tx_count = 0;           /* frames handed to the card */
static uint32_t g_rx_enable_tick = 0;     /* timer tick of the RCTL.EN write */

static uint32_t e1000_rd32(uint32_t reg) {
    return g_mmio[reg / 4];
}

static void e1000_wr32(uint32_t reg, uint32_t val) {
    g_mmio[reg / 4] = val;
}

static volatile uint32_t *desc(uint32_t ring, uint32_t i) {
    return (volatile uint32_t *)(ring + i * DESC_DWORDS * 4);
}

static uint32_t rx_buf_addr(uint32_t i) {
    return g_rx_buf[i / 2] + (i % 2) * RX_BUF_SIZE;
}

static void zero_page(uint32_t addr) {
    volatile uint32_t *p = (volatile uint32_t *)addr;
    for (uint32_t i = 0; i < PAGE_SIZE / 4; i++) p[i] = 0;
}

/* Waits until (register & mask) == want, up to `ticks` PIT ticks. Returns 1
   if the condition was met, 0 on timeout. */
static int wait_reg(uint32_t reg, uint32_t mask, uint32_t want, uint32_t ticks) {
    uint32_t start = timer_get_ticks();
    uint32_t cap = ticks * SPIN_CAP_PER_TICK;
    for (uint32_t spins = 0; spins < cap; spins++) {
        if ((e1000_rd32(reg) & mask) == want) return 1;
        if (timer_get_ticks() - start >= ticks) break;
    }
    return (e1000_rd32(reg) & mask) == want;
}

static void put_hex8(uint8_t v) {
    static const char hex[] = "0123456789abcdef";
    console_putc(hex[v >> 4]);
    console_putc(hex[v & 0xF]);
}

static void put_mac(const uint8_t *m) {
    for (int i = 0; i < 6; i++) {
        if (i) console_putc(':');
        put_hex8(m[i]);
    }
}

static void net_tag(void) {
    console_set_color(CONSOLE_WHITE, CONSOLE_BLACK);
    console_puts(msg(MSG_NET_TAG));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
}

/* ── init / teardown ────────────────────────────────────────────────── */

/* Gives back everything init took so far and leaves the card quiet. */
static void e1000_release(void) {
    if (g_mmio) {
        e1000_wr32(REG_RCTL, 0);
        e1000_wr32(REG_TCTL, 0);
        e1000_wr32(REG_IMC, 0xFFFFFFFFu);
    }
    for (uint32_t i = 0; i < RX_PAGES; i++)
        if (g_rx_buf[i]) { pmm_free_page(g_rx_buf[i]); g_rx_buf[i] = 0; }
    if (g_tx_buf)  { pmm_free_page(g_tx_buf);  g_tx_buf = 0; }
    if (g_rx_ring) { pmm_free_page(g_rx_ring); g_rx_ring = 0; }
    if (g_tx_ring) { pmm_free_page(g_tx_ring); g_tx_ring = 0; }
    /* The page table holding these PTEs stays: it belongs to the static
       kernel pool, and the PDE is already in the kernel directory. */
    for (uint32_t off = 0; off < g_mmio_mapped; off += PAGE_SIZE)
        vmm_unmap_page(g_mmio_phys + off);
    g_mmio_mapped = 0;
    g_mmio = 0;
    g_ready = 0;
}

static int init_fail(msg_id_t why) {
    e1000_release();
    net_tag();
    console_set_color(CONSOLE_LIGHT_RED, CONSOLE_BLACK);
    console_puts(msg(MSG_NET_INIT_FAILED));
    console_puts(msg(why));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
    return 0;
}

/* Reads one 16-bit EEPROM word through EERD. Returns 1 on success. */
static int eeprom_read(uint8_t addr, uint16_t *out) {
    e1000_wr32(REG_EERD, ((uint32_t)addr << 8) | EERD_START);
    if (!wait_reg(REG_EERD, EERD_DONE, EERD_DONE, T_EEPROM)) return 0;
    *out = (uint16_t)(e1000_rd32(REG_EERD) >> 16);
    return 1;
}

static int read_mac(void) {
    uint32_t ral = e1000_rd32(REG_RAL0);
    uint32_t rah = e1000_rd32(REG_RAH0);
    if (rah & RAH_AV) {
        for (int i = 0; i < 4; i++) g_mac[i] = (uint8_t)(ral >> (8 * i));
        g_mac[4] = (uint8_t)rah;
        g_mac[5] = (uint8_t)(rah >> 8);
        return 1;
    }
    /* Receive address not loaded: words 0-2 of the EEPROM hold the MAC,
       low byte first. Program RAL0/RAH0 with it so the card accepts
       unicast frames for this address. */
    for (uint8_t w = 0; w < 3; w++) {
        uint16_t v;
        if (!eeprom_read(w, &v)) return 0;
        g_mac[w * 2]     = (uint8_t)v;
        g_mac[w * 2 + 1] = (uint8_t)(v >> 8);
    }
    e1000_wr32(REG_RAL0, (uint32_t)g_mac[0] | ((uint32_t)g_mac[1] << 8) |
                         ((uint32_t)g_mac[2] << 16) | ((uint32_t)g_mac[3] << 24));
    e1000_wr32(REG_RAH0, (uint32_t)g_mac[4] | ((uint32_t)g_mac[5] << 8) | RAH_AV);
    return 1;
}

static int setup_rings(void) {
    g_rx_ring = pmm_alloc_page();
    g_tx_ring = pmm_alloc_page();
    g_tx_buf  = pmm_alloc_page();
    if (!g_rx_ring || !g_tx_ring || !g_tx_buf) return 0;
    for (uint32_t i = 0; i < RX_PAGES; i++) {
        g_rx_buf[i] = pmm_alloc_page();
        if (!g_rx_buf[i]) return 0;
    }
    /* PMM frames are below 8 MB, inside the identity map: the address the
       kernel writes through is the bus address the card uses. One page per
       ring keeps each ring 16-byte aligned (the card needs that) and
       inside a single page. */
    zero_page(g_rx_ring);
    zero_page(g_tx_ring);

    for (uint32_t i = 0; i < NDESC; i++) {
        volatile uint32_t *d = desc(g_rx_ring, i);
        d[0] = rx_buf_addr(i);
        d[1] = 0;
        d[2] = 0;
        d[3] = 0;
    }
    e1000_wr32(REG_RDBAL, g_rx_ring);
    e1000_wr32(REG_RDBAH, 0);
    e1000_wr32(REG_RDLEN, NDESC * DESC_DWORDS * 4);
    e1000_wr32(REG_RDH, 0);
    e1000_wr32(REG_RDT, NDESC - 1);   /* every descriptor but one belongs to the card */
    g_rx_next = 0;
    e1000_wr32(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC);
    g_rx_enable_tick = timer_get_ticks();

    /* Every TX descriptor points at the one TX buffer: e1000_send() waits
       for each frame to go out before it returns, so at most one is in
       flight. */
    for (uint32_t i = 0; i < NDESC; i++) {
        volatile uint32_t *d = desc(g_tx_ring, i);
        d[0] = g_tx_buf;
        d[1] = 0;
        d[2] = 0;
        d[3] = TXD_STA_DD;   /* "done": free for the driver */
    }
    e1000_wr32(REG_TDBAL, g_tx_ring);
    e1000_wr32(REG_TDBAH, 0);
    e1000_wr32(REG_TDLEN, NDESC * DESC_DWORDS * 4);
    e1000_wr32(REG_TDH, 0);
    e1000_wr32(REG_TDT, 0);
    g_tx_tail = 0;
    e1000_wr32(REG_TIPG, TIPG_COPPER);
    e1000_wr32(REG_TCTL, TCTL_EN | TCTL_PSP | TCTL_CT(0x10) | TCTL_COLD(0x40));
    return 1;
}

int e1000_init(void) {
    uint8_t bus = 0, dev = 0, fn = 0;
    int found = 0;
    for (uint32_t i = 0; i < E1000_NIDS && !found; i++)
        found = pci_find_device(e1000_ids[i].vendor, e1000_ids[i].device, &bus, &dev, &fn);
    if (!found) {
        net_tag();
        console_puts(msg(MSG_NET_NO_E1000));
        return 0;
    }

    /* BAR0: 32-bit memory BAR (bit 0 = 0, type bits 2:1 = 00). */
    uint32_t bar0 = pci_config_read32(bus, dev, fn, 0x10);
    uint32_t size = pci_bar_size(bus, dev, fn, 0);
    uint32_t base = bar0 & 0xFFFFFFF0u;
    if ((bar0 & 0x7) != 0 || base < MMIO_MIN_ADDR || size < MMIO_SIZE ||
        base > 0xFFFFFFFFu - MMIO_SIZE + 1)
        return init_fail(MSG_NET_ERR_BAR);

    /* Memory Space (decode BAR0) and Bus Master (the card reads and writes
       the rings and buffers by DMA; without it nothing ever moves). The
       other command bits are kept. */
    uint16_t cmd = pci_config_read16(bus, dev, fn, 0x04);
    pci_config_write16(bus, dev, fn, 0x04,
                       (uint16_t)(cmd | PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER));

    /* Uncached (PCD|PWT): a cached register read would return a stale value
       and a write could sit in the cache. Kernel only, no VMM_USER. */
    g_mmio_phys = base;
    for (uint32_t off = 0; off < MMIO_SIZE; off += PAGE_SIZE) {
        if (vmm_map_page(base + off, base + off, VMM_KERNEL | VMM_PCD | VMM_PWT) != 0)
            return init_fail(MSG_NET_ERR_MAP);
        g_mmio_mapped = off + PAGE_SIZE;
    }
    g_mmio = (volatile uint32_t *)base;

    /* Reset. Interrupts masked before and after (the reset re-enables
       nothing, but a stale cause must not stay latched). */
    e1000_wr32(REG_IMC, 0xFFFFFFFFu);
    e1000_wr32(REG_CTRL, e1000_rd32(REG_CTRL) | CTRL_RST);
    timer_poll_delay_ms(1);   /* the card ignores register access for ~1 us after RST */
    if (!wait_reg(REG_CTRL, CTRL_RST, 0, T_RESET))
        return init_fail(MSG_NET_ERR_RESET);
    e1000_wr32(REG_IMC, 0xFFFFFFFFu);
    (void)e1000_rd32(REG_ICR);   /* reading ICR clears it */

    if (!read_mac())
        return init_fail(MSG_NET_ERR_EEPROM);

    for (uint32_t i = 0; i < 128; i++)
        e1000_wr32(REG_MTA + i * 4, 0);

    uint32_t ctrl = e1000_rd32(REG_CTRL);
    ctrl &= ~(CTRL_LRST | CTRL_PHY_RST | CTRL_ILOS | CTRL_VME);
    ctrl |= CTRL_SLU | CTRL_ASDE;
    e1000_wr32(REG_CTRL, ctrl);

    if (!setup_rings())
        return init_fail(MSG_NET_ERR_NOMEM);

    int link = wait_reg(REG_STATUS, STATUS_LU, STATUS_LU, T_LINK);
    g_ready = 1;

    net_tag();
    console_puts(msg(MSG_NET_E1000_MAC));
    put_mac(g_mac);
    console_puts(msg(link ? MSG_NET_LINK_UP : MSG_NET_LINK_DOWN));
    return 1;
}

int e1000_ready(void) {
    return g_ready;
}

uint32_t e1000_tx_count(void) {
    return g_tx_count;
}

uint32_t e1000_rx_enable_tick(void) {
    return g_rx_enable_tick;
}

void e1000_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) mac[i] = g_mac[i];
}

/* ── frames ─────────────────────────────────────────────────────────── */

int e1000_send(const uint8_t *frame, uint16_t len) {
    if (!g_ready || !frame || len < 14 || len > E1000_MAX_FRAME || len > TX_BUF_SIZE)
        return -1;

    /* Every descriptor points at the one TX buffer, so the buffer is free
       only once the card is done with the LAST frame handed to it, i.e. the
       descriptor before the tail has DD. After a send that timed out it may
       still be reading that frame: refuse instead of overwriting the
       buffer under it. (setup_rings() starts every descriptor with DD set,
       so the first send passes.) */
    volatile uint32_t *prev = desc(g_tx_ring, (g_tx_tail + NDESC - 1) % NDESC);
    if (!(prev[3] & TXD_STA_DD))
        return -1;

    volatile uint32_t *d = desc(g_tx_ring, g_tx_tail);

    volatile uint8_t *buf = (volatile uint8_t *)g_tx_buf;
    for (uint16_t i = 0; i < len; i++) buf[i] = frame[i];

    d[0] = g_tx_buf;
    d[1] = 0;
    d[2] = (uint32_t)len | ((uint32_t)(TXD_CMD_EOP | TXD_CMD_IFCS | TXD_CMD_RS) << 24);
    d[3] = 0;
    g_tx_tail = (g_tx_tail + 1) % NDESC;
    g_tx_count++;
    e1000_wr32(REG_TDT, g_tx_tail);

    uint32_t start = timer_get_ticks();
    uint32_t cap = T_TX * SPIN_CAP_PER_TICK;
    for (uint32_t spins = 0; spins < cap; spins++) {
        if (d[3] & TXD_STA_DD) return 0;
        if (timer_get_ticks() - start >= T_TX) break;
    }
    return (d[3] & TXD_STA_DD) ? 0 : -1;
}

int e1000_poll_rx(uint8_t *out, uint16_t max) {
    if (!g_ready || !out) return -1;

    uint32_t i = g_rx_next;
    volatile uint32_t *d = desc(g_rx_ring, i);
    uint32_t st = d[3];
    if (!(st & RXD_STA_DD)) return 0;
    /* d is volatile, so every poll reads the status from memory. The
       barrier keeps the compiler from reading the length or the buffer
       before the DD check (x86 does not reorder loads with loads). */
    __asm__ volatile ("" : : : "memory");

    uint32_t len    = d[2] & 0xFFFF;
    uint32_t errors = (st >> 8) & 0xFF;
    int result;
    if (!(st & RXD_STA_EOP) || errors || len == 0 || len > RX_BUF_SIZE || len > max) {
        result = -1;   /* split, bad or too big for the caller: dropped */
    } else {
        const volatile uint8_t *src = (const volatile uint8_t *)rx_buf_addr(i);
        for (uint32_t k = 0; k < len; k++) out[k] = src[k];
        result = (int)len;
    }

    /* Back to the card: clear the status, then move the tail onto this
       descriptor (the card stops one short of the tail). */
    d[2] = 0;
    d[3] = 0;
    e1000_wr32(REG_RDT, i);
    g_rx_next = (i + 1) % NDESC;
    return result;
}

/* ── RX diagnostics ─────────────────────────────────────────────────── */

static void serial_str(const char *s) {
    while (*s) serial_putchar(*s++);
}

static void serial_hex32(uint32_t v) {
    static const char hex[] = "0123456789abcdef";
    serial_putchar('0'); serial_putchar('x');
    for (int sh = 28; sh >= 0; sh -= 4) serial_putchar(hex[(v >> sh) & 0xF]);
}

static void serial_dec(uint32_t v) {
    char b[10];
    int n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) serial_putchar(b[--n]);
}

static void diag_hex(const char *name, uint32_t v) {
    serial_putchar(' '); serial_str(name); serial_putchar('='); serial_hex32(v);
}

static void diag_dec(const char *name, uint32_t v) {
    serial_putchar(' '); serial_str(name); serial_putchar('='); serial_dec(v);
}

/* One serial line with the RX state, for when an expected frame never
   showed up (the boot self-test prints it when ARP gets no reply): did the
   card take the frame (GPRC), had it no buffer (RNBC/MPC), or was it
   filtered (nothing counted)? The statistics registers clear on
   read, so each is read exactly once. The register names are hardware
   mnemonics, not prose, so they stay literals; the prefix goes through
   msg(). */
void e1000_rx_diag(void) {
    if (!g_ready) return;
    volatile uint32_t *d0 = desc(g_rx_ring, 0);
    uint32_t gprc = e1000_rd32(REG_GPRC);
    uint32_t mpc  = e1000_rd32(REG_MPC);
    uint32_t rnbc = e1000_rd32(REG_RNBC);
    serial_str(msg(MSG_NET_DIAG));
    diag_hex("RCTL",  e1000_rd32(REG_RCTL));
    diag_dec("RDLEN", e1000_rd32(REG_RDLEN));
    diag_dec("RDH",   e1000_rd32(REG_RDH));
    diag_dec("RDT",   e1000_rd32(REG_RDT));
    diag_hex("RDBAL", e1000_rd32(REG_RDBAL));
    diag_hex("STATUS", e1000_rd32(REG_STATUS));
    diag_hex("RAL0",  e1000_rd32(REG_RAL0));
    diag_hex("RAH0",  e1000_rd32(REG_RAH0));
    diag_hex("D0STA", d0[3] & 0xFF);
    diag_dec("D0LEN", d0[2] & 0xFFFF);
    diag_dec("next",  g_rx_next);
    diag_dec("GPRC",  gprc);
    diag_dec("MPC",   mpc);
    diag_dec("RNBC",  rnbc);
    serial_putchar('\n');
}
