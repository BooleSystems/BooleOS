# Hardware abstraction layer (HAL) — Phase 18-A, first pass

`kernel/hal.h` declares the interface, `kernel/hal.c` is the x86 implementation. Everything the rest of the kernel needs from the machine — text output, key input, block I/O, power control, the boot memory map — is reached through these functions instead of calling `vga.c`/`keyboard.c`/`ata.c`/Multiboot2 code directly.

## Why it exists

Portability. The signatures are plain C and architecture-neutral: no VGA cells, port I/O, PS/2 scancode tables or Multiboot2 tags in the interface. Those stay inside `hal.c` and the drivers. A future port (e.g. `arch/arm/`) implements the same functions its own way, and none of the callers (`kmain`, `syscall.c`, `fat16.c`, ...) has to change. Safe Mode (Phase 18-B) is also built on this interface.

`hal.c` is a **thin forwarding layer**: each function calls the driver that already existed and adds no behavior, so a caller sees exactly what it saw before. The drivers were not rewritten.

## The interface

| Area | Functions | Backend today |
|---|---|---|
| Console | `console_putc`, `console_puts`, `console_put_hex`, `console_put_dec`, `console_set_color(fg, bg)`, `console_clear`, `console_set_cursor(col, row)`; `console_color_t` (`CONSOLE_*`, same 16 values as the VGA palette) | `vga.c` |
| Input | `input_poll_key()` (next character or -1), `input_poll_raw()` (raw event or -1; bit 8 = Ctrl, bit 9 = Shift), `input_flush()` | `keyboard.c` |
| Block device | `block_read_sector(lba, buf)`, `block_write_sector(lba, buf)` — 512-byte sectors, 0 / -1 | `ata.c` |
| Power | `power_reboot()`, `power_shutdown()` — `hal.h` includes `power.h`; no wrapper (the functions already had arch-neutral signatures) | `power.c` |
| Boot info | `hal_boot_init(magic, info)`, `boot_get_memory_map(out, max)`, `boot_mem_region_t`, `BOOT_MEM_*` | Multiboot2 (`multiboot2.h`) |

Notes:

- **Serial mirroring stays inside the backend.** `vga_putchar()` already mirrors every character to the serial port, so `console_putc()` does too. Callers must not write to serial again after calling a console function (this is the rule from CLAUDE.md, now attached to the abstract function).
- `input_poll_*` never block; the caller (`sys_read`) polls and sleeps as it always did.
- `block_write_sector()` keeps `ata_write_sector()`'s contract: a write that reached the media but whose final cache flush was not confirmed still returns 0 (see `docs/filesystem.md`).
- `hal_boot_init()` is called once at the start of `kmain` with what the bootloader passed in; it doubles as the boot-magic check (returns -1 if the magic is wrong). `boot_get_memory_map()` returns the number of regions written (truncated to `max`), or -1 when there is no memory map. The boot information must still be readable when it is called (on x86: the identity-mapped low memory where GRUB puts it).

## What is migrated, and what is not

Migrated (they use the HAL): `main.c`, `syscall.c`, `process.c`, `scheduler.c`, `exec.c`, `power.c`, `memory/{pmm,vmm,heap}.c`, `drivers/pci.c`, and every disk access in `fs/fat16.c`.

Deliberately not migrated:

- **Driver bring-up** — `vga_init()`, `keyboard_init()`, `ata_init()` are called directly from `kmain`. They are hardware initialization, not something a caller consumes.
- **The exception handler (`idt.c`)** — it runs in an unstable state and must not depend on a layer that could itself be part of what broke. It still calls `vga_*` directly. Tracked in `docs/TODO.md`.
- **The drivers' internals** — `vga.c`, `keyboard.c`, `ata.c` call their own helpers as before.
- **Serial debug prints** (`serial_putchar` etc.) — one implementation, no second one planned (see CLAUDE.md's rule against abstracting "in the dark").

Also not done yet: `boot_get_memory_map()` has no caller — `pmm_init()` still receives a hardcoded `64 * 1024` KB from `kmain`. Wiring the real map into the PMM would change behavior, so it is a separate step (`docs/TODO.md`). The centralized text table `msg(ID)` is a separate later commit of 18-A.

## Relevant files

```
kernel/hal.h          interface (arch-neutral)
kernel/hal.c          x86 implementation (forwards to the drivers)
kernel/multiboot2.h   Multiboot2 tag parser (module + memory map)
```
