# NullOS

> A bare-metal x86 (32-bit) operating system written from scratch in C99 and x86 Assembly.

```
  _   _       _ _  ___  ____  
 | \ | |_   _| | |/ _ \/ ___| 
 |  \| | | | | | | | | \___ \ 
 | |\  | |_| | | | |_| |___) |
 |_| \_|\__,_|_|_|\___/|____/ 

 NullOS v0.10.1 - Phase 10: persistent disk (ATA PIO + FAT16)
```

## Overview

NullOS is an experimental x86 OS written from scratch in C99 and NASM assembly. It boots via GRUB (Multiboot2), runs kernel and user processes with memory isolation, handles syscalls from ring 3 via `int 0x80`, and loads user programs from a flat ramfs image passed as a GRUB module.

## Roadmap

| Phase | Description | Status |
|------|-----------|--------|
| **0** | Bootloader (Multiboot2) + VGA text output | ✅ Done |
| **1** | GDT, IDT, PIC, PIT (100 Hz), PS/2 keyboard | ✅ Done |
| **2** | PMM (Physical Memory Manager) | ✅ Done |
| **2b** | VMM with paging + heap (`kmalloc`/`kfree`) | ✅ Done |
| **3a** | Process table + cooperative round-robin scheduler | ✅ Done |
| **3b** | Per-process context switch, per-process CR3, exception handlers | ✅ Done |
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

## What's implemented

### Kernel base
- Boots via GRUB2 with a Multiboot2 header
- VGA text mode 80×25 with colors
- GDT with ring 0 and ring 3 segments (code + data)
- IDT with handlers for CPU exceptions (0–31), IRQs (32–33 timer/keyboard, 46–47 ATA primary/secondary), and the syscall gate (`int 0x80`, DPL=3)
- Remapped 8259 PIC (IRQs 0–15 → vectors 32–47)
- PIT configured at 100 Hz
- PS/2 keyboard driver

### Memory
- PMM: physical page bitmap (64 MB)
- VMM: 32-bit paging with 0–8 MB identity map, per-process directories
- Kernel heap: `kmalloc`/`kfree` with first-fit

### Multitasking
- Process table with up to 16 entries
- Round-robin scheduler with preemption via IRQ0 (10-tick slice = 100ms at 100Hz)
- Context switch in assembly (saves/restores callee-saved registers via ESP)
- TSS configured for a per-process kernel stack (SS0:ESP0)
- Processes that never call yield are preempted by the timer automatically

### Usermode and syscalls
- `jump_to_usermode` via `iret` with ring 3 segments (CS=0x1B, SS=0x23)
- Isolation via per-process CR3
- Syscall gate: `int 0x80`, convention `eax=num, ebx=arg1, ecx=arg2, edx=arg3`

