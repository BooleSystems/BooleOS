// nullos/kernel/power.c — reboot / shutdown (see power.h).
#include "power.h"
#include "drivers/pci.h"
#include "drivers/vga.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

#define KBC_STATUS_PORT 0x64
#define KBC_STATUS_IBF  0x02   // input buffer full: controller not ready for a command
#define KBC_CMD_RESET   0xFE   // pulse the CPU reset line

#define PIIX4_VENDOR    0x8086
#define PIIX4_PM_DEVICE 0x7113 // PIIX4 power management function (00:01.3 on QEMU's pc machine)
#define PIIX4_PMBA_REG  0x40   // PM I/O base address register in PCI config space
#define PIIX4_PMBA_MASK 0xFFC0 // base address = bits 15:6
#define PM1A_CNT_OFFSET 4      // PM1a control register, relative to the PM I/O base
#define PM1A_CNT_SLP_EN 0x2000 // bit 13: sleep enable; SLP_TYP (bits 12:10) 0 = S5 on QEMU

// Gives the hardware time to act on a reset/power-off request before we
// conclude it was ignored. Only a "did it fail?" timeout: on success the
// machine is gone long before this loop ends.
static void wait_for_request_to_take_effect(void) {
    for (volatile uint32_t i = 0; i < 20000000u; i++) { }
}

int power_reboot(void) {
    // wait until the 8042 can accept a command (bounded, in case there is
    // no controller at all and the port floats high)
    for (uint32_t i = 0; i < 100000u; i++) {
        if (!(inb(KBC_STATUS_PORT) & KBC_STATUS_IBF)) break;
    }
    outb(KBC_STATUS_PORT, KBC_CMD_RESET);

    wait_for_request_to_take_effect();
    vga_puts("reboot failed: keyboard-controller reset had no effect\n");
    return -1;
}

int power_shutdown(void) {
    uint8_t bus, dev, fn;
    if (!pci_find_device(PIIX4_VENDOR, PIIX4_PM_DEVICE, &bus, &dev, &fn)) {
        vga_puts("shutdown not supported on this hardware\n");
        return -1;
    }

    uint16_t pmba = (uint16_t)(pci_config_read32(bus, dev, fn, PIIX4_PMBA_REG) & PIIX4_PMBA_MASK);
    if (pmba == 0) {
        vga_puts("shutdown failed: PM I/O base address is not set\n");
        return -1;
    }

    outw((uint16_t)(pmba + PM1A_CNT_OFFSET), PM1A_CNT_SLP_EN);

    wait_for_request_to_take_effect();
    vga_puts("shutdown failed: ACPI power-off had no effect\n");
    return -1;
}
