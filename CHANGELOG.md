# Changelog

All notable changes to NullOS are documented in this file, grouped by the
version/phase they shipped in. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

This file was reconstructed retroactively from `git log` (commit messages
and diffs) cross-referenced with what `README.md` documented at each point
in the project's history. Where the commit history doesn't have enough
detail to say exactly what changed, that's stated explicitly instead of
being guessed at. See `PROGRESS.md` for the architecture-decision/tech-debt
memory this changelog doesn't duplicate.

**A note on version numbers:** the project's own README version banner
was bumped inconsistently in a few places, and was never bumped at all
past `v0.10.1` even though Phases 11–13 were completed afterward — until
Phase 14 (below), which is the first phase since Phase 10 to actually
update the banner (now `v0.14.0`). Those older inconsistencies are
called out inline rather than silently "corrected", and `[0.11.0]`–
`[0.13.0]` for Phases 11–13 are this changelog's own numbering
(following the `Phase N → v0.N.0` pattern the project used through
Phase 10), not a version string that ever actually appeared in the repo
at the time.

## [0.14.1] - Documentation reorganization + selftest tool

Not a new phase — intermediate work between phases, versioned as a PATCH
bump under the convention established here: PATCH is for work that
doesn't change user-facing behavior or complete a phase (doc
reorganization, test tooling, small additive functions), MINOR stays
reserved exclusively for a completed phase number (see CLAUDE.md,
"Convenções de fim de fase (versionamento)").

### Changed
- Documentation reorganized out of a single growing `README.md`:
  `README.md` is now a lean index (overview, completed-phases table
  0–14, links, build instructions); `ROADMAP.md` holds the detailed
  planning for future Phases 15–21 (moved verbatim); `docs/*.md` holds
  one file per technical system (`kernel.md`, `memory.md`,
  `scheduler.md`, `syscalls.md`, `filesystem.md`, `security.md`,
  `pci.md`, `shell.md`, `testing.md`, plus the pre-existing
  `setup.md`). No technical content was dropped, only moved — see
  `CLAUDE.md`'s "Documentação" section for where new content should go
  from here on.
- `kernel/syscall.c`: `SYS_PCI_LIST` now returns the PCI device count
  (via the new `pci_device_count()`, see below) instead of always
  `0`. Additive — the shell's `lspci` command already discarded the
  return value, so no existing caller's behavior changes.

### Added
- `user/selftest.c`: automated regression suite, runnable via
  `run selftest` from the shell. 7 checks (memory/`SYS_MEMINFO`,
  `fork()` PID validity, file create, file write/read roundtrip, the
  Phase 10 duplicate-create regression, invalid-pointer rejection via
  the Phase 14 validation path, PCI enumeration ≥ 1 device) plus one
  informational cleanup note (no delete/unlink syscall exists yet).
  See `docs/testing.md` for the full breakdown and known limitations.
- `kernel/drivers/pci.c`/`pci.h`: `pci_device_count()` — returns the
  device count from the last `pci_scan_bus()` call without rescanning,
  added so `user/selftest.c` can assert "found ≥ 1 device"
  programmatically instead of parsing `pci_print_list()`'s VGA output.

## [0.14.0] - Phase 14: kernel memory-safety hardening

Fixes 4 confirmed bugs found in a memory-safety audit, all sharing the
same root cause: a syscall receiving a userland pointer and reading or
writing through it without validating that the address actually
belongs to (and is accessible by) the calling process. Since every
process's page directory clones the kernel's own PDE0/PDE1 (the
identity-mapped first 8MB — kernel heap, page tables, IDT, ...), any of
these let a ring 3 process read or corrupt kernel memory it was never
meant to touch — a trivial privilege escalation primitive, not just a
crash bug. This phase also extends the same fix to the rest of the
syscall surface that touches a userland pointer (filename/argument
strings), fixes the `kmalloc()` bug the `sys_write_file` fix depends
on, removes unrelated leftover debug output found along the way, and
closes out with a build-tooling cleanup (a single source of truth for
the version string) — all landed as part of this one phase/version,
not spread across several.

### Added
- `kernel/syscall.c`: `user_ptr_valid(cur, uaddr, len)` — checks that
  every page in `[uaddr, uaddr+len)` is mapped in the calling process's
  own address space AND carries `VMM_USER` (not just "present" — the
  shared kernel identity map is present in every process's directory
  too, just not `VMM_USER`)
