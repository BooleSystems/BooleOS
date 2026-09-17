# Kernel base and program loading

## Kernel base
- Boots via GRUB2 with a Multiboot2 header
- VGA text mode 80×25 with colors
- GDT with ring 0 and ring 3 segments (code + data)
- IDT with handlers for CPU exceptions (0–31), IRQs (32–33 timer/keyboard, 46–47 ATA primary/secondary), and the syscall gate (`int 0x80`, DPL=3)
- Remapped 8259 PIC (IRQs 0–15 → vectors 32–47)
- PIT configured at 100 Hz
- PS/2 keyboard driver
- Version string centralized in `kernel/version.h` (`NULLOS_VERSION`/`NULLOS_PHASE`/`NULLOS_PHASE_DESC`, plus the composed `NULLOS_BANNER`/`NULLOS_SHORT_BANNER`) — the boot banner (`kernel/main.c`), the userland shell's `fetch`/`uname` (`user/shell.c`, which includes this header directly since it's plain text macros with no kernel types), and the GRUB menu label (`tools/grub.cfg`, generated at build time from `tools/grub.cfg.in`) all read from this one place

## Program loading
- Multiboot2 tag parser (`multiboot2_find_module`)
- Flat ramfs: `[uint32_t n] [entry×n: name[32]+offset+size] [data...]`
- ELF32 loader: validates the magic, iterates `PT_LOAD`, allocates physical pages, maps them into the process's CR3, copies segments
- `exec(name)`: looks it up in ramfs → creates a CR3 → `elf_load` → allocates a user stack → `scheduler_spawn_user`
- `kmain` calls `exec("shell")` if GRUB passed a module; otherwise it boots without a user process

## Using the ramfs

To load an `init` program:

1. Compile the program as a static ELF32:
   ```bash
   i686-elf-gcc -m32 -nostdlib -static -o init init.c
   ```

2. Create the ramfs image (tool to be implemented in `tools/mkramfs`):
   ```
   [uint32_t n_entries=1]
   [name="init\0..." offset=X size=Y]
   [ELF bytes]
   ```

3. Add it to `grub.cfg.in`:
   ```
   module2 /boot/ramfs.img
   ```

## Relevant files

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
  multiboot2.h        Multiboot2 tag parser
  ramfs.c/h           Flat ramfs (find by name)
  elf.c/h             ELF32 loader
  exec.c/h            exec(): ramfs → ELF → spawn
  drivers/vga.c       VGA text driver
```

See also `docs/setup.md` for the Phase 0 build/toolchain setup.
