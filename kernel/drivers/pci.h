// nullos/kernel/drivers/pci.h — PCI configuration space access (legacy
// mechanism #1, ports 0xCF8/0xCFC) and bus enumeration.

#ifndef PCI_H
#define PCI_H

#include <stdint.h>

typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  header_type;
    uint32_t bar[6];   /* raw BAR0-BAR5, offsets 0x10-0x24 */
} pci_device_t;

/* Raw configuration space access. offset must be within 0x00-0xFF;
   16/8-bit variants read the containing dword and shift/mask it. */
uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t  pci_config_read8 (uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

/* Scans bus 0-255 / device 0-31 / function 0-7 (skipping functions 1-7
   on single-function devices), storing every device found (vendor ID
   != 0xFFFF) in an internal static table. Returns the device count. */
int pci_scan_bus(void);

/* Prints the table built by the last pci_scan_bus() call via VGA, in
   the same style as the other boot-time device logs. */
void pci_print_list(void);

/* Returns the device count from the last pci_scan_bus() call, without
   rescanning (same table pci_print_list() reads from). */
int pci_device_count(void);

#endif