- `kernel/syscall.c`: `copy_from_user()` / `copy_to_user()` — validate
  a range with `user_ptr_valid()` and only then copy it byte-by-byte
  via the existing `user_kptr()` translation; copy nothing if any part
  of the range is invalid (no partial copies on failure)
- `kernel/memory/vmm.c`/`vmm.h`: `vmm_get_user_phys_from_dir()` — like
  the existing `vmm_get_phys_from_dir()`, but returns 0 unless the PDE
  *and* PTE both have `VMM_USER` set. This is the actual mechanism
  `user_ptr_valid()` (and, below, `user_kptr()`) is built on: without
  the `VMM_USER` check, a validator that only tested "is this address
  mapped at all" would still treat the shared kernel identity map as a
  valid target.
- `kernel/version.h`: single source of truth for the OS version
  (`NULLOS_VERSION`, `NULLOS_PHASE`, `NULLOS_PHASE_DESC`, and the
  composed `NULLOS_BANNER`/`NULLOS_SHORT_BANNER`) — see "Changed"
  below for why and where it's used.

### Fixed
- **`sys_write`** — read `buf[0..len)` directly from a raw userland
  pointer with no validation; a process could point it at kernel memory
  (heap, page tables) and have the kernel print it out, or at any
  unmapped address and crash the whole machine (see the `idt.c` note
  below). Now validates the whole range with `user_ptr_valid()` before
  reading a single byte.
- **`sys_read`** — wrote into a raw userland pointer with no
  validation, for both the keyboard path and the file-read (VFS) path;
  this was a write primitive into arbitrary kernel memory from ring 3,
  the most severe of the four. Now stages file reads through a small
  bounded kernel buffer and keyboard input through single-byte kernel
  locals, copying each chunk out via `copy_to_user()`.
- **`sys_write_file` + `kmalloc()`** — two chained bugs: (a) `kmalloc`'s
  `size = (size + 3) & ~3U` alignment step overflowed for a `size` near
  `UINT32_MAX`, silently handing back a much smaller block than
  requested; (b) `sys_write_file`'s copy loop then used the *original*
  (unclamped) `len` to copy userland data into that undersized block —
  a real, exploitable kernel heap overflow with attacker-controlled
  length and content. Fixed in both places: `kmalloc()` now rejects any
  `size` above `HEAP_MAX - HEAP_START` before doing any arithmetic on
  it (so the alignment step can no longer wrap), and `sys_write_file`
  separately rejects `len` above a 64KB cap before ever calling
  `kmalloc`, then uses `copy_from_user()` instead of a manual
  unvalidated per-byte loop.
- **`sys_meminfo`** — wrote its three `uint32_t` results directly
  through raw userland pointers with no validation, an arbitrary
  4-byte write to any address in the calling process's page directory.
  Now takes the three addresses as plain `uint32_t` and writes each one
  out via `copy_to_user()` (a `0` address is treated as "skip this
  output", matching the previous NULL-pointer-skips-it behavior).
- **`sys_open`, `sys_create`, `sys_exec`, `sys_getarg`** — same root
  cause as the four bugs above, just reached through a filename or
  `SYS_EXEC` argument string instead of an arbitrary-length buffer:
  `user_kptr()` (the byte-resolution helper `copy_from_user()`/
  `copy_to_user()` above are built on, and that `copy_user_str()` — the
  string-copy helper these four syscalls use — is also built on)
  resolved addresses through the plain `vmm_get_phys_from_dir()`
  (present-only, no `VMM_USER` check). `user_kptr()` now resolves
  through `vmm_get_user_phys_from_dir()` instead, which fixes all four
  syscalls at once with no change needed in any of them individually.

### Changed
- `kernel/syscall.c`: removed leftover debug instrumentation in
  `sys_open` (`serial_putchar('O')...'F'` markers) — temporary
  debugging left behind from an earlier session, against the project's
  own convention of removing it once the investigation is done. The
  now-unused `#include "serial.h"` was dropped along with it.
- Single source of truth for the version string: it had drifted
  independently in `kernel/main.c`'s boot banner, `user/shell.c`'s
  `fetch`/`uname` commands, and `tools/grub.cfg`'s menu entry — three
  places to remember to update by hand, which had already been missed
  twice. All three now read from the new `kernel/version.h`:
  - `kernel/main.c` prints `NULLOS_BANNER` instead of a literal string.
  - `user/shell.c` includes `kernel/version.h` directly (it's plain
    text macros, no kernel types/functions, so it's safe for a
    userland compilation unit to include) and uses
    `NULLOS_SHORT_BANNER`; `user/Makefile` gained `-I../kernel` so the
    include resolves.
  - `tools/grub.cfg` is no longer a static file — it's generated at
    build time as `build/grub.cfg` from a new `tools/grub.cfg.in`
    template by a `GEN grub.cfg` Makefile rule that pulls the version
    straight out of `kernel/version.h` via `sed`. The old
    `tools/grub.cfg` was renamed to `tools/grub.cfg.in`.
  - This is also the first time since Phase 10 that the boot banner
    was actually bumped — it had been stuck reading `v0.10.1 - Phase
    10` through Phases 11, 12 and 13 (see the version-numbers note at
    the top of this file), each of which shipped without updating it.
    The banner now reads `v0.14.0 - Phase 14: user pointer validation`,
    and from here on the banner's version is kept in lock step with
    the phase number as each phase lands, per the convention
    documented in CLAUDE.md.

## [0.13.0] - Phase 13: fork()

Not reflected in the README's own version banner (still reads `v0.10.1`
at this point in the repo) — see the note above.

