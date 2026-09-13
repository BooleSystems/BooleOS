# NullOS — Setup and Build (Phase 0)

## What you'll need

### Dependencies (Fedora)

```bash
sudo dnf install -y nasm xorriso qemu-system-x86 gdb make
```

### Cross-compiler x86_64-elf-gcc

This is the annoying step. Your system's default GCC generates binaries for Linux (with libc, Linux syscalls, etc.). The kernel needs a compiler that generates pure ELF with no dependencies at all.

**Fastest option: Docker**

```bash
# Runs a container with everything ready
docker run -it --rm -v $(pwd):/nullos ghcr.io/osdev/osdev-env
cd /nullos/tools && make
```

**Manual option (crosstool-ng on Fedora):**

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

# Configure for x86_64-elf
./ct-ng x86_64-unknown-elf
./ct-ng build
# (this takes ~20-30 minutes)

# Add to PATH
export PATH="$HOME/x-tools/x86_64-unknown-elf/bin:$PATH"
```

**Simpler alternative (i686 instead of x86_64):**

If you want to get started faster, the OSDev Wiki has prebuilt i686-elf-gcc binaries for Linux. Phase 0 also works in 32-bit — you just need to adjust the Makefile.

---

## Build structure

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
    ├── grub.cfg        <- Bootloader config
    └── setup_env.sh    <- Setup script
```

## Build and run

```bash
cd nullos/tools

# Full build (generates nullos.iso)
make

# Run in QEMU
make run

# Run with GDB for debugging
make debug

# Clean the build
make clean
```

## What you should see in QEMU

```
  _   _       _ _  ___  ____
 | \ | |_   _| | |/ _ \/ ___|
 |  \| | | | | | | | | \___ \
 | |\  | |_| | | | |_| |___) |
 |_| \_|\__,_|_|_|\___/|____/

 NullOS v0.0.1 - Experimental AI-generated OS
 Phase 0: Boot

------------------------------------------------------------
[BOOT] Multiboot2: OK
[MEM]  Kernel at 0x100000, multiboot_info at 0x...
[VGA]  Text mode 80x25 active
------------------------------------------------------------

 Phase 0 complete. Kernel responding.
 Next steps: GDT, IDT, interrupts (Phase 1)

 > _
```

## Debug via serial

QEMU with `-serial stdio` will print serial output to the terminal. In Phase 1 we'll add a serial driver for debugging without needing VGA.

## Next phase (Phase 1)

- GDT (Global Descriptor Table) — memory segmentation
- IDT (Interrupt Descriptor Table) — interrupt table
- PIC (Programmable Interrupt Controller) — remap IRQs
- Timer IRQ — foundation for the future scheduler
- Keyboard IRQ — read the keyboard for real
