# Safe Mode and the boot configuration sector (Phase 18-B)

Safe Mode is a recovery environment inside the **same kernel binary**, entered very early in `kmain()` when the previous boots kept failing (or when GRUB asks for it). It exists to survive bugs in exactly the subsystems it must not depend on, so it runs in ring 0 before the scheduler, `process_spawn_user`, `exec` and the syscall layer are initialized. This document holds the design as approved and what is implemented so far.

**Status: complete (pass 5 of 5).** Failure counter and entry condition, tier-1 text UI, tier-2 restricted shell, and the GRUB entries (Safe Mode, previous release) with the release tooling are implemented. Phase 18-B is closed; the GUI-debug / Text-mode entries wait for the GUI (Phase 26).

## Implemented in pass 5: the "previous release" GRUB entry and release tooling

The kernel of the last release stays bootable from the GRUB menu, covering what Safe Mode cannot: a bug in the boot code itself (paging, GDT/IDT, early init), which runs before any Safe Mode flag is even read.

- **Menu** (`tools/grub.cfg.in`): Default, serial debug mode, Safe Mode (`safemode` flag, since pass 2) and **"NullOS v<version> (previous release)"**, booting `/boot/prev-nullos.elf` with `/boot/prev-ramfs.img`. The entry is a block between `## PREV-BEGIN` / `## PREV-END` marker lines; `tools/Makefile`'s "GEN grub.cfg" rule fills `@PREV_VERSION@` from `tools/prev/VERSION` and deletes the block (and the ISO gets no `prev-*` files) when `tools/prev/` doesn't hold a complete snapshot. The title says "previous release" (English UI text) plus the version.
- **`tools/prev/`** (tracked in git on purpose, not ignored): `nullos.elf`, `ramfs.img` and a one-line `VERSION`. The kernel and the ramfs are kept **together** — the old kernel needs its own userland, because the syscall ABI must match.
- **`make snapshot`** copies the current `build/nullos.elf` and `build/ramfs.img` there and writes the version from `kernel/version.h`. It is never run by `all`, `run` or any other target. **Release routine:** after a release is tagged, on the tagged tree run `make clean && make && make snapshot` and commit `tools/prev/`; the *next* release's ISO then offers that release as its "previous release". (`make clean` only removes `build/`, so `tools/prev/` survives.)
- **Behavior to expect:** the old kernel does not know the failure counter, so booting it neither increments nor resets `boot_fail_count`. If you got there because of repeated failures the counter is still at the limit, and the next normal boot lands in Safe Mode again — from which "Reboot normally" resets it. That is the intended behavior.
- **Bootstrap:** the first snapshot (`tools/prev/`) is the v0.17.1 build (the last release before this feature), built from its tag in a temporary worktree — `make snapshot` did not exist at that tag, so its two outputs were copied by hand with the same layout. After 0.18.0 is tagged, the routine above refreshes it.

## Implemented in pass 4: tier 2, the restricted shell (`kernel/safemode.c`, `kernel/safeshell.c`)

Main menu item **5. Restricted shell (initializes disk access)**. Tier 1 never touches the PMM/VMM/heap; this item is the only place in Safe Mode that does, and only when chosen.

- **On-demand initialization**, in the normal boot's order: PMM (`boot_get_memory_map()` -> `pmm_init()`, then the ramfs module and the boot-info block reserved by their real addresses — two HAL accessors, `boot_get_module()` and `boot_get_info_region()`, were added for this), VMM (`vmm_init()`, paging), heap (`heap_init()`), FAT16 (`fat16_init()`). The scheduler is deliberately skipped: there are no processes. Failures that can be detected are reported and return to the menu without hanging: no usable memory (`pmm_free_pages() == 0`), an empty heap (`heap_init()` returns `void`, so `heap_free_bytes() == 0` is the check), no usable FAT16 (`fat16_init() == 0`). `vmm_init()` has no return value (it works or the machine faults).
- **Runs at most once per session.** Progress is tracked per step (`g_t2_stage`, `g_t2_fat_ready`): choosing the item again does not re-initialize anything, and a step that failed part way is retried from the step that did not finish (a second `vmm_init()` under a running kernel would rebuild the page tables). If the initialization crashes the machine, the counter is still at or above the limit, so the next boot is Safe Mode again.
- **The shell** (`safe> ` prompt, 128-byte static line buffer, backspace editing only, no history). Built-ins call the FAT16 functions directly in ring 0 — never `process_spawn_user`/`exec`, and it does not run ramfs programs even if one has the same name:
  - `help` — the command list;
  - `ls [dir]` — entries of the current directory or a path (`fat16_resolve_dir()` + `fat16_readdir()`), sizes in bytes, `<DIR>` for directories; ramfs is not listed;
  - `cat <file>` — prints a file (`fat16_find()` + `fat16_read_at()`, 512-byte chunks; non-printable bytes as `.`; ESC or Ctrl+C stops a long file);
  - `pwd` — `fat16_get_path()`;
  - `cd [dir]` — no argument = root;
  - `back` — returns to the menu; nothing is torn down, and the current directory is kept for the next visit.
  Unknown command: an error and the prompt continues.