### Added
- `fork()`: duplicates the parent's full address space (new CR3, new page
  directory/tables, physical pages copied page-by-page)
- Race-safe slot allocation for the new child process (`process.c`)
- File descriptor table duplication from parent to child
- `SYS_FORK` syscall, plus `user/forktest.c` demonstrating parent/child
  divergence via the syscall's return value
- Child process resumes via `isr128_resume` + `g_syscall_frame` (fabricated
  ring-3 return context), instead of the normal process bootstrap path —
  see `PROGRESS.md` for why
- `fork_free_address_space()`: unwinds a partially-built child's address
  space if the copy runs out of memory partway through

### Known limitations introduced/left open in this phase
- `process_spawn()`/`process_spawn_user()`'s free-slot scan is still not
  `cli`/`sti`-protected against a concurrent race — explicitly flagged as
  out of scope for this phase (see `PROGRESS.md`)

## [0.12.0] - Phase 12: IRQ-driven ATA

Not reflected in the README's own version banner — see the note above.

### Changed
- ATA read/write completion is now signaled by IRQ14/15 instead of
  busy-wait polling; the process blocks (`PROCESS_BLOCKED`) instead of
  spinning while waiting for the drive
- Added an exclusion gate around ATA operations so a second process
  can't issue a command while another is still waiting on its IRQ
- IDT gains the IRQ14/15 handlers; `isr.asm` wakes the correct waiting
  process from the IRQ context

## [0.11.0] - Phase 11: PCI bus enumeration

Not reflected in the README's own version banner — see the note above.

### Added
- PCI configuration-space access via the legacy Configuration Mechanism
  #1 (ports `0xCF8`/`0xCFC`)
- Bus enumeration at boot, building a device table
- `SYS_PCI_LIST` syscall and the `lspci` shell command, which reprints
  the table captured at boot without rescanning the bus

## [0.10.1] - Phase 10: English translation

### Changed
- Translated all code, comments, and UI/boot strings from Portuguese to
  English project-wide. No functional changes.

## [0.10.0] - Phase 10: persistent disk (ATA PIO + FAT16)

This entry also folds in the editor/`SYS_WAIT`/raw-mode/`SYS_GETARG` work
from the `editor: cursor correto, sem eco duplicado, SYS_WAIT, raw mode`
commit, which landed chronologically *before* this phase's own commit but
was never given its own version bump in the README — the `v0.10.0` README
diff is what first documents those syscalls (numbers 13–19 in the syscall
table), so this changelog follows the README's own grouping rather than
inventing an intermediate version number that never existed in the repo.

### Added
- `kernel/drivers/ata.c`: ATA PIO driver (no IRQ/DMA yet — that comes in
  Phase 12), probing all 4 possible slots (primary/secondary ×
  master/slave) via `IDENTIFY` (0xEC), skipping ATAPI devices
- `ata_read_sector`/`ata_write_sector`: LBA28 `READ SECTORS`(0x20) /
  `WRITE SECTORS`(0x30) + `CACHE FLUSH`(0xE7)
- `kernel/fs/fat16.c`: FAT16 driver — reads the BPB, caches the whole FAT
  in the kernel heap, `fat16_find`/`fat16_readdir` scan the root
  directory, `fat16_read_at` follows the cluster chain from an arbitrary
  offset, `fat16_write_file` frees the old chain and writes a new one,
  `fat16_create` writes a new (idempotent) root-dir entry
- `kernel/fs/vfs.c`: single dispatcher used by the syscalls — tries the
  (read-only) ramfs first, then FAT16; refuses writes to ramfs files