| num | name | signature |
|-----|------|------------|
| 1 | `SYS_WRITE` | `write(fd, buf, len) → bytes` |
| 2 | `SYS_EXIT` | `exit(code) → does not return` |
| 3 | `SYS_YIELD` | `yield() → 0` |
| 4 | `SYS_GETPID` | `getpid() → pid` |
| 5 | `SYS_READ` | `read(fd, buf, len) → bytes read` |
| 6 | `SYS_UPTIME` | `uptime() → ticks (100 Hz)` |
| 7 | `SYS_MEMINFO` | `meminfo(*pmm_pages, *heap_bytes, *nprocs) → 0` |
| 8 | `SYS_PS` | `ps() → 0` (prints the process table via VGA) |
| 9 | `SYS_KILL` | `kill(pid) → 0 or -1` |
| 10 | `SYS_EXEC` | `exec(name) → pid or -1` |
| 11 | `SYS_OPEN` | `open(name) → fd (≥3) or -1` |
| 12 | `SYS_CLOSE` | `close(fd) → 0 or -1` |
| 13 | `SYS_READ_RAW` | `read_raw() → scancode\|(ctrl<<8)` (blocking, no echo) |
| 14 | `SYS_GOTOXY` | `gotoxy(col, row) → 0` |
| 15 | `SYS_CLEAR` | `clear() → 0` |
| 16 | `SYS_GETARG` | `getarg(buf, len) → bytes or -1` (argument passed by `SYS_EXEC`) |
| 17 | `SYS_KBD_FLUSH` | `kbd_flush() → 0` (flushes keyboard buffers) |
| 18 | `SYS_SETCOLOR` | `set_color(fg, bg) → 0` |
| 19 | `SYS_SET_RAW_MODE` | `set_raw_mode(1/0) → 0` (turns off `SYS_READ` echo) |
| 20 | `SYS_WAIT` | `wait(pid) → 0` (blocks until the process terminates) |
| 21 | `SYS_READDIR` | `readdir() → 0` (lists ramfs + FAT16 via VGA) |
| 22 | `SYS_WRITE_FILE` | `write_file(fd, buf, len) → 0 or -1` (writes to FAT16) |
| 23 | `SYS_CREATE` | `create(name) → fd (≥3) or -1` (opens if it exists, otherwise creates it empty on FAT16) |
| 24 | `SYS_PCI_LIST` | `pci_list() → 0` (reprints the PCI device table found at boot via VGA, without rescanning) |
| 25 | `SYS_FORK` | `fork() → child's pid (parent) / 0 (child) / -1` (duplicates the caller: full address space, open fds; not copy-on-write) |

> `SYS_READ` is polymorphic: fd=0 reads from the keyboard (blocking, with echo and backspace); fd≥3 reads from a file opened via `SYS_OPEN`/`SYS_CREATE`, advances the position, and returns 0 on EOF.

> `SYS_WRITE` (fd=1/2) writes to VGA; files use the dedicated `SYS_WRITE_FILE`, which writes to FAT16 via `vfs_write`/`fat16_write_file` — ramfs remains read-only.

> **Syscall return value:** `isr128` writes the `syscall_handler`'s return value into the EAX slot of the `pusha` frame before the `popa`, delivering the correct value in `eax` to userland after the `iret`. Userland inline asm must use the `"=a"`/`"0"` constraints so the compiler doesn't assume eax is unchanged after `int $0x80`.

### Program loading
- Multiboot2 tag parser (`multiboot2_find_module`)
- Flat ramfs: `[uint32_t n] [entry×n: name[32]+offset+size] [data...]`
- ELF32 loader: validates the magic, iterates `PT_LOAD`, allocates physical pages, maps them into the process's CR3, copies segments
- `exec(name)`: looks it up in ramfs → creates a CR3 → `elf_load` → allocates a user stack → `scheduler_spawn_user`
- `kmain` calls `exec("shell")` if GRUB passed a module; otherwise it boots without a user process

### Interactive shell
- Keyboard ringbuffer (256 chars) in IRQ1; echo removed from the handler
- `SYS_READ (fd=0)`: reads from the ringbuffer with polling + `scheduler_sleep_current(1)` to avoid starving; echoes input and handles backspace
- `user/shell`: loop `> ` → `sys_read` → `run_command`
- Commands: `help`, `uname`, `fetch`, `ps`, `mem`, `ls`, `touch <name>`, `echo <text>`, `kill <pid>`, `run <prog>`, `edit <file>`, `clear`, `exit`
- `fetch`: ASCII banner with OS, Arch, Uptime, free PMM, free Heap, running Procs
- No background debug tasks — VGA is exclusive to the shell

### Files: open/read/close (ramfs + FAT16)
- `SYS_OPEN (11)`: copies the name from user space via `user_kptr`, allocates the first free slot in `fd_table[proc_slot][0..7]`, and calls `vfs_open` — which tries ramfs via `ramfs_find` and, if not found, FAT16 via `fat16_find`; returns `fd = 3 + idx`, or -1 if not found in either backend or no free slots
- `SYS_READ (5)` with fd≥3: dispatches by `fd->backend` via `vfs_read` — ramfs reads directly from `ramfs_base + offset + pos`, FAT16 uses `fat16_read_at` following the cluster chain; both advance `pos` and return 0 on EOF — non-blocking
- `SYS_CLOSE (12)`: marks the slot as free
- `fd_table[PROCESS_MAX][8]` — global table indexed by process slot; `sys_exit` clears all of the process's fds on exit, avoiding slot leaks
- fds 0/1/2 are reserved (stdin/stdout/stderr); files start at fd=3

