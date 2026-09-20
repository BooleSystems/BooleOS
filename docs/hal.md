# Hardware abstraction layer (HAL) and `msg(ID)` — Phase 18-A

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

Migrated (they use the HAL): `idt.c` (the exception handler and the `idt_init` progress lines — see below), `main.c`, `syscall.c`, `process.c`, `scheduler.c`, `exec.c`, `power.c`, `memory/{pmm,vmm,heap}.c`, `drivers/pci.c`, and every disk access in `fs/fat16.c`.

**The exception handler goes through the HAL on purpose.** The worry with printing from an exception handler is state that may be inconsistent at that instant. The HAL console has none: `hal.c` is a stateless 1:1 forwarder, and all the screen state (`term_col`, `term_row`, `term_color`, and the serial mirror) lives in `vga.c`, exactly where it lived before the HAL existed. Going through `console_*` therefore adds no new risk — only one extra call frame, which is irrelevant even for a stack-overflow #DF (that fails either way). What was already true either way: the handler depends on `vga.c` not being mid-write and on `serial_putchar()` (which polls the transmit-ready bit). `msg()` in the handler is a static table lookup.

Deliberately not migrated:

- **Driver bring-up** — `vga_init()`, `keyboard_init()`, `ata_init()` are called directly from `kmain`. They are hardware initialization, not something a caller consumes.
- **The drivers' internals** — `vga.c`, `keyboard.c`, `ata.c` call their own helpers as before.
- **Serial debug prints** (`serial_putchar` etc.) — one implementation, no second one planned (see CLAUDE.md's rule against abstracting "in the dark").

`boot_get_memory_map()` is consumed by `pmm_init()` (see `docs/memory.md`, "Where the map comes from"). The centralized text table `msg(ID)` is described below.

## Centralized text: `msg(ID)` (kernel, pass 1)

`kernel/messages.h` declares `typedef enum { MSG_..., MSG_COUNT } msg_id_t` and `const char *msg(msg_id_t id)`; `kernel/messages.c` holds `static const char *const g_msgs[]`, one text per ID (C99 designated initializers, so table order can't drift from the enum). Kernel code prints with `console_puts(msg(MSG_PMM_TOTAL))`.

This is **not a translation system**: one column, English, no language selector. It only moves text that already existed into one place, so a new string can't be scattered through the code again.

- **Fragments, not format strings.** There is no printf in the kernel; numbers are still printed with `console_put_dec()`/`console_put_hex()` between pieces of text. A sentence with a number in the middle is therefore two or three IDs (`MSG_PMM_TOTAL` = `"Total: "`, `MSG_PMM_KB_FREE` = `"KB Free: "`).
- **Only output text.** A string that is compared with `strcmp`, passed to `exec()`, or used as a file/process name stays a literal at its use. Also left as literals: whitespace-only strings (`"\n"`, indentation and column padding — layout, not text), the version banner line (built from `NULLOS_BANNER` at compile time), the default process name `"kernel-task"` (an identifier stored in the process table), and the `"0x"` prefix inside `vga_puthex()` (driver internals).
- **Naming and stability.** `MSG_<SUBSYSTEM>_<DESCRIPTION>` (`MSG_TAG_*` for the `[BOOT] `-style prefixes, `MSG_EXC_*` for the CPU exception names, `MSG_PROC_STATE_*` for `ps` states), grouped by subsystem in the enum. Identical strings share one ID. An ID is never reused for a different string; add new IDs before `MSG_COUNT`.
- **Safe everywhere.** `msg()` is a bounds-checked lookup in a static const table: no init, no heap, no hardware. An out-of-range ID (or an entry left empty) returns `"(?)"`, never `NULL`. That is why `idt.c`'s exception handler may use it even though it bypasses the HAL for output: `exception_msgs[32]` holds IDs instead of pointers.
- **Compile-time check.** `g_msgs[]` has no explicit size, so its size is set by the highest designated initializer; a `typedef char ...[(sizeof(g_msgs)/sizeof(g_msgs[0]) == MSG_COUNT) ? 1 : -1]` fails the build if an ID was added at the end of the enum without a text (C99 has no `_Static_assert`). A missing entry in the middle is caught at run time by the `"(?)"` fallback.
- **Scope:** the whole `kernel/` (~130 call sites, 12 files) plus the userland (below). `selftest.c` and `forktest.c` are diagnostic output and are deliberately excluded.

### Userland table (`user/lib/messages.h/.c`, pass 2)

User programs cannot call the kernel's `msg()` (separate address space, no shared code), so the userland has its own table with the same mechanism: `umsg_id_t` enum with `UMSG_<PROGRAM>_<DESCRIPTION>` IDs, `const char *msg(umsg_id_t)` (bounds-checked, `"(?)"` fallback, never `NULL`) and the same compile-time size check. The prefix and type differ from the kernel's on purpose, and since the function is also called `msg()`, including both headers in one file is a compile error rather than a silent mixup.

- **Migrated:** `shell.c` (~50 output strings, the five `fetch` logo lines, `help_text` as one ID), `edit.c` (help bar, `[no name]`, the two save messages) and `cat.c` (1 string). Printing is `sh_puts(msg(UMSG_SH_...))`; the table is linked (`user/Makefile`, `$(LIBMSG)`) only into `shell`, `edit` and `cat`.
- **Not migrated:** shell command names compared with `strcmp`/`strncmp`, program names given to `nos_exec()`, the default path `"/"`, `nos_write_file(fd, "", 0)`, `clear_text` (25 newlines — layout), whitespace-only strings, and the two lines built from the version macro (`sh_puts(NULLOS_SHORT_BANNER " i686\n")`, `"  OS: " NULLOS_SHORT_BANNER ...`). `init.c` and `spintest.c` print one demo line each; linking the whole table into them would cost more than it saves.
- **Cost:** every program that links the table carries all of it (~2.7 KB; `cat.elf` went from 9.7 KB to 12.4 KB). Accepted; `user/Makefile` links objects directly, not an archive.
- **Layout-sensitive text is unchanged byte for byte:** the `fetch` logo lines and the `"Procs: "` line (its leading spaces align it under the logo) and the multi-line `help_text` are moved verbatim. `edit.c`'s status bar `sbar[80]` is still built column by column: only the whole-word pieces (`[no name]`, the save message) come from the table; the single characters (`L:`, `C:`) are assembled one at a time and stay as they are.
- **Known fragile spots, left as they are (migration only moved text):** the status bar's manual `sbar`/`memcpy` assembly, and `selftest.c`'s message built by `memcpy` into a shared `static char msg[64]` (excluded from this pass). Note `init.c`/`spintest.c`/`selftest.c` already use local variables named `msg`; they don't include `lib/messages.h`, so there is no clash.

## Relevant files

```
kernel/hal.h          interface (arch-neutral)
kernel/hal.c          x86 implementation (forwards to the drivers)
kernel/multiboot2.h   Multiboot2 tag parser (module + memory map)
kernel/messages.h     msg_id_t enum + msg()
kernel/messages.c     the message table
user/lib/messages.h   userland umsg_id_t enum + msg()
user/lib/messages.c   the userland message table
```