- `SYS_CREATE`, `SYS_WRITE_FILE`, `SYS_READDIR` syscalls
- `SYS_READ_RAW`, `SYS_GOTOXY`, `SYS_CLEAR`, `SYS_GETARG`,
  `SYS_KBD_FLUSH`, `SYS_SETCOLOR`, `SYS_SET_RAW_MODE`, `SYS_WAIT`
  syscalls
- `user/edit.c`: text editor with a real status bar/cursor, raw
  keyboard mode (no echo, no duplicated input), Ctrl+S save via
  `SYS_WRITE_FILE`, Ctrl+Q to close and exit; creates the file via
  `SYS_CREATE` if it doesn't already exist
- Shell: `touch` (creates an empty file via `SYS_CREATE`+`SYS_CLOSE`);
  `ls` lists ramfs and FAT16 entries separately
- `tools/make_disk.sh` + `make disk` target: generates
  `build/disk.img` (32 MB, FAT16 via `mkfs.vfat`) only if it doesn't
  already exist, so on-disk data persists across rebuilds

### Fixed
- **ATA probe race condition**: the reset-detection wait used a fixed
  ~400ns delay and assumed that was enough time for `BSY` to clear,
  instead of actually polling the status register's `BSY` bit with a
  real (~1s) timeout — the delay wasn't guaranteed to cover how long a
  real reset takes. Replaced with `wait_bsy_clear_after_reset()`, which
  polls the real bit.
- **Cache-flush false negative**: `ata_write_sector` used to be able to
  report a failure if the trailing `CACHE FLUSH` (0xE7) command timed
  out, even though the actual `WRITE SECTORS` payload had already been
  confirmed written beforehand. A flush timeout only means the drive
  didn't confirm the cache was committed to media within the deadline —
  it does not mean the write was lost — so the flush outcome is no
  longer propagated as a write failure (see `PROGRESS.md` for the full
  rationale, since this is easy to "fix" back into a false failure).
- **FAT16 dirent out-of-bounds name compare**: 8.3 filenames are stored
  as two separate fixed-size struct fields, `name[8]` and `ext[3]`.
  Comparing past the end of `name[8]` to reach the extension (rather
  than building an explicit 11-byte buffer from both fields) is
  undefined behavior in C, and produced a bug where debug prints showed
  the right bytes but the logical comparison still failed. Fixed by
  building an explicit `uint8_t[11]` buffer before comparing. The exact
  same inline comparison logic was duplicated in `fat16_write_file`
  (separately from `fat16_find`) and got the same fix applied there too
  — see `PROGRESS.md` for why this duplication is still flagged as tech
  debt going into Phase 16.

## [0.9.0] - Phase 9: `SYS_OPEN`/`SYS_CLOSE`/`SYS_READ` for ramfs files

### Added
- `SYS_OPEN` (`open(name) → fd`), `SYS_CLOSE` syscalls
- Per-process file descriptor table (`fd_table[PROCESS_MAX][8]`); fds
  0/1/2 reserved for stdin/stdout/stderr, files start at fd 3;
  `sys_exit` clears a process's fds on exit to avoid leaking slots
- `SYS_READ` becomes polymorphic: fd 0 still reads from the keyboard
  (blocking, with echo/backspace); fd ≥ 3 reads from a file opened via
  `SYS_OPEN`, advances the file position, and returns 0 at EOF

## [0.8.0] - Phase 8: `SYS_EXEC`, Ctrl+C, foreground PID

### Added
- `SYS_EXEC` syscall: copies the program name from user space via
  `vmm_get_phys_from_dir` (identity-map), calls the kernel's `exec()`,
  returns the new process's PID
- Shell tracks the `foreground_pid` of the last exec'd process; Ctrl+C
  in the keyboard handler sends it a kill instead of only working on
  the shell itself
- `run <prog>` shell command

## [0.7.0] - Phase 7: `SYS_READ` + interactive userland shell

### Added
- Keyboard ringbuffer (256 chars) fed from IRQ1; echo moved out of the
  IRQ handler and into `SYS_READ`
- `SYS_READ` (fd 0): polls the ringbuffer with `scheduler_sleep_current(1)`
  between attempts so it doesn't starve the scheduler; handles echo and
  backspace
- `user/shell.c`: interactive `> ` prompt loop (`sys_read` →
  `run_command`); commands `help`, `uname`, `fetch`, `ps`, `mem`,
  `echo`, `kill`, `clear`, `exit`
- `fetch`: ASCII banner with OS/Arch/Uptime/free PMM/free heap/running
  process count
