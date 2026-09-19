# NullOS — Setup and Build

This document describes how to set up the build environment and
build/run NullOS today. (The project has grown a lot since its first
boot — see the top-level `README.md` for the list of completed
phases — but this file only covers the current setup, not history.)

## Dependencies

Today, automatic installation only exists for **Fedora** and
**Debian**, via:

```bash
bash tools/setup_env.sh
```

On any other distro (Arch, openSUSE, etc.), install the equivalent
packages manually. You need:

- `nasm` — assembler
- `gcc`/`make` — host toolchain (used for host-side tooling; the
  kernel itself is built with the `i686-elf-gcc` cross-compiler, see
  below)
- GRUB2 tools providing `grub-mkrescue`/`grub2-mkrescue` (Fedora:
  `grub2-tools`; Debian: `grub-common` + `grub-pc-bin`)
- `xorriso` — builds the bootable ISO
- A QEMU x86 package providing `qemu-system-x86_64`/`qemu-system-i386`
  (Fedora/Debian package name: `qemu-system-x86`)
- `gdb` — for `make debug`
- `dosfstools` + `mtools` (`mkfs.vfat`/`mcopy`) — used by
  `tools/make_disk.sh` to build the FAT16 test disk. Not currently
  installed by `setup_env.sh`.
- `python3` — used by `tools/make_ramfs.py`. Not currently installed
  by `setup_env.sh`.

### Arch Linux

`tools/setup_env.sh` does not cover Arch today — it only detects
Fedora and Debian — so on Arch the dependencies above need to be
installed manually via `pacman`:

```bash
sudo pacman -S --needed nasm grub libisoburn qemu-system-x86 gdb make gcc dosfstools mtools python
```

Notes on package names that differ from Fedora/Debian: `libisoburn`
is the package that provides the `xorriso` binary (Arch has no
separate `xorriso` package); `qemu-system-x86` provides both
`qemu-system-x86_64` and `qemu-system-i386`; `python` is Python 3
(Arch dropped Python 2 long ago, so there's no `python3`-suffixed
package). Everything else matches the Fedora/Debian names 1:1.

*(Possible future improvement, not implemented: `tools/setup_env.sh`
could detect Arch via `/etc/arch-release` and add a `pacman` branch
alongside the existing Fedora/Debian ones, instead of leaving Arch as
manual-only.)*

## Cross-compiler: i686-elf-gcc (32-bit, not x86_64)

The kernel is 32-bit (i686): every C file is compiled with `-m32`
(see `CFLAGS` in `tools/Makefile` and `user/Makefile`), and the linker
uses `-m elf_i386`. The cross-compiler you need is **`i686-elf-gcc`**
— a plain `x86_64-elf-gcc` cross-compiler targets the wrong
architecture and will not produce a working build. Your system's
default GCC generates Linux binaries (with libc, Linux syscalls,
etc.); the kernel needs a compiler that generates pure 32-bit ELF
with no dependencies at all.

**Fastest option: the project's own Docker script (recommended)**

```bash
tools/docker_build.sh clean   # or without "clean" to keep build/ around
```

This pulls `randomdude/gcc-cross-i686-elf` (a prebuilt i686-elf
toolchain image; override with the `NULLOS_DOCKER_IMAGE` env var if
you use a different one), installs `nasm`/`grub-pc-bin`/`grub-common`/
`xorriso`/`mtools` inside the container if missing, and runs `make` in
`tools/` — producing `build/nullos.iso` on the host (ownership fixed
up to your UID/GID afterward). See `tools/docker_build.sh` for the
exact steps.

**Manual option (crosstool-ng), targeting i686 — not x86_64:**

```bash
sudo dnf install -y gcc gcc-c++ gperf bison flex texinfo help2man \
                    make diffutils patch ncurses-devel autoconf \
                    automake libtool git wget

# Install crosstool-ng
git clone https://github.com/crosstool-ng/crosstool-ng
cd crosstool-ng
./bootstrap
./configure --enable-local
make

# Configure for i686-elf (NOT x86_64-unknown-elf — this kernel is 32-bit)
./ct-ng i686-unknown-elf
./ct-ng build
# (this takes ~20-30 minutes)

# Add to PATH
export PATH="$HOME/x-tools/i686-unknown-elf/bin:$PATH"
```

