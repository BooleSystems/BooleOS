# PCI bus enumeration

- `kernel/drivers/pci.c/h`: reads the PCI configuration space via the legacy Configuration Mechanism #1 — 32-bit `outl`/`inl` on `CONFIG_ADDRESS` (0xCF8) and `CONFIG_DATA` (0xCFC); `pci_config_read32/16/8` always read the containing dword and shift/mask it for the narrower widths
- `pci_scan_bus()`: iterates bus 0–255 × device 0–31 × function 0–7, treating vendor ID `0xFFFF` as an empty slot; only probes functions 1–7 when the function-0 header type has the multi-function bit (0x80) set; stores vendor/device ID, class/subclass/prog IF, header type, and all 6 raw BARs (offsets 0x10–0x24) for up to 64 devices in a static table — no capability list or MSI/MSI-X parsing yet
- BARs are read per header type: type 0 has 6, type 1 (PCI-PCI bridge) 2, type 2 (CardBus) 1 — the multi-function bit (0x80) is masked off the header type first, and the rest of `bar[]` stays 0
- `pci_config_write32()`/`pci_config_write16()` (Phase 24-A): configuration writes. The 16-bit write uses `outw` on the matching half of `CONFIG_DATA`, so writing the command register (0x04) never rewrites the status register (0x06), whose bits are write-1-to-clear. `PCI_CMD_IO_SPACE`/`PCI_CMD_MEM_SPACE`/`PCI_CMD_BUS_MASTER` name the command bits.
- `pci_bar_size(bus, dev, fn, index)` (Phase 24-A): turns I/O and memory decode off, writes all ones to the BAR, reads the size mask back, restores the BAR and the command register, and returns the size in bytes (0 if unimplemented). Boot-time only.
- `pci_find_device(vendor, device, &bus, &dev, &fn)`: looks a vendor/device ID pair up in the table built by the last `pci_scan_bus()` (1 = found, output pointers optional); used by `kernel/power.c` to find the PIIX4 and by `SYS_PCI_FIND`
- `SYS_PCI_FIND (33)`: `pci_find(vendor, device) → 1/0` for userland (`nos_pci_find()`), added so `user/selftest.c` can check a specific device (the Intel 440FX host bridge, 8086:1237) instead of only "found ≥ 1". That selftest check is tied to QEMU's default `pc` machine and is expected to fail when Phase 25 moves QEMU to `-machine q35` — update the IDs then
- `pci_print_list()`: reprints the table via VGA as `bus:device.function  vendor=XXXX device=XXXX class=XX/XX progif=XX htype=XX`, plus a `bars:` line for any device with at least one non-zero BAR
- `pci_device_count()`: returns `g_device_count` from the last `pci_scan_bus()` call without rescanning — the numeric counterpart to `pci_print_list()`'s VGA dump, added so a caller can check "found anything?" without parsing text output (used by `SYS_PCI_LIST`'s return value, see below, and by `user/selftest.c`)
- `kmain` calls `pci_scan_bus()` + `pci_print_list()` right after FAT16 init — bus enumeration is independent hardware discovery, not on the disk-mount path (see `[PCI]` in the boot log)
- `SYS_PCI_LIST (24)` / shell command `lspci`: reprints the same table captured at boot without rescanning the bus, and returns `pci_device_count()` (previously always returned 0 — changed so `user/selftest.c` can assert "found ≥ 1 device" without parsing VGA output)

## Relevant files

```
kernel/drivers/pci.c/h    PCI config space access (ports 0xCF8/0xCFC) + bus enumeration
```