- **Deliberately not here:** file writes/editing, delete (no `unlink` until Phase 21), an fsck-like check, and the Tier-1 screens (reboot, disk info, hexdump stay in the main menu). It is a read/navigation-only recovery aid so it cannot corrupt more.

## Implemented in pass 3: the tier-1 TUI (`kernel/safemode.c`)

Runs before the PMM/VMM/heap/scheduler, so it uses only the HAL (console, input, block I/O, power) and bootcfg, with **static buffers only** (a 512-byte sector buffer, a small input buffer; no `kmalloc`). All text is in the message table (`MSG_SAFE_*`). The main menu is redrawn every time you come back to it; an invalid key is ignored. There is no "continue booting": Safe Mode is left only by rebooting, never resumed in place (that would mean trusting the subsystems that may be why we are here).

1. **Reboot normally** — sets `boot_fail_count` to 0, writes it, `power_reboot()`. If the counter can't be written, it warns and reboots anyway.
2. **Reboot...** — submenu: *1. Normal (reset counter)* (same as item 1), *2. Safe Mode (keep counter)* (`power_reboot()` without touching the counter), *0 / ESC* back. Option 2 lands in Safe Mode again only while the counter is at or above the limit; if Safe Mode was entered with the `safemode` flag and the counter is lower, the next boot is a normal one, and the submenu says so. GUI debug / Text mode join this submenu with the GUI (Phase 26).
3. **Disk info** — reads LBA 0 raw (no FAT16 initialization, which would need the heap) and prints bytes/sector, sectors/cluster, reserved sectors, number of FATs, root entries, total sectors, sectors/FAT, the volume label and FS-type fields (when the extended boot record signature `0x29` is present) and whether the boot signature `55 AA` is valid; then whether the config sector is available and the current `boot_fail_count`. Any key returns.
4. **Sector hexdump** — asks for an LBA (decimal digits only, at most 10, Enter confirms, Backspace corrects, ESC cancels; empty, non-digit and overflowing input are rejected with a message and asked again), reads it and prints 16 bytes per line as hex plus an ASCII column (`.` for non-printable). A sector is shown in **two pages** of 256 bytes because 32 lines would not fit the 25-line screen; any key advances/returns. LBAs above `0x0FFFFFFF` are refused (the ATA driver is LBA28 and would silently alias).

The temporary `[BOOTCFG]` serial dump was removed from `kmain`; nothing extra is printed on a normal boot (the counter and config availability are visible in Disk info).

## Implemented in pass 2: counter, entry condition, stub