**Prebuilt binaries:** the OSDev community also maintains prebuilt
`i686-elf-gcc` binaries (e.g.
[lordmilko/i686-elf-tools](https://github.com/lordmilko/i686-elf-tools)
on GitHub) if you'd rather skip building the cross-compiler yourself.

## Windows and macOS

There is no native build path for Windows or macOS today — GRUB2's
`grub-mkrescue` has no functional native equivalent on Windows, and
Homebrew's GRUB package on macOS historically ships without the boot
files `grub-mkrescue` needs to produce a bootable ISO (a licensing
restriction upstream, not a packaging bug). Docker is the recommended
path on both platforms, since `tools/docker_build.sh` does all the
ISO-building work inside a Linux container and only needs a working
Docker daemon on the host — nothing in the script depends on anything
Linux-native beyond that.

> **⚠ Not tested on Windows or macOS.** This Docker path has not been
> tried by anyone on the project on either platform yet. It's the
> most likely route to work without extra effort, given how the
> script is built, but it is a prediction, not a verified path — if
> you try it, you may hit undocumented problems. Please open an issue
> on the repo reporting what worked and what didn't, so this section
> can be corrected with real results.

**Windows:**

1. Install [Docker Desktop](https://www.docker.com/products/docker-desktop/)
   with the WSL2 backend (the default recommended by both Microsoft
   and Docker today).
2. Open a WSL2 terminal (e.g. Ubuntu) and run the build from there,
   not from PowerShell/cmd:

   ```bash
   bash tools/docker_build.sh clean
   ```

**macOS:**

1. Install [Docker Desktop for Mac](https://www.docker.com/products/docker-desktop/).
2. Run the build from the native Terminal app (zsh or bash — macOS
   already has a Unix-compatible shell, no WSL-equivalent needed):

   ```bash
   bash tools/docker_build.sh clean
   ```

In both cases, `build/nullos.iso` and `build/disk.img` end up on the
host filesystem afterward, same as on Linux — see "Build and run"
below for how to run them (note that QEMU itself still needs to be
installed on the host to actually boot the resulting ISO; Docker only
handles the cross-compiled build).

## Build and run

```bash
cd nullos/tools

# Full build (generates nullos.iso and disk.img)
make

# Run in QEMU (attaches the FAT16 test disk; serial output goes to
# this terminal via -serial stdio — see tools/Makefile's run target)
make run

# Clean the build (⚠ also deletes disk.img — persisted FAT16 data is lost)
make clean
```

`tools/run_qemu.sh` is a separate, older standalone script that boots
`build/nullos.iso` directly — it does **not** attach `disk.img`, so
prefer `make run`/`make debug` unless you specifically want to boot
without the disk.

## Debugging with GDB

`make debug` uses the same flags as `make run`, but with
`qemu-system-i386` instead of `qemu-system-x86_64`, plus `-cpu qemu32
-s -S`: QEMU starts paused, forces a plain 32-bit CPU, and opens a GDB
stub on `localhost:1234`. The binary swap is the part that actually
matters: `qemu-system-x86_64`'s gdbstub always reports the 64-bit
register set over the wire regardless of `-cpu`, which GDB rejects
with "g packet reply is too long" — that's a property of the binary
itself, not the emulated CPU, so `-cpu` alone can't fix it.
`qemu-system-i386` (same `qemu-system-x86` package on Fedora, no extra
install) reports plain `i386` as expected, and GDB auto-detects it —
no manual `set architecture i386` needed. `-cpu qemu32` is kept on top
of that for a plain 32-bit CPU with no long-mode extensions. Two
terminals:

```bash
# Terminal 1 — from tools/
make debug

# Terminal 2 — from tools/, once QEMU is paused waiting for GDB
gdb ../build/nullos.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) continue
```

## What you should see in QEMU

The banner and boot log include GDT/IDT/PIC/timer/keyboard/memory/
scheduler/ATA/FAT16/PCI initialization steps — see `docs/kernel.md`
for what's actually printed today, and `kernel/version.h` for the
current version/phase strings (`NULLOS_BANNER`), rather than a
specific version number here that would just go stale at the next
phase. Generically, the banner looks like:

```
  _   _       _ _  ___  ____
 | \ | |_   _| | |/ _ \/ ___|
 |  \| | | | | | | | | \___ \
 | |\  | |_| | | | |_| |___) |
 |_| \_|\__,_|_|_|\___/|____/

 NullOS vX.Y.Z - Phase N: <phase description>

------------------------------------------------------------
[BOOT] Multiboot2: OK
[BOOT] ramfs module: 0x... - 0x... (... bytes)
 ... (GDT/IDT/PIC/timer/keyboard/memory/scheduler/ATA/FAT16/PCI init) ...
[EXEC] Loading shell...
------------------------------------------------------------

 Shell started!
 > _
```

## Debug via serial

`serial_init()` runs at the very start of `kmain` (before VGA), and
every `vga_putchar()` call already mirrors its character to serial
automatically. `make run` passes `-serial stdio` to QEMU, so the same
boot log VGA shows also appears in the terminal you ran `make run`
from. The GRUB menu (`tools/grub.cfg.in`) also offers a second "NullOS
(serial debug mode)" entry at the boot menu.
