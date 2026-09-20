# NullOS

> A bare-metal x86 (32-bit) operating system written from scratch in C99 and x86 Assembly.

```
  _   _       _ _  ___  ____  
 | \ | |_   _| | |/ _ \/ ___| 
 |  \| | | | | | | | | \___ \ 
 | |\  | |_| | | | |_| |___) |
 |_| \_|\__,_|_|_|\___/|____/ 

 NullOS v0.17.0 - Phase 17: Cleanup A
```

## Overview

NullOS is an experimental x86 OS written from scratch in C99 and NASM assembly. It boots via GRUB (Multiboot2), runs kernel and user processes with memory isolation, handles syscalls from ring 3 via `int 0x80`, and loads user programs from a flat ramfs image passed as a GRUB module.

See [CHANGELOG.md](CHANGELOG.md) for version history and [ROADMAP.md](ROADMAP.md) for planned future phases.

## Completed phases

| Phase | Description | Status |
|------|-----------|--------|
| **0** | Bootloader (Multiboot2) + VGA text output | ✅ Done |
| **1** | GDT, IDT, PIC, PIT (100 Hz), PS/2 keyboard | ✅ Done |
| **2** | PMM (Physical Memory Manager) + VMM with paging + kernel heap (`kmalloc`/`kfree`) | ✅ Done |
| **3** | Process table + cooperative round-robin scheduler, per-process context switch, per-process CR3, exception handlers | ✅ Done |
| **4** | TSS, ring 3 usermode, syscalls via `int 0x80` | ✅ Done |
| **5** | Multiboot2 module parser, flat ramfs, ELF32 loader, `exec()`, `user/init` | ✅ Done |
| **6** | Syscall return value in `eax`, preemption via IRQ0 (10-tick slice) | ✅ Done |
| **7** | `SYS_READ`, keyboard ringbuffer, interactive userland shell | ✅ Done |
| **8** | `SYS_EXEC`, Ctrl+C, foreground PID, copy-from-user | ✅ Done |
| **9** | `SYS_OPEN`, `SYS_CLOSE`, `SYS_READ` for ramfs files, per-process fd table | ✅ Done |
| **10** | Persistent disk: ATA PIO driver, FAT16 read/write, `SYS_CREATE`/`SYS_WRITE_FILE`, `touch`, editor with real saving | ✅ Done |
| **11** | PCI bus enumeration (legacy Configuration Mechanism #1), device table, `SYS_PCI_LIST`/`lspci` | ✅ Done |
| **12** | ATA IRQ-driven I/O: IRQ14/15 handlers, process blocking instead of busy-wait, exclusion gate, `PROCESS_BLOCKED` | ✅ Done |
| **13** | `fork()`: full address-space duplication, fabricated child kernel stack (resumes via `isr128_resume`), fd table duplication, `SYS_FORK` | ✅ Done |
| **14** | Kernel memory-safety hardening: userland pointer validation closing 4 confirmed ring 3 → ring 0 arbitrary memory read/write bugs, a `kmalloc()` integer-overflow bug, and the same gap in `sys_open`/`sys_create`/`sys_exec`/`sys_getarg`; version string centralized in `kernel/version.h` | ✅ Done |
| **15** | FAT16 subdirectories: `mkdir`/`cd`, path-aware `touch`/`edit`/`ls`; shared `dir_lookup()`/`dir_insert()`/`resolve_path()` core (resolves the Phase 10 duplicated-lookup tech debt); `SYS_CHDIR`/`SYS_MKDIR`; `exec()` (`run`/`edit`) now inherits the caller's `cwd_cluster` instead of always starting at the root | ✅ Done |
| **16** | Inter-process pipes (`kernel/pipe.c`, fixed pool, `SYS_PIPE`/`SYS_EXEC_PIPE`) and a real blocking `waitpid()` (`process_t.waiting_for_pid`, woken by `process_exit()`); shell gains `cmd1 \| cmd2` (`user/cat.c` as a minimal pipe sink) | ✅ Done |
| **17** | Cleanup A: audit fixes (`pmm_init` overflow, checked `vmm_map_page` returns, atomic pid/slot allocation, `fat16_init` validation), edit.c Shift and `process_spawn_user` race fixes, libnos string helpers, stream FAT16 writes (`fat16_write_at`), shell `>`/`<` redirection, `cat <file>`, `pwd`/`SYS_GETCWD`, `reboot`/`shutdown`, selftest expanded to 18 tests (`SYS_PCI_FIND`), `make inject` / `make run-reboot-test` | ✅ Done |

For planned Phases 17–31, see **[ROADMAP.md](ROADMAP.md)**.

## Documentation

Detailed, per-system documentation lives under `docs/`:

- [docs/kernel.md](docs/kernel.md) — kernel base (boot, GDT/IDT/PIC/PIT, keyboard) and program loading (Multiboot2, ramfs, ELF loader, `exec()`)
- [docs/memory.md](docs/memory.md) — PMM, VMM, kernel heap
- [docs/scheduler.md](docs/scheduler.md) — process table, scheduler, `fork()`, real `waitpid`
- [docs/syscalls.md](docs/syscalls.md) — full syscall table (number, signature, description)
- [docs/filesystem.md](docs/filesystem.md) — ATA PIO driver, FAT16, VFS
- [docs/pipes.md](docs/pipes.md) — in-kernel pipes, `SYS_EXEC_PIPE`, the shell's `cmd1 | cmd2`
- [docs/security.md](docs/security.md) — userland pointer validation, Phase 14 bug history
- [docs/pci.md](docs/pci.md) — PCI bus enumeration
- [docs/shell.md](docs/shell.md) — interactive shell and commands
- [docs/testing.md](docs/testing.md) — `run selftest`, the automated regression suite
- [docs/setup.md](docs/setup.md) — toolchain/dependency setup

[PROGRESS.md](PROGRESS.md) (not end-user documentation) carries cross-session working
memory: non-obvious architecture decisions and known technical debt.

## Structure

```
boot/
  boot.asm            Multiboot2 header + _start
  linker.ld           Memory layout (kernel @ 0x100000)
kernel/
  main.c              kmain: initialization and the scheduler loop
  version.h           Single source of truth for the version string
  gdt.c/asm           Global Descriptor Table
  idt.c               Interrupt Descriptor Table + exception handler
  isr.asm             Exception stubs and the syscall gate (isr128)
  pic.c               8259 PIC
  timer.c             PIT 100 Hz
  keyboard.c          PS/2 keyboard
  serial.c/h          Serial driver (mirrors VGA output)
  power.c/h           power_reboot() / power_shutdown()
  pipe.c/h            Inter-process pipes (fixed pool)
  tss.c               Task State Segment
  process.c/h         Process table + process_fork()
  scheduler.c/h       Cooperative round-robin
  context_switch.asm  ESP context switch
  usermode.asm        jump_to_usermode
  syscall.c/h         Syscall dispatcher
  multiboot2.h        Multiboot2 tag parser
  ramfs.c/h           Flat ramfs (find by name)
  elf.c/h             ELF32 loader
  exec.c/h            exec(): ramfs → ELF → spawn
  drivers/
    vga.c             VGA text driver
    ata.c/h           ATA PIO driver (LBA28, IRQ14/15-driven waits + exclusion gate)
    pci.c/h           PCI config space access (ports 0xCF8/0xCFC) + bus enumeration
  fs/
    fat16.c/h         FAT16 read/write over ATA
    vfs.c/h           ramfs + FAT16 dispatcher
  memory/
    pmm.c             Physical Memory Manager
    vmm.c             Virtual Memory Manager
    heap.c            kmalloc/kfree
user/
  lib/nullos.c/h      Syscall wrapper library (libnos, "nos_*") — one thin wrapper per syscall, see docs/kernel.md
  init.c              simple user process: nos_write + nos_exit
  spintest.c          process without yield: validates IRQ0 preemption
  shell.c             interactive shell: help/uname/fetch/ps/mem/ls/touch/mkdir/cd/pwd/echo/kill/run/edit/cat/redirects/reboot/shutdown/clear/exit
  edit.c              text editor: opens/creates/saves files on FAT16
  cat.c               prints a file, or copies stdin to stdout (pipe sink)
  forktest.c          calls fork(), prints the parent/child paths and PIDs
  selftest.c          automated regression suite ("run selftest") — see docs/testing.md
  link.ld             user linker script (entry @ 0x01000000)
  Makefile            builds lib/nullos.o and links init.elf, spintest.elf, shell.elf, edit.elf, forktest.elf, selftest.elf, and cat.elf against it
tools/
  Makefile            Build system (i686-elf-gcc + NASM + grub2-mkrescue), `disk`, `run`, `run-reboot-test`, `inject` targets
  grub.cfg.in         GRUB configuration template (version substituted at build time from kernel/version.h → build/grub.cfg)
  make_disk.sh        generates build/disk.img (FAT16, 32 MB) if it doesn't already exist
build/                Build artifacts (git-ignored) — includes disk.img (persists across builds)
docs/                 Per-system technical documentation (see "Documentation" above)
```

## Build

```bash
cd tools
make          # generates build/nullos.iso and build/disk.img (only creates the disk if it doesn't exist)
make disk     # forces creation of build/disk.img on its own
make run      # launches in QEMU with the disk attached (-drive ...,if=ide)
make clean    # cleans build/ (⚠ also deletes disk.img — persisted data is lost)
```

**Dependencies:** `i686-elf-gcc`, `i686-elf-ld`, `nasm`, `grub2-mkrescue`, `qemu-system-x86_64`, `mkfs.vfat`/`mcopy` (`dosfstools`/`mtools` packages, used by `tools/make_disk.sh`)

See [docs/setup.md](docs/setup.md) for toolchain installation details.

## Technical specs

- Architecture: x86 32-bit (i686)
- Language: C99 + NASM
- Boot: Multiboot2 via GRUB2
- Toolchain: i686-elf-gcc, i686-elf-ld, NASM

## License

MIT — see [LICENSE](LICENSE).