- **`ata_init()` moved.** It is still called exactly once, but now right after `sti` (after the keyboard) and before the PMM, so the log order is `[ATA]` before `[PMM]`. It needs the PIC, IDT and PIT (IRQ 14/15 registration, `timer_get_ticks()` for the BSY timeout). Before the scheduler runs, `process_current()` is NULL and `ata_read_sector()`/`ata_write_sector()` use their polling path (no IRQ wait, no process to block; bounded loops return -1 instead of hanging) — the same path `fat16_init()` has always used.
- **Counter and entry** (`kmain`): after the disk is up, `bootcfg_read()`. If the config sector is unavailable (no disk, or a disk without the reserved layout) the whole mechanism is skipped and boot is as before — there is nowhere to record failures. Otherwise: if `boot_fail_count >= BOOTCFG_FAIL_THRESHOLD` (3, in `bootcfg.h`) **or** the `safemode` flag is on the command line, `kmain` calls `safemode_enter()` **without touching the counter**; else it writes `boot_fail_count + 1` and continues booting.
- **Reset** (`sys_read`): on the first *keyboard* read (fd 0, after any stdin redirection is resolved) by any process — once per kernel lifetime, guarded by `g_boot_considered_up` — the counter is written back to 0. An interactive read only happens after a prompt was printed. (Reads from a redirected stdin don't count.)
- **The entry function** (`kernel/safemode.h/.c`, `safemode_enter()`, never returns): in this pass it was a one-action stub; pass 3 replaced it with the TUI above.
- Expected effect on the boot: the counter is 1 during boot and back to 0 once the shell asks for input. A boot that dies in between leaves it at >= 1; three in a row send the fourth boot to Safe Mode. Booting the "serial debug mode" entry does **not** enter Safe Mode (only the `safemode` word does; no GRUB entry passes it yet — pass 5).
- (The temporary `[BOOTCFG]` serial dump used to verify this pass was removed in pass 3.)

## Implemented in pass 1

### The config sector (`kernel/bootcfg.h/.c`)

A tiny `key=value` store in **one raw sector, LBA 1**, outside the filesystem, accessed only through the HAL's `block_read_sector()`/`block_write_sector()`. It does not use FAT16, the VFS or the heap, so it works when those are broken. One sector also makes a write atomic (it lands whole or not at all); if the config ever outgrows a sector, that is the signal to solve atomic writes properly.

- **Format** (text, NUL-padded to 512 bytes): the magic line `# nullos-config v1`, then `key=value` lines (`boot_fail_count=0`). Keys are `[a-z0-9_]{1,31}`; values cannot contain a newline. A sector that does not start with the magic line — a never-written disk, garbage, corruption — is read as an **empty config** (every get returns its default), never as an error; the terminator is forced so an unterminated sector can't be over-read.
- **API:** `bootcfg_read()` (1 valid / 0 empty-or-invalid / -1 unavailable), `bootcfg_write()`, `bootcfg_get_u32(key, default)`, `bootcfg_get()`, `bootcfg_set()`, `bootcfg_set_u32()`, `bootcfg_is_available()`. `set` only changes memory; nothing reaches the disk until `bootcfg_write()`. A `set` that would not fit in the sector fails and leaves the config untouched.
- **Availability guard:** LBA 1 is used only if the disk's boot sector (LBA 0, bytes/sector at offset 11, reserved sectors at offset 14) says the FAT16 reserved region is at least 2 sectors, i.e. sector 1 belongs to no filesystem structure. Otherwise the store is *unavailable*: gets return defaults and writes fail, so a foreign or unformatted disk is never written to.
- **Why LBA 1 works on existing disks:** FAT16's first FAT starts at LBA `reserved_sectors` (`fat16.c`: `g_fat_start_lba = bpb->reserved_sectors`). The disks made before this pass have 4 reserved sectors (the `mkfs.vfat` default here) and sector 1 is zero on them, so no disk needs to be recreated. `tools/make_disk.sh` now passes `-R 8` explicitly (the default varies by dosfstools version); `fsck.vfat` accepts the result.
- **Known caveat:** if a disk ever becomes a GRUB boot disk, GRUB's embedded core image lives right after the MBR and would collide with LBA 1. Boot is by ISO today.

### The boot command line (`kernel/multiboot2.h`, HAL)

The kernel used to ignore the Multiboot2 command line (the `debug` word of the "serial debug mode" GRUB entry was never read). Now `multiboot2_find_cmdline()` parses tag type 1, and the HAL gained:

- `boot_get_module()` / `boot_get_info_region()` (pass 4) — the ramfs module range and the boot-info block, so tier 2 can reserve them in the PMM;
- `boot_get_cmdline(out, max)` — the text after the kernel path in the GRUB entry (length, or -1 if none);
- `boot_has_flag("safemode")` — true if the word is one of the whitespace-separated words (exact match).

## Design for the remaining passes (approved)

- **Counter** (implemented in pass 2, see above). `boot_fail_count` in the config sector. Incremented right after the disk is up, before anything that can fail (i.e. `ata_init()` moves to just after `sti`, before the PMM; today it is called exactly once, in `kmain`, so this is a relocation, not a second call). If the count is already >= N (N = 3) at boot, or the `safemode` flag is present, `kmain` branches to Safe Mode **without incrementing further**. Reset to 0 when the system is considered up: the first `SYS_READ` on fd 0 by any process (an interactive read only happens after the prompt is printed; with the GUI in Phase 26 the criterion becomes "compositor ready"). Safe Mode's "reboot normally" zeroes the counter first. Failures before the disk is initialized (GDT/IDT/PIC/PIT/keyboard) cannot be counted.
- **Two tiers** (both implemented). The decision is taken before the PMM, so:
  - *Tier 1* uses only what is ready: console, keyboard, block HAL, power, PCI — menu, counter reset, reboot submenu, disk info, sector hexdump. No heap.
  - *Tier 2* (restricted shell with files) initializes the PMM, VMM, heap and FAT16 **on demand** from a menu entry (`fat16_init()` calls `kmalloc` for the FAT cache). If that crashes, the counter is still >= N, so the next boot lands in Safe Mode again.
- **TUI.** Numbered menu with submenus; destructive actions always go through their own confirmation screen; fsck-like operations split into verify-only vs verify-and-repair. Restricted shell: built-ins only, calling FAT16/HAL functions directly (no processes, no `run`), static `help` text.
- **GRUB** (implemented in passes 2 and 5). Entries: Default, serial debug mode, Safe Mode (`multiboot2 /boot/nullos.elf safemode`), and a permanent "previous release" entry. The GUI-debug and Text-mode entries wait for the GUI (Phase 26). The previous-release entry needs **both** the old `nullos.elf` and the old `ramfs.img` (the userland ABI must match the kernel), kept in a tracked `tools/prev/` and refreshed by a `make snapshot` step after each release tag.

## Files

```
kernel/bootcfg.h/.c     config sector (LBA 1), BOOTCFG_FAIL_THRESHOLD
kernel/safemode.h/.c    Safe Mode (tier-1 TUI; menu item 5 initializes tier 2 on demand)
kernel/safeshell.h/.c   Safe Mode restricted shell (tier 2)
kernel/hal.h/.c         boot_get_cmdline(), boot_has_flag()
kernel/multiboot2.h     cmdline tag (type 1) parser
tools/make_disk.sh      mkfs.vfat -R 8
tools/grub.cfg.in       menu template (Default, serial debug, Safe Mode, previous release)
tools/prev/             previous-release nullos.elf + ramfs.img + VERSION (make snapshot)
```
