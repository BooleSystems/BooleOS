// nullos/kernel/drivers/pci.c — PCI configuration space access and bus
// enumeration via the legacy Configuration Mechanism #1 (CONFIG_ADDRESS/
// CONFIG_DATA at ports 0xCF8/0xCFC). This is the discovery path future
// drivers (AHCI, xHCI, ...) will use instead of fixed I/O ports.

#include "pci.h"
#include "../hal.h"
#include "../messages.h"
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_MAX_DEVICES 64

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t v;
    __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static uint32_t pci_make_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return 0x80000000u
         | ((uint32_t)bus << 16)
         | ((uint32_t)(device & 0x1F) << 11)
         | ((uint32_t)(function & 0x07) << 8)
         | (offset & 0xFC);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t dword = pci_config_read32(bus, device, function, (uint8_t)(offset & 0xFC));
    return (uint16_t)(dword >> ((offset & 2) * 8));
}

uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t dword = pci_config_read32(bus, device, function, (uint8_t)(offset & 0xFC));
    return (uint8_t)(dword >> ((offset & 3) * 8));
}

/* ── device table, filled by pci_scan_bus() ─────────────────────── */

static pci_device_t g_devices[PCI_MAX_DEVICES];
static uint32_t     g_device_count = 0;

/* Number of real BARs for a header type. Bit 7 of the raw value only
   says "multi-function device", it is NOT part of the layout type, so
   it is masked off first: 0x80 is a general device (type 0) that
   happens to be multi-function, not a bridge.
     type 0 (general device): BAR0-BAR5
     type 1 (PCI-PCI bridge): BAR0-BAR1
     type 2 (CardBus bridge): none in the BAR0 sense (its 0x10 dword is a
                              single memory base register, reported as one)
   Any other type is unknown, so no BARs are assumed. */
static int pci_bar_count(uint8_t header_type) {
    switch (header_type & 0x7F) {
    case 0x00: return 6;
    case 0x01: return 2;
    case 0x02: return 1;
    default:   return 0;
    }
}

static void scan_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = pci_config_read16(bus, device, function, 0x00);
    if (vendor_id == 0xFFFF) return;   /* no device in this slot */
    if (g_device_count >= PCI_MAX_DEVICES) return;

    pci_device_t *d = &g_devices[g_device_count];
    d->bus         = bus;
    d->device      = device;
    d->function    = function;
    d->vendor_id   = vendor_id;
    d->device_id   = pci_config_read16(bus, device, function, 0x02);
    d->prog_if     = pci_config_read8 (bus, device, function, 0x09);
    d->subclass    = pci_config_read8 (bus, device, function, 0x0A);
    d->class_code  = pci_config_read8 (bus, device, function, 0x0B);
    d->header_type = pci_config_read8 (bus, device, function, 0x0E);

    /* Only read the BARs this header layout actually has; the rest stay
       zero. Past them, the same config-space offsets mean something else
       (e.g. a PCI-PCI bridge's bus numbers / I/O base+limit), which is
       not a BAR and must not be reported as one. */
    int nbars = pci_bar_count(d->header_type);
    for (int i = 0; i < 6; i++)
        d->bar[i] = (i < nbars)
            ? pci_config_read32(bus, device, function, (uint8_t)(0x10 + i * 4))
            : 0;

    g_device_count++;
}

int pci_scan_bus(void) {
    g_device_count = 0;

    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t device = 0; device < 32; device++) {
            uint16_t vendor_id = pci_config_read16((uint8_t)bus, (uint8_t)device, 0, 0x00);
            if (vendor_id == 0xFFFF) continue;   /* empty slot */

            scan_function((uint8_t)bus, (uint8_t)device, 0);

            /* only probe functions 1-7 if the device declares itself
               multi-function (header type bit 7) */
            uint8_t header_type = pci_config_read8((uint8_t)bus, (uint8_t)device, 0, 0x0E);
            if (header_type & 0x80) {
                for (uint8_t function = 1; function < 8; function++)
                    scan_function((uint8_t)bus, (uint8_t)device, function);
            }
        }
    }

    return (int)g_device_count;
}

/* Looks vendor/device up in the table built by the last pci_scan_bus(). */
int pci_find_device(uint16_t vendor_id, uint16_t device_id,
                    uint8_t *bus, uint8_t *device, uint8_t *function) {
    for (uint32_t i = 0; i < g_device_count; i++) {
        const pci_device_t *d = &g_devices[i];
        if (d->vendor_id == vendor_id && d->device_id == device_id) {
            if (bus)      *bus      = d->bus;
            if (device)   *device   = d->device;
            if (function) *function = d->function;
            return 1;
        }
    }
    return 0;
}

/* Returns the count from the last pci_scan_bus() call, without
   rescanning — the numeric counterpart to pci_print_list()'s VGA
   dump, so a caller (e.g. SYS_PCI_LIST) can check "found anything?"
   without parsing text output. */
int pci_device_count(void) {
    return (int)g_device_count;
}

/* ── reporting ───────────────────────────────────────────────────── */

static void print_hex_padded(uint32_t value, int digits) {
    static const char *hex = "0123456789ABCDEF";
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
        console_putc(hex[(value >> shift) & 0xF]);
}

void pci_print_list(void) {
    if (g_device_count == 0) {
        console_puts(msg(MSG_PCI_NO_PCI_DEVICES_FOUND));
        return;
    }

    for (uint32_t i = 0; i < g_device_count; i++) {
        const pci_device_t *d = &g_devices[i];

        console_puts("  ");
        print_hex_padded(d->bus, 2);
        console_putc(':');
        print_hex_padded(d->device, 2);
        console_putc('.');
        print_hex_padded(d->function, 1);
        console_puts(msg(MSG_PCI_VENDOR));
        print_hex_padded(d->vendor_id, 4);
        console_puts(msg(MSG_PCI_DEVICE));
        print_hex_padded(d->device_id, 4);
        console_puts(msg(MSG_PCI_CLASS));
        print_hex_padded(d->class_code, 2);
        console_putc('/');
        print_hex_padded(d->subclass, 2);
        console_puts(msg(MSG_PCI_PROGIF));
        print_hex_padded(d->prog_if, 2);
        console_puts(msg(MSG_PCI_HTYPE));
        print_hex_padded(d->header_type, 2);
        console_puts("\n");

        int nbars = pci_bar_count(d->header_type);
        int any_bar = 0;
        for (int b = 0; b < nbars; b++)
            if (d->bar[b] != 0) { any_bar = 1; break; }

        if (any_bar) {
            console_puts(msg(MSG_PCI_BARS));
            for (int b = 0; b < nbars; b++) {
                if (d->bar[b] == 0) continue;
                console_puts(msg(MSG_PCI_BAR));
                console_putc((char)('0' + b));
                console_putc('=');
                print_hex_padded(d->bar[b], 8);
            }
            console_puts("\n");
        }
    }
}
