# NullOS

> A bare-metal x86 (32-bit) operating system written from scratch in C99 and x86 Assembly.

```
  _   _       _ _  ___  ____  
 | \ | |_   _| | |/ _ \/ ___| 
 |  \| | | | | | | | | \___ \ 
 | |\  | |_| | | | |_| |___) |
 |_| \_|\__,_|_|_|\___/|____/ 

 NullOS v0.3.0 - Phase 3: Processes + Scheduler
```

## Overview

NullOS is an experimental operating system for the x86 architecture. It aims to implement a complete OS stack, including memory management, multitasking, a virtual file system, and eventually a graphical user interface. The project is built using a cross-compiler toolchain and is designed to boot via GRUB using the Multiboot2 specification.

## Project Roadmap

| Phase | Description | Status |
|-------|-------------|--------|
| **0** | Bootloader + VGA text output | ✅ Done |
| **1** | GDT, IDT, PIC, Timer, Keyboard | ✅ Done |
| **2** | Physical Memory Manager (PMM) | ✅ Done |
| **2b** | Virtual Memory Manager (VMM) + Kernel Heap | ✅ Done |
| **3a** | Kernel process table + cooperative scheduler | ✅ Done |
| **3b** | Context switching + isolated processes | 🔄 In Progress |
| **4** | Filesystem + VFS | ⏳ Planned |
| **5** | Syscalls + Userland | ⏳ Planned |
| **6** | Shell (nullsh) | ⏳ Planned |
| **7** | Basic Networking | ⏳ Planned |
| **8** | Graphical User Interface (GUI) | ⏳ Planned |
| **9** | User Applications | ⏳ Planned |

## Project Structure

```text
boot/           - Bootloader entry point and linker scripts
  ├── boot.asm  - Multiboot2 header and early initialization
  └── linker.ld - Kernel memory layout
kernel/         - Core kernel source code
  ├── main.c    - Kernel entry point
  ├── memory/   - PMM, VMM, and Heap management
  ├── drivers/  - VGA text driver
  └── ...       - GDT, IDT, PIC, Keyboard, Scheduler, etc.
tools/          - Build scripts and configuration files
  ├── Makefile  - Build system instructions
  ├── grub.cfg  - GRUB bootloader configuration
  ├── docker_build.sh - Environment-agnostic build script
  └── run_qemu.sh     - Helper script to launch QEMU
build/          - Compilation artifacts (git-ignored)
docs/           - Technical documentation and setup guides
```

## Getting Started

### Prerequisites

To build and run NullOS, you will need:
- **Docker**: For the consistent cross-compilation environment.
- **QEMU**: To emulate the x86 hardware.
- **Bash**: To run the build and helper scripts.

### Building NullOS

The recommended way to build NullOS is via Docker to avoid toolchain issues.

```bash
# Build the kernel and generate a bootable ISO
bash tools/docker_build.sh
```

### Running NullOS

Once the build is complete, you can launch the OS using QEMU:

```bash
qemu-system-i386 -cdrom build/nullos.iso -m 512M -vga std -serial stdio
```

## Technical Specifications

- **Architecture**: x86 (32-bit, i686-elf)
- **Standard**: C99 / GNU99
- **Boot Protocol**: Multiboot2
- **Toolchain**: i686-elf-gcc, NASM
- **Target Platform**: Bare-metal (Generic x86 PC)

## License

This project is licensed under the MIT License.