### Persistent disk: ATA PIO + FAT16

- `kernel/drivers/ata.c`: ATA PIO driver — probes all 4 possible slots (primary/secondary × master/slave) by sending `IDENTIFY` (0xEC) and discarding ATAPI devices (signature `LBA_MID=0x14`/`LBA_HI=0xEB`); `ata_read_sector`/`ata_write_sector` do LBA28 with `READ SECTORS` (0x20) / `WRITE SECTORS` (0x30) + `CACHE FLUSH` (0xE7); waits for command completion via IRQ14/15 instead of polling when running inside a scheduled process — see "ATA IRQ-driven I/O" below
- `kernel/fs/fat16.c`: reads the BPB from sector 0, caches the whole FAT in the heap (`kmalloc`); `fat16_find`/`fat16_readdir` scan the root directory (8.3 names); `fat16_read_at` follows the cluster chain from an arbitrary offset; `fat16_write_file` frees the old chain, allocates a new one, and rewrites the FAT to disk; `fat16_create` looks for a free/deleted entry in the root dir and writes an empty dirent (idempotent — doesn't fail if the file already exists)
- `kernel/fs/vfs.c`: the single dispatcher used by the syscalls — `vfs_open` tries ramfs (read-only) and then FAT16; `vfs_create` calls `vfs_open` first and only creates on FAT16 if not found; `vfs_write` refuses to write to ramfs files
- `SYS_CREATE (23)`: open-or-create — copies the name from userland, tries to open it, and if it doesn't exist, creates the entry on FAT16 and returns the fd ready for writing
- Boot: `kmain` calls `ata_init()` and `fat16_init()` right after the scheduler; if there's no disk (or it's not valid FAT16), boot continues normally and on-disk file operations return -1 without crashing
- `tools/make_disk.sh` + the `make disk` target: generates `build/disk.img` (32 MB, FAT16 via `mkfs.vfat`) only if it doesn't already exist, preserving data across builds; `tools/run_qemu`/`make run` attaches the disk as `-drive file=build/disk.img,format=raw,if=ide`
- Shell: `touch <name>` creates an empty file (`SYS_CREATE` + `SYS_CLOSE`); `ls` lists ramfs and FAT16 separately
- Editor: `load_file` now creates the file (`SYS_CREATE`) when it doesn't exist, keeping the fd open; Ctrl+S writes the whole buffer via `SYS_WRITE_FILE` and shows "saved" or "saved (no disk)" in the footer; Ctrl+Q closes the fd before exiting

### PCI bus enumeration

- `kernel/drivers/pci.c/h`: reads the PCI configuration space via the legacy Configuration Mechanism #1 — 32-bit `outl`/`inl` on `CONFIG_ADDRESS` (0xCF8) and `CONFIG_DATA` (0xCFC); `pci_config_read32/16/8` always read the containing dword and shift/mask it for the narrower widths
- `pci_scan_bus()`: iterates bus 0–255 × device 0–31 × function 0–7, treating vendor ID `0xFFFF` as an empty slot; only probes functions 1–7 when the function-0 header type has the multi-function bit (0x80) set; stores vendor/device ID, class/subclass/prog IF, header type, and all 6 raw BARs (offsets 0x10–0x24) for up to 64 devices in a static table — no capability list or MSI/MSI-X parsing yet
- `pci_print_list()`: reprints the table via VGA as `bus:device.function  vendor=XXXX device=XXXX class=XX/XX progif=XX htype=XX`, plus a `bars:` line for any device with at least one non-zero BAR
- `kmain` calls `pci_scan_bus()` + `pci_print_list()` right after FAT16 init — bus enumeration is independent hardware discovery, not on the disk-mount path (see `[PCI]` in the boot log)
- `SYS_PCI_LIST (24)` / shell command `lspci`: reprints the same table captured at boot without rescanning the bus

