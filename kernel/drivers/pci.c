// nullos/kernel/drivers/pci.c — PCI configuration space access and bus
// enumeration via the legacy Configuration Mechanism #1 (CONFIG_ADDRESS/
// CONFIG_DATA at ports 0xCF8/0xCFC). This is the discovery path future
// drivers (AHCI, xHCI, ...) will use instead of fixed I/O ports.

#include "pci.h"
#include "vga.h"
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

    for (int i = 0; i < 6; i++)
        d->bar[i] = pci_config_read32(bus, device, function, (uint8_t)(0x10 + i * 4));

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
        vga_putchar(hex[(value >> shift) & 0xF]);
}

void pci_print_list(void) {
    if (g_device_count == 0) {
        vga_puts("  (no PCI devices found)\n");
        return;
    }

    for (uint32_t i = 0; i < g_device_count; i++) {
        const pci_device_t *d = &g_devices[i];

        vga_puts("  ");
        print_hex_padded(d->bus, 2);
        vga_putchar(':');
        print_hex_padded(d->device, 2);
        vga_putchar('.');
        print_hex_padded(d->function, 1);
        vga_puts("  vendor=");
        print_hex_padded(d->vendor_id, 4);
        vga_puts(" device=");
        print_hex_padded(d->device_id, 4);
        vga_puts(" class=");
        print_hex_padded(d->class_code, 2);
        vga_putchar('/');
        print_hex_padded(d->subclass, 2);
        vga_puts(" progif=");
        print_hex_padded(d->prog_if, 2);
        vga_puts(" htype=");
        print_hex_padded(d->header_type, 2);
        vga_puts("\n");

        int any_bar = 0;
        for (int b = 0; b < 6; b++)
            if (d->bar[b] != 0) { any_bar = 1; break; }

        if (any_bar) {
            vga_puts("        bars:");
            for (int b = 0; b < 6; b++) {
                if (d->bar[b] == 0) continue;
                vga_puts(" bar");
                vga_putchar((char)('0' + b));
                vga_putchar('=');
                print_hex_padded(d->bar[b], 8);
            }
            vga_puts("\n");
        }
    }
}