- `SYS_UPTIME`, `SYS_MEMINFO`, `SYS_PS`, `SYS_KILL` syscalls

### Fixed
- `process_exit()` now actually frees the process's table slot on
  `SYS_EXIT` (previously the slot wasn't being reliably released)

## [0.6.0] - Phase 6: syscall return values + IRQ0 preemption

### Added
- `user/spintest.c`: a process that never calls `yield()`, used to
  validate that preemption actually works
- `scheduler_tick`/IRQ0-driven preemption: a process that hogs the CPU
  without yielding is now preempted automatically after a fixed tick
  slice

### Fixed
- `isr128` (the `int 0x80` handler) now writes `syscall_handler`'s
  return value into the `pusha` frame's `EAX` slot before `popa`, so
  the syscall's return value actually reaches userland in `eax` after
  `iret` (previously userland could see a stale/wrong `eax`). Userland
  inline asm needed `"=a"`/`"0"` constraints so the compiler doesn't
  assume `eax` is unchanged across `int $0x80`.

## [0.5.0] - Phase 5: ramfs + ELF32 loader + `exec()`

### Added
- Multiboot2 module tag parser
- Flat ramfs format: `[uint32_t n] [entry×n: name[32]+offset+size] [data...]`
  and `ramfs_find()` by name
- ELF32 loader: validates the magic, iterates `PT_LOAD` segments,
  allocates physical pages, maps them into the process's CR3, copies
  segment data
- `exec(name)`: ramfs lookup → new CR3 → `elf_load` → user stack →
  `scheduler_spawn_user`
- `user/init.c`: minimal user process (`SYS_WRITE` + `SYS_EXIT`) used
  to validate the whole ramfs → ELF → usermode pipeline
- `kmain` calls `exec()` for the GRUB-provided module(s) at boot

## [0.4.0] - Phase 3b + Phase 4: context switch, exception handlers, ring 3 usermode, syscalls

The project's own version banner used `v0.4.0` for **both** of these —
Phase 4 shipped without its own version bump (the README diff at the
Phase 4 commit changes the banner text but keeps the same `v0.4.0` it
already had from Phase 3b). Combined into one entry here rather than
inventing a version number the project never actually used.

### Added (Phase 3b)
- Per-process context switch and per-process CR3
- CPU exception handlers (previously only IRQs/syscalls were handled)

### Fixed (Phase 3b)
- Fixed a page-directory/stack collision in the VMM (`PAGE_DIR` was
  overlapping with stack memory)

### Added (Phase 4)
- TSS setup for a per-process kernel stack (SS0:ESP0)
- Ring 3 usermode via `jump_to_usermode` (`iret` with CS=0x1B, SS=0x23)
- Syscall gate: `int 0x80`, DPL=3, convention `eax=num, ebx/ecx/edx=args`
- `process_spawn_user()`

## [0.3.0] - Phase 3a: cooperative scheduler

### Added
- Process table
- Cooperative round-robin scheduler (no preemption yet — added in
  Phase 6)

## [0.2.0] - Phase 2: PMM, VMM, kernel heap

### Added
- Physical Memory Manager (page bitmap)
- Virtual Memory Manager: 32-bit paging with an identity-mapped low
  region
- Kernel heap (`kmalloc`/`kfree`, first-fit)

## [0.1.0] - Phase 1: GDT, IDT, PIC, PIT, PS/2 keyboard

### Added
- Global Descriptor Table (ring 0 + ring 3 code/data segments)
- Interrupt Descriptor Table with CPU exception handlers
- Remapped 8259 PIC (IRQs 0–15 → vectors 32–47)
- PIT configured at 100 Hz
- PS/2 keyboard driver

### Fixed
- Relocated the IDT to `0x200000` (commit message just says "IDT
  movida para 0x200000, Fase 1 completa"; the same fix message appears
  twice in the log in immediate succession — granular detail on what
  specifically was wrong at the original address isn't available from
  the commit history, so this is recorded honestly as "IDT relocated,
  reason not further documented" rather than invented)

## [0.0.1] - Phase 0: boot + VGA driver

### Added
- Multiboot2-compliant bootloader entry (`boot/boot.asm`, `linker.ld`)
- VGA text-mode (80×25, color) output driver
- Initial `README.md` and project scaffolding

Granular per-commit detail beyond this is not available — this very
early range of the project's history (before phase numbers were used
consistently in commit messages) was committed generically; grouping
here reflects that, rather than forcing an artificial split the commit
log doesn't actually support.