### ATA IRQ-driven I/O

- Replaces phase 10's busy-wait: whenever `ata_read_sector`/`ata_write_sector` run inside a scheduled process, they block the calling process instead of polling `BSY`/`DRQ` while a disk command is in flight; the original polling path is kept as a fallback for calls with no current process (early boot, e.g. `fat16_init()` reading the BPB before the scheduler ever runs a process)
- IRQ14 (primary channel, vector 46) and IRQ15 (secondary, vector 47) stubs were added to `isr.asm`/`idt.c` following the existing IRQ0/IRQ1 pattern; `ata_init()` registers the handler and unmasks the correct line — plus the master PIC's IRQ2 cascade, required for any slave-PIC IRQ to reach the CPU — for whichever channel was actually detected
- `ata_irq_handler()` is intentionally minimal: it acknowledges the drive's IRQ (reading Status clears the line) and flips the waiting process back to `PROCESS_READY`; it never touches the scheduler or performs a context switch itself
- `ata_wait_irq()`: `cli` → check an "IRQ already fired?" flag → if not yet fired, mark the process `PROCESS_BLOCKED` and call `scheduler_block_current()`. The check and the block happen inside the same `cli`/`sti` section, so an IRQ that fires before the process finishes blocking is never lost — it's just observed as "already fired" instead
- Exclusion gate (`ata_gate_acquire`/`ata_gate_release`): since the calling process no longer holds the CPU for the whole operation, a second process could otherwise issue a competing ATA command on the same registers while the first is still waiting on its IRQ. The gate serializes access; a second process waiting for the gate also blocks for real via `scheduler_block_current()` (no busy-wait), rechecking the gate after being woken since more than one waiter can be released at once
- New process state `PROCESS_BLOCKED` (`process.h`) — kept distinct from `PROCESS_SLEEPING` so the timer's tick-based wake-up (`process_wake_sleepers`) never touches it — and `scheduler_block_current()` (`scheduler.c`), a sibling of `scheduler_yield()` that switches away from the CPU without forcing the state back to `READY`; whoever owns the wait (the IRQ handler, or the process releasing the gate) is responsible for that transition
- Both the IRQ handler and the gate re-check that a waiter is still `PROCESS_BLOCKED` before waking it, so a process killed (`kill <pid>`) while waiting can't have its (possibly already reused) process-table slot resurrected by a late wake-up
- `probe()`, disk detection, `fat16.c`, and `vfs.c` are unchanged

### Program execution and foreground control
- `SYS_EXEC (10)`: receives a user virtual pointer to the program name; `sys_exec` copies the string byte by byte from user space via `vmm_get_phys_from_dir(cur->cr3, vaddr)` (identity-map), calls the kernel's `exec()`, and returns the new process's PID or -1
- The shell stores the returned PID in `foreground_pid`; typing another command resets `foreground_pid`
- **Ctrl+C**: IRQ1 detects scancode `0x1D` (Ctrl press/release) and `0x2E` (C); injects `0x03` into the ringbuffer; `SYS_READ` returns immediately with `buf[0]=0x03` and echoes `^C\n`; the shell calls `sys_kill(foreground_pid)` and resets the PID

### `fork()`

