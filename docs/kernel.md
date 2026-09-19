# Kernel base and program loading

## Kernel base
- Boots via GRUB2 with a Multiboot2 header
- VGA text mode 80×25 with colors
- GDT with ring 0 and ring 3 segments (code + data)
- IDT with handlers for CPU exceptions (0–31), IRQs (32–33 timer/keyboard, 46–47 ATA primary/secondary), and the syscall gate (`int 0x80`, DPL=3)
- Remapped 8259 PIC (IRQs 0–15 → vectors 32–47)
- PIT configured at 100 Hz
- PS/2 keyboard driver — tracks Ctrl and Shift press/release (scancodes `0x1D`/`0x9D` and `0x2A`/`0x36`/`0xAA`/`0xB6`) and picks between two US-QWERTY scancode→ASCII tables (`scancode_map`/`scancode_map_shift`) accordingly; before this fix there was no Shift table or tracking at all, so e.g. `Shift+5` never produced `%` and `Shift+\` never produced `|` — every character always came from the single unshifted table regardless of Shift. The raw-scancode path (`SYS_READ_RAW`, used by `user/edit.c`'s own separate `sc_map`) still has the same gap and wasn't touched by this fix — see `PROGRESS.md`.
- Serial driver (`kernel/serial.c/h`): `serial_init()` runs first in `kmain`, before VGA; `vga_putchar()` mirrors every character to serial automatically (see CLAUDE.md's debug-instrumentation rules — never write to serial manually after a call that already goes through `vga_puts()`/`vga_putchar()`, or output gets duplicated)
- Version string centralized in `kernel/version.h` (`NULLOS_VERSION`/`NULLOS_PHASE`/`NULLOS_PHASE_DESC`, plus the composed `NULLOS_BANNER`/`NULLOS_SHORT_BANNER`) — the boot banner (`kernel/main.c`), the userland shell's `fetch`/`uname` (`user/shell.c`, which includes this header directly since it's plain text macros with no kernel types), and the GRUB menu label (`tools/grub.cfg`, generated at build time from `tools/grub.cfg.in`) all read from this one place

## Program loading
- Multiboot2 tag parser (`multiboot2_find_module`)
- Flat ramfs: `[uint32_t n] [entry×n: name[32]+offset+size] [data...]`
- ELF32 loader: validates the magic, iterates `PT_LOAD`, allocates physical pages, maps them into the process's CR3, copies segments
- `exec(name)`: looks it up in ramfs → creates a CR3 → `elf_load` → allocates a user stack → `scheduler_spawn_user`
- `kmain` calls `exec("shell")` if GRUB passed a module; otherwise it boots without a user process

## User-space syscall library (libnos)

- **`user/lib/nullos.c/h`**: a small static library, one function per syscall in `kernel/syscall.h`, prefixed `nos_` (e.g. `nos_write`, `nos_open`, `nos_fork`, `nos_mkdir`). Every function is a thin wrapper — it does the `int $0x80` with the matching `SYS_*` number (included straight from `kernel/syscall.h`, not re-typed as magic numbers) and returns whatever the kernel put in `eax`. No added logic, no retries, no changed argument order — see the header comment in `nullos.h` and `docs/syscalls.md` for what each syscall actually does.
- **Why this exists:** before this library, every user program (`shell.c`, `edit.c`, `forktest.c`, `selftest.c`, `init.c`, `spintest.c`) had its own private copy of the same `int $0x80` wrapper functions, hand-written per file. Before v1.0.0 the syscall *interface* is still free to change (see ROADMAP.md's Phase 22, "syscall deprecation and compatibility strategy"), but changing how a syscall works *underneath* an unchanged interface used to still mean touching every program that called it. Now it means recompiling `lib/nullos.c` once — `user/Makefile` links every program against the same `$(BUILD)/lib/nullos.o`.
- **Two real (not purely cosmetic) differences from the wrappers this replaced**, found while unifying six independent copies:
  - `nos_write`/`nos_read` take an explicit `fd` argument. Every program's own `sys_write`/`sys_read` had `fd` hardcoded inline (`1` for write, `0` for read) even though the real `SYS_WRITE`/`SYS_READ` syscalls always took `(fd, buf, len)` — `edit.c`'s and `selftest.c`'s wrappers had already independently converged on exposing `fd` explicitly, so this standardizes on the form two of the four programs already used, not an invented one.
  - `nos_exec(name, arg)` replaces `shell.c`'s old two-function split (`sys_exec(name)` / `sys_exec_arg(name, arg)`) with the syscall's real 2-argument signature (`arg` may be `NULL`). This also fixes a latent bug: the old single-argument `sys_exec(name)` never constrained `ecx` in its inline asm, so the kernel's `sys_exec` read whatever garbage happened to be in `ecx` as the argument pointer — harmless in practice (an invalid address just makes `copy_user_str` fail silently), but not something a "thin wrapper" should have been doing.
- **Build wiring** (`user/Makefile`): `lib/nullos.c` compiles once to `$(BUILD)/lib/nullos.o`; every program's link line now includes that object alongside its own `.c` file (`link.ld`'s section rules are glob-based, so linking a second object needs no linker-script change).
- Adding a syscall wrapper for a *new* syscall means adding one function to `nullos.c`/`nullos.h` — no changes to any program that doesn't need the new syscall.

## Using the ramfs

`tools/make_ramfs.py` (fully implemented, not a stub) builds the ramfs
image automatically as part of `make`/`make all` — nothing needs to be
done by hand at build time, and `grub.cfg.in` already has a permanent
`module2 /boot/ramfs.img` line for every boot entry.

To add a new user program to the ramfs (the same steps used to add
`user/selftest.c` — see `docs/testing.md`):

1. Write `user/<name>.c` (see `user/forktest.c` for a minimal example:
   `#include "lib/nullos.h"` for syscalls — no raw `int $0x80` needed —
   entry point `_start`).
2. Add a build rule for it to `user/Makefile` (a `$(BUILD)/<name>.elf`
   target, and add that target to `all`'s dependency list).
3. In `tools/Makefile`: add a `<NAME>_ELF = $(BUILD)/user/<name>.elf`
   variable, add it to `$(RAMFS_IMG)`'s prerequisite list, and add
   `<name>=$(<NAME>_ELF)` to the `make_ramfs.py` invocation's argument
   list.
4. `make clean && make` — the new program is now in `build/ramfs.img`
   and runnable from the shell via `run <name>`.

## Relevant files

```
boot/
  boot.asm            Multiboot2 header + _start
  linker.ld           Memory layout (kernel @ 0x100000)
kernel/
  main.c              kmain: initialization and the scheduler loop
  version.h           Single source of truth for the version string
  serial.c/h          Serial driver (initialized before VGA; mirrored by vga_putchar())
  gdt.c/asm           Global Descriptor Table
  idt.c               Interrupt Descriptor Table + exception handler
  isr.asm             Exception stubs and the syscall gate (isr128)
  pic.c               8259 PIC
  timer.c             PIT 100 Hz
  keyboard.c          PS/2 keyboard
  multiboot2.h        Multiboot2 tag parser
  ramfs.c/h           Flat ramfs (find by name)
  elf.c/h             ELF32 loader
  exec.c/h            exec(): ramfs → ELF → spawn
  drivers/vga.c       VGA text driver
user/
  lib/nullos.c/h      Syscall wrapper library (libnos, "nos_*") — one thin wrapper per syscall
```

See also `docs/setup.md` for the Phase 0 build/toolchain setup.
