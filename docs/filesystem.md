# ATA, FAT16, and VFS

## Files: open/read/close (ramfs + FAT16)
- `SYS_OPEN (11)`: copies the name from user space via `user_kptr`, allocates the first free slot in `fd_table[proc_slot][0..7]`, and calls `vfs_open` — which tries ramfs via `ramfs_find` and, if not found, FAT16 via `fat16_find`; returns `fd = 3 + idx`, or -1 if not found in either backend or no free slots
- `SYS_READ (5)` with fd≥3: dispatches by `fd->backend` via `vfs_read` — ramfs reads directly from `ramfs_base + offset + pos`, FAT16 uses `fat16_read_at` following the cluster chain; both advance `pos` and return 0 on EOF — non-blocking
- `SYS_CLOSE (12)`: marks the slot as free
- `fd_table[PROCESS_MAX][8]` — global table indexed by process slot; `sys_exit` clears all of the process's fds on exit, avoiding slot leaks
- fds 0/1/2 are reserved (stdin/stdout/stderr); files start at fd=3

## Persistent disk: ATA PIO + FAT16

- `kernel/drivers/ata.c`: ATA PIO driver — probes all 4 possible slots (primary/secondary × master/slave) by sending `IDENTIFY` (0xEC) and discarding ATAPI devices (signature `LBA_MID=0x14`/`LBA_HI=0xEB`); `ata_read_sector`/`ata_write_sector` do LBA28 with `READ SECTORS` (0x20) / `WRITE SECTORS` (0x30) + `CACHE FLUSH` (0xE7); waits for command completion via IRQ14/15 instead of polling when running inside a scheduled process — see "ATA IRQ-driven I/O" below
- `kernel/fs/fat16.c`: reads the BPB from sector 0, caches the whole FAT in the heap (`kmalloc`); `fat16_find`/`fat16_readdir` scan the root directory (8.3 names); `fat16_read_at` follows the cluster chain from an arbitrary offset; `fat16_write_file` frees the old chain, allocates a new one, and rewrites the FAT to disk; `fat16_create` looks for a free/deleted entry in the root dir and writes an empty dirent (idempotent — doesn't fail if the file already exists)
- `kernel/fs/vfs.c`: the single dispatcher used by the syscalls — `vfs_open` tries ramfs (read-only) and then FAT16; `vfs_create` calls `vfs_open` first and only creates on FAT16 if not found; `vfs_write` refuses to write to ramfs files
- `SYS_CREATE (23)`: open-or-create — copies the name from userland, tries to open it, and if it doesn't exist, creates the entry on FAT16 and returns the fd ready for writing
- Boot: `kmain` calls `ata_init()` and `fat16_init()` right after the scheduler; if there's no disk (or it's not valid FAT16), boot continues normally and on-disk file operations return -1 without crashing
- `tools/make_disk.sh` + the `make disk` target: generates `build/disk.img` (32 MB, FAT16 via `mkfs.vfat`) only if it doesn't already exist, preserving data across builds; `make run` (the `tools/Makefile` target — NOT `tools/run_qemu.sh`, a separate, older standalone script that boots the ISO without attaching any disk at all) attaches the disk as `-drive file=build/disk.img,format=raw,if=ide`
- Shell: `touch <name>` creates an empty file (`SYS_CREATE` + `SYS_CLOSE`); `ls` lists ramfs and FAT16 separately
- Editor: `load_file` now creates the file (`SYS_CREATE`) when it doesn't exist, keeping the fd open; Ctrl+S writes the whole buffer via `SYS_WRITE_FILE` and shows "saved" or "saved (no disk)" in the footer; Ctrl+Q closes the fd before exiting

## ATA IRQ-driven I/O

- Replaces phase 10's busy-wait: whenever `ata_read_sector`/`ata_write_sector` run inside a scheduled process, they block the calling process instead of polling `BSY`/`DRQ` while a disk command is in flight; the original polling path is kept as a fallback for calls with no current process (early boot, e.g. `fat16_init()` reading the BPB before the scheduler ever runs a process)
- IRQ14 (primary channel, vector 46) and IRQ15 (secondary, vector 47) stubs were added to `isr.asm`/`idt.c` following the existing IRQ0/IRQ1 pattern; `ata_init()` registers the handler and unmasks the correct line — plus the master PIC's IRQ2 cascade, required for any slave-PIC IRQ to reach the CPU — for whichever channel was actually detected
- `ata_irq_handler()` is intentionally minimal: it acknowledges the drive's IRQ (reading Status clears the line) and flips the waiting process back to `PROCESS_READY`; it never touches the scheduler or performs a context switch itself
- `ata_wait_irq()`: `cli` → check an "IRQ already fired?" flag → if not yet fired, mark the process `PROCESS_BLOCKED` and call `scheduler_block_current()`. The check and the block happen inside the same `cli`/`sti` section, so an IRQ that fires before the process finishes blocking is never lost — it's just observed as "already fired" instead
- Exclusion gate (`ata_gate_acquire`/`ata_gate_release`): since the calling process no longer holds the CPU for the whole operation, a second process could otherwise issue a competing ATA command on the same registers while the first is still waiting on its IRQ. The gate serializes access; a second process waiting for the gate also blocks for real via `scheduler_block_current()` (no busy-wait), rechecking the gate after being woken since more than one waiter can be released at once
- New process state `PROCESS_BLOCKED` (`process.h`) — kept distinct from `PROCESS_SLEEPING` so the timer's tick-based wake-up (`process_wake_sleepers`) never touches it — and `scheduler_block_current()` (`scheduler.c`), a sibling of `scheduler_yield()` that switches away from the CPU without forcing the state back to `READY`; whoever owns the wait (the IRQ handler, or the process releasing the gate) is responsible for that transition
- Both the IRQ handler and the gate re-check that a waiter is still `PROCESS_BLOCKED` before waking it, so a process killed (`kill <pid>`) while waiting can't have its (possibly already reused) process-table slot resurrected by a late wake-up
- `probe()`, disk detection, `fat16.c`, and `vfs.c` are unchanged

## Relevant files

```
kernel/
  drivers/ata.c/h     ATA PIO driver (LBA28, IRQ14/15-driven waits + exclusion gate)
  fs/fat16.c/h        FAT16 read/write over ATA
  fs/vfs.c/h          ramfs + FAT16 dispatcher
tools/
  make_disk.sh        generates build/disk.img (FAT16, 32 MB) if it doesn't already exist
```

See `PROGRESS.md` for the known FAT16 duplicated-lookup tech debt (`fat16_find` vs. `fat16_write_file`).