- Full duplication (not copy-on-write): a fresh page directory, a fresh physical page for every page the parent has mapped (code, data, stack — whatever's actually present, not a fixed list of regions), and a duplicated fd table entry for every open file
- `process_fork()` (`process.c`) does the heavy lifting:
  1. Atomically claims a free process-table slot under `cli`/`sti` (assigns the pid and sets `PROCESS_BLOCKED` — reserved but not runnable — in one uninterruptible step), so two `fork()`s interleaved by the preemptive timer can't pick the same slot
  2. Walks the parent's page directory beyond the two shared kernel PDEs, allocating a new physical page and calling `vmm_map_user_page()` (the same helper `exec()` uses) for every present mapping
  3. Fabricates the child's kernel stack: the 13-word frame captured from the parent's syscall entry (8 `pusha` registers + the CPU's ring3→ring0 trap frame), with `eax` forced to 0, preceded by the same 4-dummy-words-plus-return-address prologue `build_initial_stack()` uses for brand-new processes — except the return address is `isr128_resume` (a new label in `isr.asm`, right before the existing `popa`/`iret`) instead of a bootstrap function
  4. Only then flips the child to `PROCESS_READY`
- The first time the scheduler runs the child, `context_switch()`'s `ret` lands on `isr128_resume`, which does `popa; iret` off that fabricated frame — resuming in ring 3 at the exact instruction after the parent's `fork()` call, with `eax=0`
- `g_syscall_frame` (`syscall.c`): set by `isr128` right after `pusha` to a pointer at the 13-word frame above; `sys_fork()` copies it into a **local** buffer as its very first action, before anything that could be preempted, and nothing else ever reads the global again for that call — otherwise a different process's syscall entry could overwrite it mid-copy and corrupt the child being built
- `SYS_FORK (25)`: after `process_fork()` succeeds, duplicates `fd_table[parent_slot]` into `fd_table[child_slot]` (`vfs_fd_t` is plain data with no owned/shared resource, so a shallow copy is safe — each fd then tracks its own read/write position independently, not POSIX's shared-offset semantics)
- Slot exhaustion or an out-of-memory mid-copy both return -1 to the parent with no process left behind; a partial address-space copy is unwound by walking the child's own (partially built) directory and freeing its data pages, since `process_fork()` runs on the caller's small fixed-size kernel stack and can't afford a separate allocation-tracking array
- Known, explicitly out-of-scope limitations (documented in code comments, not fixed here): `process_spawn()`/`process_spawn_user()` still claim a free slot without `cli`/`sti` (same class of race `process_fork()` closes for itself); `process_exit()` never frees a process's `cr3` or its mapped pages, forked children included; the page-copy assumes physical pages fall in the identity-mapped first 8MB, the same assumption `elf_load()` already makes
- `exec()`, `elf.c`, and `scheduler_yield()`/`scheduler_block_current()` are unchanged — `fork()` reuses the existing scheduler rather than adding a parallel path
- `user/forktest.c`: calls `fork()` and prints "I'm the parent, child=PID" or "I'm the child, pid=PID" depending on the return value

## Structure

```
boot/
  boot.asm            Multiboot2 header + _start
  linker.ld           Memory layout (kernel @ 0x100000)
kernel/
  main.c              kmain: initialization and the scheduler loop
  gdt.c/asm           Global Descriptor Table
  idt.c               Interrupt Descriptor Table + exception handler
  isr.asm             Exception stubs and the syscall gate (isr128)
  pic.c               8259 PIC
  timer.c             PIT 100 Hz
  keyboard.c          PS/2 keyboard
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
  init.c              simple user process: SYS_WRITE + SYS_EXIT
  spintest.c          process without yield: validates IRQ0 preemption
  shell.c             interactive shell: help/uname/fetch/ps/mem/ls/touch/echo/kill/run/edit/clear/exit
  edit.c              text editor: opens/creates/saves files on FAT16
  forktest.c          calls fork(), prints the parent/child paths and PIDs
  link.ld             user linker script (entry @ 0x01000000)
  Makefile            builds init.elf, spintest.elf, shell.elf, edit.elf, and forktest.elf
tools/
  Makefile            Build system (i686-elf-gcc + NASM + grub2-mkrescue), `disk` target
  grub.cfg            GRUB configuration
  make_disk.sh        generates build/disk.img (FAT16, 32 MB) if it doesn't already exist
build/                Build artifacts (git-ignored) — includes disk.img (persists across builds)
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

3. Add it to `grub.cfg`:
   ```
   module2 /boot/ramfs.img
   ```

## Technical specs

- Architecture: x86 32-bit (i686)
- Language: C99 + NASM
- Boot: Multiboot2 via GRUB2
- Toolchain: i686-elf-gcc, i686-elf-ld, NASM

## License

MIT
