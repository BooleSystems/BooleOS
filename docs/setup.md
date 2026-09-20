# NullOS — Setup and Build

This document is for **developers**: how to set up the build environment,
compile NullOS from source, run it in QEMU from the build tree and debug
it. (If you only want to run a released build without compiling anything,
see [quickstart.md](quickstart.md).) The top-level `README.md` has the list
of completed phases; this file only covers the current setup, not history.

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
- GRUB2 tools: `tools/Makefile` runs **`grub2-mkrescue`** (the Fedora
  name; Fedora package `grub2-tools`). Debian-family systems ship the same
  tool as `grub-mkrescue` (packages `grub-common` + `grub-pc-bin`); if only
  that name exists on your system, the ISO step fails with "command not
  found" and you need a `grub2-mkrescue` alias/symlink to it (not verified
  on Debian for this document)
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

Not verified for this document: the script does not install `dosfstools`
(`mkfs.vfat`, needed by `tools/make_disk.sh`) or `python3` (needed by
`tools/make_ramfs.py`) in the container, nor does it check that the
container has a `grub2-mkrescue` command — the build only works if the image
already provides them.

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

## Branches and versions

- **`main`** always holds the last released version. Every release is an
  annotated tag `vX.Y.Z` on `main`.
- **`nightly`** is where the day-to-day work happens; it may be temporarily
  broken between pushes. While a version is in progress, `kernel/version.h`
  reads `X.Y.Z-nightly` (the *next* version); closing the version drops the
  suffix, merges `nightly` into `main` and tags it.
- `kernel/version.h` is the single source of the version string: the boot
  banner, the shell's `uname`/`fetch` and the GRUB entry title all come from
  it (the GRUB config is generated at build time from `tools/grub.cfg.in`).
- The ISO also carries the *previous release* as an extra GRUB entry, kept in
  `tools/prev/` (`nullos.elf` + `ramfs.img` + `VERSION`, tracked in git).
  `make snapshot` refreshes it; run it by hand right after tagging a release,
  on the tagged tree (see `docs/safemode.md`).

## Build and run

```bash
cd nullos/tools

# Full build (generates nullos.iso and disk.img)
make

# Create build/disk.img on its own (only if it doesn't exist yet)
make disk

# Run in QEMU (attaches the FAT16 test disk; serial output goes to
# this terminal via -serial stdio — see tools/Makefile's QEMU_FLAGS)
make run

# Clean the build (⚠ also deletes disk.img — persisted FAT16 data is lost)
make clean
```

`make run` starts `qemu-system-x86_64` with `-boot d -cdrom build/nullos.iso
-drive file=build/disk.img,format=raw,if=ide -m 256M -serial stdio
-no-shutdown -no-reboot -display sdl`. `-display sdl` needs a QEMU built with
SDL support (edit `QEMU_FLAGS` in `tools/Makefile` if yours isn't).

`make run-reboot-test` is the same as `make run` but without `-no-reboot`,
so the `reboot` command really restarts the guest (`run`/`debug` keep the
flag on purpose: it makes QEMU exit on a triple fault instead of silently
rebooting, and it also turns a guest reset into a shutdown).

`make inject FILE=path/to/file [NAME=name.ext]` copies a file into the
root of `build/disk.img` with `mcopy`, without rebuilding the ISO. It is
host-side infrastructure only: the kernel still cannot `exec()` programs
from FAT16 (that arrives with Phase 19), so it is for putting test files on
the disk quickly. Don't run it while QEMU has the image open.

`make snapshot` records the current build in `tools/prev/` as the "previous
release" GRUB entry (see "Branches and versions"); it is never run by any
other target.

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

## The GRUB menu

The generated menu (`tools/grub.cfg.in`) has four entries: the default boot,
"NullOS (serial debug mode)", "NullOS (Safe Mode)" (boots with the `safemode`
argument, see `docs/safemode.md`) and "NullOS v<version> (previous release)".
Safe Mode also starts by itself after 3 boots in a row that never reached the
shell prompt.

## Debug via serial

`serial_init()` runs at the very start of `kmain` (before VGA), and
every `vga_putchar()` call already mirrors its character to serial
automatically. `make run` passes `-serial stdio` to QEMU, so the same
boot log VGA shows also appears in the terminal you ran `make run`
from. The "NullOS (serial debug mode)" entry passes a `debug` boot argument;
the kernel reads the command line (`boot_has_flag()`), but nothing acts on
`debug` yet, so today it boots exactly like the default entry.
