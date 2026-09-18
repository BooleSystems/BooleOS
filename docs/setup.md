# NullOS — Setup and Build

## What you'll need

### Dependencies

The project ships a detection script that installs these automatically
on Fedora or Debian:

```bash
bash tools/setup_env.sh
```

Which installs (Fedora package names shown; see the script for Debian
equivalents): `nasm`, `grub2-tools` (provides `grub2-mkrescue`),
`xorriso`, `qemu-system-x86` (provides the `qemu-system-x86_64`
binary), `gdb`, `make`, `gcc`. Also needed, not currently installed by
the script: `dosfstools`/`mtools` (`mkfs.vfat`/`mcopy`, used by
`tools/make_disk.sh` to build the FAT16 test disk) and `python3` (used
by `tools/make_ramfs.py`).

### Cross-compiler: i686-elf-gcc (32-bit), not x86_64

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

---

## Build structure

This is what the tree looked like right after Phase 0 (this file's
original scope); see the top-level `README.md` → "Structure" for the
current full layout (kernel subsystems, `user/`, `docs/`, etc.):

```
nullos/
├── boot/
│   ├── boot.asm       <- Assembly entry point (Multiboot2)
│   └── linker.ld      <- Linker script
├── kernel/
│   ├── main.c         <- kmain()
│   └── drivers/
│       ├── vga.h
│       └── vga.c      <- VGA text 80x25 driver
└── tools/
    ├── Makefile        <- Build system
    ├── grub.cfg.in     <- Bootloader config template
    └── setup_env.sh    <- Setup script
```

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

To debug with GDB, use `make debug` (same flags as `make run`, but
with `qemu-system-i386` instead of `qemu-system-x86_64`, plus
`-cpu qemu32 -s -S`: QEMU starts paused, forces a plain 32-bit CPU,
and opens a GDB stub on `localhost:1234`). The binary swap is the
part that actually matters: `qemu-system-x86_64`'s gdbstub always
reports the 64-bit register set over the wire regardless of `-cpu`,
which GDB rejects with "g packet reply is too long" — that's a
property of the binary itself, not the emulated CPU, so `-cpu` alone
can't fix it. `qemu-system-i386` (same `qemu-system-x86` package on
Fedora, no extra install) reports plain `i386` as expected, and GDB
auto-detects it — no manual `set architecture i386` needed. `-cpu
qemu32` is kept on top of that for a plain 32-bit CPU with no
long-mode extensions. Two terminals:

```bash
# Terminal 1 — from tools/
make debug

# Terminal 2 — from tools/, once QEMU is paused waiting for GDB
gdb ../build/nullos.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) continue
```

`tools/run_qemu.sh` is a separate, older standalone script that boots
`build/nullos.iso` directly — it does **not** attach `disk.img`, so
prefer `make run`/`make debug` unless you specifically want to boot
without the disk.

## What you should see in QEMU

The banner and detailed boot log have grown a lot since Phase 0 and
now include GDT/IDT/PIC/timer/keyboard/memory/scheduler/ATA/FAT16/PCI
initialization steps — see `docs/kernel.md` for what's actually
printed today, and `kernel/version.h` for the current version/phase
strings (`NULLOS_BANNER`), rather than a specific version number here
that would just go stale again at the next phase. Generically, the
banner looks like:

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

(Phase 0 itself only printed the banner, a Multiboot2/memory/VGA check,
and a "Phase 0 complete" line — there was no shell, no ramfs, and no
device drivers yet.)

## Debug via serial

`serial_init()` runs at the very start of `kmain` (before VGA), and
every `vga_putchar()` call already mirrors its character to serial
automatically — this isn't a future phase, it's already wired in.
`make run` passes `-serial stdio` to QEMU, so the same boot log VGA
shows also appears in the terminal you ran `make run` from. The GRUB
menu (`tools/grub.cfg.in`) also offers a second "NullOS (serial debug
mode)" entry at the boot menu.

## Where to go from here

Phase 0 (bootloader + VGA) is long done — see the top-level
`README.md` → "Completed phases" for what's actually implemented
today (Phase 14 as of this writing), `ROADMAP.md` for what's planned
next, and `docs/` for the per-system technical detail.
