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

## [Unreleased]

### Added

- **`exec()` runs programs from FAT16**, not only from the ramfs baked into the
  ISO. The program is found with `vfs_open()` — the lookup every file open
  uses: the ramfs first (so a disk file can never shadow a system program),
  then FAT16 resolved against the caller's cwd, so `run dir/prog.elf` works. A
  FAT16 program is read from disk whole by its directory-entry size (at most
  192 KB) into a temporary heap buffer, loaded and freed; the ramfs path stays
  zero-copy. No change to the on-disk format.
- **SDK for writing programs from outside the kernel tree:** `sdk/hello.c` (the
  template) and `sdk/Makefile`, which builds every `*.c` to `build/<name>.elf`
  reusing the system's own compiler flags, linker script (`user/link.ld`) and
  library sources, and `make inject PROG=<name>` to copy the result into
  `build/disk.img` without rebuilding the ISO. New guide `docs/sdk.md` for
  someone writing a program (rules, build, running it, the library, `printf`).
- **`printf` family in libnos** (`user/lib/nosstdio.c`): `printf`, `vprintf`,
  `sprintf`, `snprintf`, `vsnprintf` with the standard libc names — `%d %i %u %x
  %X %c %s %p %%`, flags `- 0 + space`, width and precision (also `*`), `h`/`hh`/`l`.
  No floating point, no 64-bit, no `#`. A separate object linked only into the
  programs that use it.
- **`make test-elf`** (`tools/test_elf_load.c`): a host-side test of the real
  `kernel/elf.c` on every built user program, plus truncation, header fuzzing and
  crafted hostile headers.
- selftest: four new tests (exec of a program that exists only on FAT16;
  malformed, truncated and missing programs rejected; the printf family) —
  22 tests.

### Changed

- `elf_load()` takes the file size and no longer trusts the image: the program
  header table and every segment's file data are checked against it, segments
  must lie in `[0x00800000, 0x02000000)`, and all checks use 64-bit arithmetic so
  a 32-bit field cannot wrap. All headers are validated before anything is
  mapped. This also closes the "`elf_load` never receives the file size / `page_end`
  overflow" item of the Phase 28 plan.
- The kernel heap is grown to 256 KB when it is initialized, and `heap_expand()`
  takes exactly the physical page at `heap_end` (`pmm_alloc_page_at()`, new)
  instead of the lowest free one — the heap's virtual addresses are the
  identity-mapped physical ones (see Fixed). The heap can no longer grow once
  processes exist, a limit recorded in `PROGRESS.md` for Phase 22.
- `tools/Makefile`: the user ELFs have the `user` target as an order-only
  prerequisite, so any target that needs them builds `user/` first.
- `CLAUDE.md`: a "Definition of Done" checklist (literal, not prose) at the top:
  what must be done in every subtask commit (a `docs/TODO.md` trace, a
  `[Unreleased]` CHANGELOG entry) and in the closing commit of a phase (the
  consolidated CHANGELOG entry as a blocking item, README, ROADMAP, `version.h`,
  TODO resolved into real docs, PROGRESS, syscall table, selftest), ending with
  "read this checklist item by item against the real state of the files".
  `docs/TODO.md` now lists the documentation owed by Phase 19.
- `ROADMAP.md`: a proposed phase "Crash handler leads into Safe Mode" is
  recorded (unnumbered, outside the priority order until it gets a place): a
  kernel crash saves its dump in the boot config sector, resets by itself and
  lands in Safe Mode with the reason and a "view last crash" item.

- Documentation split by audience: the new `docs/quickstart.md` covers only
  "download a release zip, install QEMU, run one command" (no toolchain, no
  Docker, no compiling), and `docs/setup.md` is now developer-only. `setup.md`
  was re-checked against `tools/Makefile` and the scripts: it now states that the
  Makefile runs `grub2-mkrescue` (the Fedora name) and that the Docker script
  installs neither `dosfstools` nor `python3` (both unverified), documents
  `make disk`, `make snapshot`, the exact `make run` flags (including `-display
  sdl`), the four-entry GRUB menu, that the `debug` boot argument is parsed but
  not acted on yet, and adds a "Branches and versions" section (`main` = last
  release with an annotated tag, `nightly` = daily work, the `-nightly` suffix,
  `tools/prev/`). `README.md` and `docs/kernel.md` now link the right document
  for each case.
  Follow-up: version numbers in the quickstart's explanatory text are now the
  placeholders `vX.Y.Z` (current) and `vA.B.C` (previous release) instead of
  hardcoded values; the QEMU installation section covers Linux, macOS (Homebrew,
  MacPorts) and Windows evenly; a new section explains running the ISO and disk
  image in other virtual machines (general requirements, plus step-by-step
  VirtualBox and VMware Workstation/Player, including the one-time raw-to-VDI/VMDK
  conversion — general guidance, not verified by the project); and `setup.md` now
  explains how to switch to the `nightly` branch to build the development
  version.
- `CLAUDE.md`: two release conventions made formal — every version closed and
  merged into `main` gets an annotated `vX.Y.Z` tag at the same moment, and right
  after the tag `make snapshot` is run on the tagged tree and `tools/prev/` is
  committed back to `nightly` before the next phase starts.

### Fixed

- A kernel page fault (`#PF` inside `vmm_get_user_phys_from_dir`) when the heap
  had to grow after a process existed. `heap_expand()` mapped `heap_end` — an
  address inside the identity-mapped 4–8 MB range — to the lowest free physical
  page, which repointed the kernel's view of the physical page at that address;
  if it held a process's page directory or page table, the kernel read another
  page's contents through it. Latent since the heap and processes shared that
  range; the first `kmalloc` large enough to grow the heap after boot (an
  `exec()` from FAT16) exposed it.
- `make run` (or any target that needs the user programs) right after `make
  clean` failed with "No rule to make target ../build/user/init.elf"; it now builds
  `user/` first.

## [0.18.0] - Phase 18: Safety/portability foundation (HAL, msg(ID), Safe Mode)

### Added

- **Hardware abstraction layer** (`kernel/hal.h`, `kernel/hal.c`,
  `docs/hal.md`): an arch-neutral interface over the existing drivers, as thin
  forwarding wrappers (the drivers were not rewritten).
  - Console: `console_putc`, `console_puts`, `console_put_hex`,
    `console_put_dec`, `console_set_color`, `console_clear`,
    `console_set_cursor`, `console_color_t` (`CONSOLE_*`).
  - Input: `input_poll_key`, `input_poll_raw`, `input_flush`.
  - Block device: `block_read_sector`, `block_write_sector`.
  - Power: `power_reboot`, `power_shutdown` (reachable through `hal.h`).
  - Boot info: `hal_boot_init`, `boot_get_memory_map`, `boot_get_module`,
    `boot_get_info_region`, `boot_get_cmdline`, `boot_has_flag`, backed by new
    Multiboot2 parsers (memory map, command line) in `kernel/multiboot2.h`.
    The kernel used to ignore the command line entirely (the `debug` word of the
    "serial debug mode" GRUB entry was never read).
- **`msg(ID)`**, a central table for all user-visible text (one English
  column; not a translation system): `kernel/messages.h/.c` for the kernel
  (boot log, errors, dumps, exception names, `ps` states) and
  `user/lib/messages.h/.c` for the shell, editor and `cat` (`umsg_id_t`,
  `UMSG_*`, linked only into those programs). `msg()` never returns NULL
  (`"(?)"` for a bad ID) and works from the exception handler. Output is
  unchanged. `selftest`/`forktest` and `init`/`spintest` are not migrated.
- **Safe Mode** (`docs/safemode.md`), a recovery environment inside the same
  kernel binary that runs in ring 0 before the PMM, VMM, heap, scheduler,
  `exec` and syscalls exist:
  - a boot configuration store in one raw sector (LBA 1, inside FAT16's
    reserved region, read and written only through the HAL block I/O, magic
    line `# nullos-config v1`, empty defaults when the sector is invalid,
    guarded by the boot sector's `reserved_sectors`) — `kernel/bootcfg.h/.c`;
  - a boot failure counter (`boot_fail_count`): incremented right after the
    disk is up, reset on the first keyboard read; at
    `BOOTCFG_FAIL_THRESHOLD` (3) failed boots in a row, or with `safemode` on
    the boot command line, the kernel enters Safe Mode instead of booting;
  - a text UI (`kernel/safemode.c`, static buffers, no heap): reboot normally
    (resets the counter), a reboot submenu, disk info from the raw boot
    sector, and a sector hexdump; Safe Mode is left only by rebooting;
  - a restricted read-only shell (menu item 5, `kernel/safeshell.c`) that
    initializes the PMM, VMM, heap and FAT16 on demand, once per session, and
    offers `help`, `ls`, `cat`, `pwd`, `cd`, `back` calling FAT16 directly —
    no processes, no `exec`;
  - GRUB entries "NullOS (Safe Mode)" (`safemode` flag) and "NullOS
    v<version> (previous release)", which boots the last release's kernel and
    ramfs kept together in the tracked `tools/prev/`; `make snapshot` records
    the current build there (run by hand after tagging a release, never
    automatically). `tools/prev/` holds the v0.17.1 build.

### Changed

- Everything outside the drivers goes through the HAL: `kmain`, `syscall.c`,
  `process.c`, `scheduler.c`, `exec.c`, `power.c`, `memory/{pmm,vmm,heap}.c`,
  `drivers/pci.c`, all disk access in `fs/fat16.c`, and `idt.c`'s exception
  handler and progress lines (the HAL console holds no state of its own, so
  this adds no risk there). Driver bring-up calls stay direct. `kmain`'s
  Multiboot magic check goes through `hal_boot_init()`.
- `ata_init()` runs right after interrupts are enabled, before the PMM (it is
  still called once), so the boot log shows `[ATA]` before `[PMM]`.
- `pmm_init()` consumes the bootloader's real memory map instead of one fixed
  contiguous block: only usable regions are freed (rounded inward to pages,
  fragmented maps supported), the first 1 MB and 1–4 MB stay reserved, and
  `kmain` marks the ramfs module and the Multiboot2 info block used by their
  real addresses. The allocation ceiling is an explicit 8 MB (`PMM_LIMIT_ADDR`,
  2048 pages): the kernel touches physical pages through its 0–8 MB identity
  map, so nothing above it is handed out. This is a mitigation of a
  pre-existing bug recorded in `PROGRESS.md` (fix: Phase 22); `[PMM] Total`
  in the boot log now shows the allocatable 8192 KB (it used to show the old
  compile-time 32768 KB cap).
- `tools/make_disk.sh` passes `-R 8` to `mkfs.vfat` so sector 1 is explicitly
  outside FAT16 (existing disks already have it: 4 reserved sectors).
- `PROGRESS.md` records two pre-existing problems found during this phase:
  the kernel writes to physical pages through the identity map without
  checking (`elf.c`, `process.c`; Phase 22), and `edit` with no file name
  cannot save (Ctrl+S reports the misleading "saved (no disk)").
- `kernel/version.h`: `0.18.0`, phase `18`, "Safety/portability foundation".

### Fixed

- `pmm_free_pages()` over-reported by the total page count since Phase 2:
  `pmm_used` started at 0 while the bitmap started all-used, so releasing a
  region drove it negative (the old boot log showed `Free: 61440KB` for
  `Total: 32768KB`). It now starts at the total; boot free is 1024 pages
  (4096 KB), the real figure.

## [0.17.1] - Documentation patch: v0.17.0 closing gaps + ROADMAP restructuring

Documentation only; no behavior change (`kernel/version.h` is 0.17.1, phase
still 17 / "Cleanup A").

### Changed

- `docs/testing.md`: the example selftest output still showed the old
  `13/13` with 13 `[PASS]` lines and the old cleanup note; it now shows the
  real 18/18 output (including the Intel 440FX check, the two-process pipe,
  the 3-child `waitpid` and the 3-level `mkdir`/`cd`) and the current
  `[INFO] cleanup` text.
- `README.md`: the "Build" section now lists `make run-reboot-test` (`make
  run` without `-no-reboot`, so `reboot` really restarts the guest) and
  `make inject` (copies a file into `build/disk.img` with `mcopy` without
  rebuilding the ISO; host-side only). The planned-phases range is 17–30.
- `ROADMAP.md`: restructured the end of the plan. The separate "technical
  prerequisites for DOOM" phase (old Phase 30) is gone: `lseek` moved into
  Phase 29 (general polish, next to `mv`/`cp`), the deferred Windows test of
  `docs/setup.md` was added there too, and the DOOM engine port (old Phase
  31) is now Phase 30 (sub-phases 30-A..D). It needs only Phase 26 and a
  one-shot single-block memory reservation syscall (DOOM's Z_Zone allocator
  asks for one big block at startup), not a full userland `malloc`/`free`.
  The plan now covers Phases 17–30, with v1.0.0 right after Phase 30.
  `PROGRESS.md` range updated to match. Phase 23's "Independent of Phases
  17–22" now reads "Independent of the other planned phases".
- Not part of this patch: the permanent "NullOS vX.Y.Z (anterior)" GRUB
  entry that CLAUDE.md requires at every merge into `main` was never
  implemented (neither in v0.16.0 nor v0.17.0). It is tracked for Phase
  18-B (Safe Mode and the 4-entry GRUB menu).

## [0.17.0] - Phase 17: Cleanup A (audit fixes, libnos/shell tools, test/build infrastructure)

### Added

- `ROADMAP.md`: granular phase table — one row per phase and
  sub-phase for completed Phases 0–16 (names taken from the README's
  completed-phases table, links to CHANGELOG versions and `docs/`
  files) and for planned Phases 17–31 including their sub-phases.
- `docs/TODO.md`: header-only file for minimal "WIP: document X"
  stubs left during `nightly` development, resolved at each version's
  final polish (per the new CLAUDE.md documentation rule).
- `user/lib/nullos.c/h`, `user/shell.c`, `forktest.c`, `selftest.c`,
  `edit.c`: libnos gained `memcpy`/`memset`/`memmove`/`memcmp`/`strlen`/
  `strcmp`/`strncmp` (standard libc names and signatures, so a call GCC
  emits by itself resolves) and `nos_uitoa`; the four per-program copies
  of `strlen`/`uitoa`, shell's `strcmp`/`strncmp`, selftest's `st_bufeq`
  and the inline zero/shift/copy loops in selftest and edit now call them.
- `user/selftest.c`: new test "SYS_WRITE >128 bytes accumulates in a FAT16
  file" — writes 2100 bytes as two `nos_write()` calls (crossing the first
  2048-byte cluster) and reads them all back; the regression test for the
  chunked-write data loss. Suite is now 14 tests (`st_big.txt` is left on
  disk like the other test files).
- `pwd` / `SYS_GETCWD` (30): `fat16_get_path()` rebuilds a cwd path from the
  cwd's cluster by walking up through `..` (names in 8.3 uppercase).
  `nos_getcwd()`, shell `pwd`.
- `cat <file>`: `user/cat.c` opens the file named by its argument and prints
  it; with no argument it still copies stdin (pipe sink). The shell's `cat
  <file>` launches it and waits. `sys_exec_pipe()` clears the global
  `exec_arg` so a piped/redirected `cat` never reads the argument left by an
  earlier plain `exec()`.
- `cmd < file` and `cmd > file` in the shell (`run_redirected()`): file fds
  through the existing `SYS_EXEC_PIPE`; `>` creates + truncates. External
  programs only, launched by name without arguments, not combinable with `|`.
- `kernel/power.c/h`: `power_reboot()` (8042 reset, port 0x64 <- 0xFE) and
  `power_shutdown()` (PIIX4 PM base from PCI config 0x40, `outw(base+4,
  0x2000)`; prints "shutdown not supported on this hardware" when the PIIX4
  isn't in the PCI table). `pci_find_device()`. Syscalls `SYS_REBOOT` (31) and
  `SYS_SHUTDOWN` (32), `nos_reboot()`/`nos_shutdown()`, shell `reboot`/
  `shutdown`.
- `user/selftest.c`: four new tests (18 total): a two-process pipeline
  (a `fork()`ed writer and `cat` launched with `SYS_EXEC_PIPE`, parent
  compares the output), `waitpid` with three children (each pid collected
  with the result that child reported, and gone afterwards), `mkdir`/`cd`
  three levels deep (pwd at every level, a file at the bottom, `cd ..` back
  to `/`), and a check for the Intel 440FX host bridge (`8086:1237`) next to
  the generic PCI count. The 440FX test is expected to fail once Phase 24
  moves QEMU to `-machine q35` (see `docs/TODO.md`).
- `SYS_PCI_FIND` (33) / `nos_pci_find(vendor, device)`: 1 if a device with
  that ID is in the PCI table, 0 if not (`pci_find_device()` for userland).
- `tools/Makefile`: `make inject FILE=... [NAME=...]` copies a file into the
  root of `build/disk.img` with `mcopy`, without rebuilding the ISO.
  Host-side only — the kernel still can't `exec()` from FAT16 (Phase 19).
- `tools/Makefile`: `make run-reboot-test` runs QEMU without `-no-reboot`, so
  `reboot` really restarts the guest. `run` and `debug` keep `-no-reboot` on
  purpose (post-mortem state on a triple fault; it also turns a guest reset
  into a shutdown).

### Changed

- `kernel/version.h`: `NULLOS_VERSION` `"0.17.0"`, `NULLOS_PHASE` `"17"`,
  `NULLOS_PHASE_DESC` `"Cleanup A"`.
- `user/shell.c`: `run` has a single implementation, `cmd_run()`, shared by
  the foreground path and `run_command()`; it trims and validates the name.
- `ROADMAP.md`: replaced the Phases 17–22 plan with the restructured
  Phases 17–31 sequence (Cleanup A, HAL + Safe Mode, SDK, COW fork,
  `unlink`/`rmdir`, `process_exit()` memory release, `e1000`, AHCI,
  xHCI, framebuffer/GUI, syscall deprecation, audit pass 2, polish,
  DOOM prerequisites, DOOM port / v1.0.0). Old Phases 17–22 renumbered
  to 20, 23, 24, 25, 26, 27, with a granular one-row-per-phase/sub-phase
  table (historical lettered phases as parent + child rows). No
  package-manager phase, by decision.
- `README.md`: "Completed phases" table shows only whole phases (the old
  rows 2/2b and 3a/3b merged into one row each); the "planned phases" range
  is 17–31.
- `CLAUDE.md`: new sections/rules for the `nightly`/`main` branch strategy,
  `-nightly` version suffix, sub-phases, HAL, centralized `msg()` text
  output, key=value system config file, and Safe Mode; serial-mirroring,
  exec()/fork() threading and docs-verification rules extended; Safe Mode
  rules name `process_spawn_user`.
- `PROGRESS.md`: consolidated from 411 to ~150 lines (closed phases one
  line each, decisions tightened with links to `docs/`).
- `docs/`: `memory.md`, `filesystem.md`, `scheduler.md`, `pci.md`,
  `kernel.md`, `shell.md`, `pipes.md`, `testing.md`, `syscalls.md` updated
  for everything above; `docs/scheduler.md` no longer describes the removed
  `process_spawn()`.

### Fixed

- `kernel/memory/pmm.c`: `pmm_init()` computed the page count as
  `(1024 + mem_upper) * 1024 / PAGE_SIZE`, which overflows `uint32_t`
  for a huge `mem_upper` and wraps to a tiny `total_pages`, underflowing
  `total_pages - 256`. Now `256 + mem_upper / 4` (same value, no
  overflow), and freeing high memory is skipped with a warning when
  `total_pages <= 256`.
- `kernel/fs/fat16.c`: `fat16_init()` now rejects a BPB with
  `sectors_per_cluster == 0` (printing the reason) before it is used as
  a divisor, instead of dividing by zero.
- `kernel/process.c`: `next_pid++` (three places) now goes through
  `alloc_pid()`, which saves/restores EFLAGS around the increment
  instead of a bare cli/sti, since `process_fork()` calls it from
  inside its own cli section.
- `kernel/drivers/pci.c`/`pci.h`: only the BARs a header type really
  has are read and printed (type 0: 6, type 1 PCI-PCI bridge: 2,
  type 2 CardBus: 1), with the multi-function bit (0x80) masked off
  first; the rest of `bar[]` stays 0.

- `kernel/memory/vmm.c`/`vmm.h`: `vmm_map_page()` and
  `vmm_map_user_page()` now return `int` (0 = success, `VMM_ERR_RANGE`
  / `VMM_ERR_NOMEM` on failure) instead of failing silently as `void`;
  `map_page_early()` reports an exhausted page-table pool.
  `vmm_map_user_page()` also rejects `virt < 0x800000` (the kernel's
  shared identity map) and frees a page-table page it can't use.
  Every call site checks the result: `heap_expand()` frees the page and
  returns 0; `exec()`'s user-stack loop prints an error and returns 0;
  `elf_load()` returns -1; `process_fork()` unwinds via its existing
  `failed` path. The three `vmm_map_user_page` sites also free the
  just-allocated physical page instead of leaking it.

- `kernel/keyboard.c`, `user/edit.c`: Shift in the editor.
  The raw scancode path (`SYS_READ_RAW`) now carries a Shift bit
  (bit 9, next to Ctrl's bit 8), because the kernel consumes the Shift
  make/break scancodes itself and `edit.c` could never see them; the
  editor selects a new `sc_map_shift[]` table from it, so Shift+5 gives
  `%`, Shift+\ gives `|` and Shift+letter gives the capital.

- `kernel/fs/fat16.c`: `fat16_write_file()` again refuses
  a directory entry (`ATTR_DIRECTORY`) — the Phase 15 switch to the
  shared `dir_lookup()` had dropped the old loop's directory skip — and
  no longer re-reads the dirent sector `dir_lookup()` just left in
  `dir_buf`. (The "duplicated dirent lookup" tech-debt item was already
  resolved by Phase 15; only these two leftovers remained.)
- `kernel/process.c`, `kernel/scheduler.c/h`, `kernel/process.h`
 : `process_spawn_user()` now reserves its slot atomically
  (interrupts off via saved EFLAGS, slot marked `PROCESS_BLOCKED`, pid
  and `waiting_for_pid` reset in the same section), fills in every
  field, and only then publishes `PROCESS_READY` (or leaves it
  `PROCESS_BLOCKED` for `start_blocked`). This closes two races: two
  spawns picking the same free slot, and the scheduler running a slot
  whose `esp`/`cr3` weren't built yet. New `irq_save()`/`irq_restore()`
  helpers, also used by `alloc_pid()`.

- `kernel/fs/fat16.c`, `fat16.h`, `vfs.c`, `vfs.h`, `kernel/syscall.c`:
  `SYS_WRITE` on a FAT16 fd lost data. `sys_write()` staged writes in
  128-byte chunks and each `vfs_write()` replaced the WHOLE file, so only
  the last chunk survived a write over 128 bytes. New `fat16_write_at()`
  (positional write: grows the chain, updates the dirent, data -> FAT ->
  dirent order) backs a stream `vfs_write()` that writes at `fd->pos` and
  advances it; the whole-file replace `SYS_WRITE_FILE` needs moved to
  `vfs_write_all()`, so the editor's save is unchanged.
- `user/shell.c`: a bare `edit` or `cat` (no argument) printed `command not
  found`. `nos_read()` returns the line with its trailing newline, and the
  branches only match "name followed by a space or end of string". The
  shell now strips the trailing `\n`/`\r` right after reading the line.
- Line editing echo (pre-existing): erasing a typed character did not
  erase it on the serial console. `vga_putchar('\b')` blanks the cell on
  screen, but its serial mirror sent a raw `\b`, which only moves a
  terminal's cursor left — retyping `shutdown` as `reboot` showed
  `rebootdows`. `kernel/drivers/vga.c` now mirrors an erasing backspace as
  `\b \b` (nothing if VGA erased nothing). Also `sys_read()` no longer
  echoes a backspace on an empty line, which used to blank the shell's own
  `> ` prompt. The typed buffer itself was always right (a backspace just
  decrements the count).

### Removed

- `tools/run_qemu.sh`: it booted the ISO without attaching `disk.img` and had
  no users besides one hint in `tools/setup_env.sh` (now points at `make
  run`); its mentions in `docs/setup.md` and `docs/filesystem.md` are gone.
- `process_spawn()`, `scheduler_spawn()` and the now-unused
  `scheduler_task_bootstrap()`: dead code (no callers anywhere, no
  future roadmap phase depends on them).

## [0.16.0] - Phase 16: pipes and real waitpid

Confirmed via manual QEMU testing (`forktest | cat`), including
serial-only temporary instrumentation (removed once confirmed) in
`sys_wait()`/`pipe_read()`/`pipe_write()`/`sys_exec_pipe()`: both
`stdin_redirect`/`stdout_redirect` were set on the two spawned
processes (not left at `-1`), real bytes flowed through
`pipe_write()`/`pipe_read()` (not a VGA bypass), `sys_wait()` genuinely
blocked on each pid until it exited, and the shell's `> ` prompt only
reappeared after both children had actually exited.

### Added
- **Inter-process pipes and a real `waitpid()`, with the shell gaining
  `cmd1 | cmd2`.**
  - `kernel/pipe.c/h` (new): a fixed pool of `PIPE_MAX=8` pipes, each a
    static 512-byte circular buffer (no `kmalloc`, nothing to leak —
    matches `process_table`/`g_gate_waiters`'s existing static-pool
    style), with the same `PROCESS_BLOCKED`/`scheduler_block_current()`
    blocking pattern `kernel/drivers/ata.c`'s `ata_wait_irq()` already
    established, applied symmetrically to both directions, plus
    refcounted ends so a writer/reader exiting or closing wakes the
    other side into EOF / broken-pipe instead of a permanent block.
    Single waiter per direction, deliberately (matches `ata.c`'s own
    `g_irq_waiter` precedent — `cmd1 | cmd2` never needs more than
    one reader/one writer per pipe). See `docs/pipes.md` for the full
    design.
  - `kernel/fs/vfs.h/.c`: two new backends, `VFS_PIPE_READ`/
    `VFS_PIPE_WRITE`, plus a new `vfs_dup()` (bumps a pipe's refcount
    when its fd is duplicated by `fork()` or `SYS_EXEC_PIPE` — a
    no-op for ramfs/FAT16, which were never refcounted).
  - New syscalls `SYS_PIPE (28)` and `SYS_EXEC_PIPE (29)`. Launching a
    pipeline stage deliberately does **not** use `fork()` +
    `dup2()` + `exec()` (the POSIX idiom) — NullOS's `exec()` spawns a
    brand-new process rather than replacing the caller's image, so
    that idiom would leave a stray extra process per stage, the same
    wrong assumption that caused the Phase 15 `cwd_cluster` bug.
    `SYS_EXEC_PIPE(name, stdin_fd, stdout_fd)` threads the redirect
    through `exec()`'s existing parameter chain instead (the same
    shape `cwd_cluster` was threaded through in Phase 15), seeding the
    new process's `fd_table` directly since `exec()` doesn't copy it
    the way `fork()` does.
  - `process_spawn_user()` gained a `start_blocked` parameter (also
    threaded through `scheduler_spawn_user()`/`exec()`): `SYS_EXEC_PIPE`
    spawns the new process `PROCESS_BLOCKED`, seeds its `fd_table`, and
    only then calls the new `process_make_ready()` — closing a real
    race window (a preemptive tick between spawn and seeding could
    otherwise run the process with a redirect pointing at nothing),
    the same discipline `process_fork()` already uses for its own
    child.
  - `process_t` gained `stdin_redirect`/`stdout_redirect` (`-1` =
    default keyboard/VGA, unchanged behavior for everything but
    `SYS_EXEC_PIPE`-launched processes) — `sys_read()`/`sys_write()`
    resolve fd 0/1/2 through these before doing anything else, so a
    piped program needs zero pipe-awareness of its own (see
    `user/cat.c`).
  - **`SYS_WAIT`'s interface didn't change** (it already took a
    specific pid) — its implementation did: real blocking
    (`process_t.waiting_for_pid` + `PROCESS_BLOCKED`, woken by
    `process_exit()`) instead of polling every 100ms
    (`scheduler_sleep_current(10)`).
  - **Fixed**: `sys_exit()`'s fd cleanup used to zero
    `fd_table[slot][j].used` directly, bypassing `vfs_close()` —
    harmless before (ramfs/FAT16 had nothing to release), but the only
    path that would ever release a pipe end on process exit. Now calls
    `vfs_close()` per used fd. `sys_fork()`'s fd-table duplication now
    also calls `vfs_dup()` on each copied entry, for the same
    refcounting reason.
  - `user/lib/nullos.h/.c`: `nos_pipe()`, `nos_exec_pipe()`.
  - `user/shell.c`: `cmd1 | cmd2` (`run_pipeline()`) — creates the
    pipe, launches both stages via `nos_exec_pipe()`, and (critically)
    closes the shell's own copies of both raw pipe fds before waiting
    on either child, since nothing else would ever drop their
    refcounts to 0 otherwise (see `docs/pipes.md`).
  - `user/cat.c` (new): minimal pipe sink (reads stdin, writes
    stdout) — added because no existing program could meaningfully
    sit on either end of a real pipe (the shell's builtins write
    straight to VGA, never through fd 1). Used for the manual
    `forktest | cat` test (`docs/testing.md`).
  - `user/selftest.c`: two new tests (pipe write/read roundtrip; EOF
    after the writer closes), both exercised within the single
    selftest process itself (`nos_pipe()` hands both ends to the same
    process, so no `fork()`/`SYS_EXEC_PIPE` is needed for these) —
    `run selftest` now reports 13/13 instead of 11/11. A real
    two-process pipeline is covered by the manual test above instead.

### Fixed
- **`kernel/keyboard.c` had no Shift key handling at all**, discovered
  because it blocked typing Phase 16's own `cmd1 | cmd2` in the shell
  (`Shift+\` never produced `|`) — also affected `Shift+5` never
  producing `%`. Not a wrong entry in an existing shifted table: there
  was no shifted table and no Shift press/release tracking at all
  (only `ctrl_pressed` existed), so every character always came from
  the single unshifted `scancode_map` regardless of Shift. Fixed by
  tracking Shift (scancodes `0x2A`/`0x36` press, `0xAA`/`0xB6`
  release) and adding a second, index-matched `scancode_map_shift`
  table (standard US QWERTY). The raw-scancode path used by
  `user/edit.c`'s own separate `sc_map` table has the identical gap
  and was deliberately left unfixed here (out of scope — this fix
  targeted the shell's `SYS_READ`/ASCII path specifically); see
  `PROGRESS.md`'s "Known technical debt".

## [0.15.1] - libnos: shared user-space syscall wrapper library

Not a new phase — same PATCH convention as 0.14.1/0.14.2 (see
CLAUDE.md, "Convenções de fim de fase (versionamento)"): infrastructure
work done after Phase 15 without starting Phase 16, and doesn't change
the completed-phases count (still 15) or any user-visible kernel
behavior — `run selftest` (11/11), `run forktest`, and the manual
`touch`/`edit`/`mkdir`/`cd` flow all behave identically to before.

### Added
- **`user/lib/nullos.c/h`: a shared syscall wrapper library ("libnos",
  `nos_*`)** — one thin `int $0x80` wrapper per syscall in
  `kernel/syscall.h`, replacing six independent, hand-written copies
  of the same wrappers previously duplicated across `shell.c`,
  `edit.c`, `forktest.c`, `selftest.c`, `init.c`, and `spintest.c`.
  Motivation: before v1.0.0 the syscall interface can still change
  freely, but changing how a syscall behaves *underneath* an
  unchanged interface used to mean editing every program that called
  it; now it means recompiling this one file. `user/Makefile` builds
  `lib/nullos.c` once to `$(BUILD)/lib/nullos.o` and links every
  program against it. Migration was a mechanical 1:1 rename for most
  syscalls, with two deliberate exceptions: `nos_write`/`nos_read`
  gained an explicit `fd` argument (matching the syscalls' real
  signatures — two of the six programs already used this fuller form)
  instead of each program hardcoding `fd=1`/`fd=0` inside its own
  wrapper, and `nos_exec(name, arg)` replaces `shell.c`'s old
  `sys_exec(name)`/`sys_exec_arg(name, arg)` split with the syscall's
  real 2-argument signature — incidentally fixing a latent bug where
  the single-argument form never constrained `ecx`, leaving the
  kernel's `sys_exec` to read register garbage as the argument pointer
  (harmless in practice, but not intentional). See `docs/kernel.md` →
  "User-space syscall library (libnos)" and `PROGRESS.md` for the full
  design rationale.

## [0.15.0] - Phase 15: FAT16 subdirectories

FAT16 subdirectories (`mkdir`/`cd`, path-aware `touch`/`edit`/`ls`),
plus a real bug found and fixed during this phase's own manual QEMU
testing (`exec()` not inheriting the caller's cwd). Originally planned
and numbered "Phase 17" in `ROADMAP.md`, but implemented ahead of the
two process-related phases that preceded it in that list (pipes,
copy-on-write fork) — it took the next real completed-phase slot, 15,
and the roadmap was renumbered accordingly: pipes/COW-fork are now
Phases 16/17 (previously 15/16); every phase from the networking one
onward kept its original number. See `ROADMAP.md`'s own note on this.

### Fixed
- **`exec()` (`run`/`edit` in the shell) now inherits the calling
  process's `cwd_cluster` instead of always starting at the root**
  (`kernel/exec.c/h`, `kernel/scheduler.c/h`, `kernel/process.c/h`,
  `kernel/syscall.c`, `kernel/main.c`) — found during Phase 15's own
  manual QEMU test: `mkdir doc; cd doc; touch notes.txt; edit
  notes.txt` silently created and wrote a **second, independent**
  `notes.txt` in the root instead of editing the one inside `doc`,
  because `edit`'s process was spawned fresh via `process_spawn_user()`
  (hardcoded to `cwd_cluster = 0`), not forked from the shell — only
  `process_fork()` had cwd inheritance wired in. `exec()`/
  `scheduler_spawn_user()`/`process_spawn_user()` all now take an
  explicit `cwd_cluster` parameter; `SYS_EXEC` passes the caller's own
  `cwd_cluster`, and the one call site with no calling process
  (`kmain` spawning the initial shell at boot) passes `0` explicitly.
  See `docs/scheduler.md` → "Current working directory" for the design
  rationale (deliberately more `posix_spawn()`-like than POSIX `exec()`
  here, matching the intuitive "run a program from where I am").
  **Anyone who ran the pre-fix Phase 15 test script needs a clean disk**
  (`rm -f build/disk.img && make disk`, or just `make clean && make`)
  before retesting — the leftover `disk.img` has stray root-level files
  from this bug (a duplicate `NOTES.TXT`, and `fk*.txt` markers from
  `forktest` that landed in the root instead of the subdirectory it was
  run from) that would otherwise be mistaken for new bugs.
- **A stale `g_irq_fired` flag from ATA's IRQ-driven wait
  (`kernel/drivers/ata.c`, Phase 12) could make a second
  `ata_read_sector()`/`ata_write_sector()` call skip waiting for its
  own command's real completion**, intermittently (timing-dependent —
  found via 6 rounds of manual testing, some passing, some not). Root
  cause: `ata_init()` unmasks IRQ14/15 before `fat16_init()`'s ~65
  boot-time polling reads (BPB + FAT cache) run, while
  `process_current()` is still `NULL` — every one of those still
  raises a real completion IRQ from the drive, but the polling path
  never calls `ata_wait_irq()` (the only place that used to reset
  `g_irq_fired`), leaving the flag dirty. The first later IRQ-driven
  wait could then see this leftover "already fired" and skip its own
  wait, checking `DRQ` before the drive was actually ready — and could
  cascade, since a skipped wait never registers a waiter for its own
  real (later) completion IRQ either. Fixed by resetting `g_irq_fired`
  right after issuing each new command, not just when a wait finishes,
  so no leftover signal from any prior command (polled or IRQ-driven)
  can be mistaken for the one just issued.
- **`to_8_3()` (`kernel/fs/fat16.c`) silently dropped the extension for
  any base name longer than 8 characters** instead of finding the real
  `.` first — e.g. `to_8_3("selftest_tmp.txt")` produced `"SELFTEST"`
  with no extension at all, not `"SELFTEST.TXT"`. This made unrelated
  names that only differed after their 8th character collide on the
  same on-disk 8.3 entry (`"selftest_dir"` and `"selftest_tmp.txt"`
  both truncated to `"SELFTEST"`), causing spurious create/mkdir
  conflicts. Fixed by locating the actual `.` before splitting into
  base/extension, instead of stopping wherever the 8-char cap for the
  base happened to land.
- **`user/selftest.c`'s own test filenames were themselves 8.3-
  colliding**, revealed (not caused) by the `to_8_3()` fix above:
  `selftest_tmp.txt` (root), `selftest_sub.txt`, and
  `selftest_fork_marker.txt` (both in `selftest_dir`) all share the
  first-8-chars prefix `"selftest"` and the `"txt"` extension, so they
  all pack to the identical `SELFTESTTXT` — making test 11
  ("subdirectory files don't leak into the root") falsely fail against
  test 3's unrelated root-level file, and making test 10 ("fork
  inherits cwd") silently reuse test 9's own dirent instead of
  creating a genuinely separate one. Renamed to `st_root.txt`/
  `st_sub.txt`/`st_mark.txt` — verified pairwise-distinct 8.3
  encodings, not just visually different names. See `docs/testing.md`.

### Changed
- `kernel/fs/fat16.c`: internally reorganized around one shared
  directory-scan/lookup/insert core (`dir_iter_t`, `dir_lookup()`,
  `dir_insert()`, `resolve_path()`) instead of each function walking a
  directory's dirents independently — resolves the "duplicated dirent
  lookup" tech debt between `fat16_find` and `fat16_write_file` tracked
  in `PROGRESS.md` since Phase 10, ahead of extending both to
  subdirectories. `fat16_find`/`fat16_create`/`fat16_write_file`/
  `fat16_readdir` signatures changed accordingly (all now take a
  directory cluster and/or a full path instead of assuming the root).
- `kernel/fs/vfs.c`: `vfs_open`/`vfs_create` take a `cwd_cluster`
  parameter; `vfs_fd_t` gained `parent_cluster`, captured at open/create
  time so `vfs_write` doesn't depend on the caller's cwd at write time.
- `docs/setup.md`: rewritten to drop Phase 0 framing (filename/section
  history that mixed "how to set up today" with "how this file used to
  document Phase 0 only") and reorganized into a direct dependencies →
  cross-compiler → Windows/macOS → Arch → build/run → boot output →
  serial-debug order. Made explicit that automatic dependency
  installation (`tools/setup_env.sh`) only covers Fedora and Debian.

### Added
- **FAT16 subdirectories** (Phase 15): `mkdir`/`cd` in
  the shell, and `touch`/`edit`/`ls` now accept a path with a subfolder
  (e.g. `edit docs/notes.txt`). New syscalls `SYS_CHDIR (26)` and
  `SYS_MKDIR (27)`; `SYS_READDIR (21)` gained an optional path argument.
  New `process_t.cwd_cluster` field, copied across `fork()`. See
  `docs/filesystem.md` → "Subdirectories" and `docs/scheduler.md` →
  "Current working directory" for the full design, and `docs/syscalls.md`
  for the syscall table.
- `user/forktest.c`: parent and child now each create a relative-path
  marker file (`fk<pid>.txt`) right after `fork()`, so `cwd_cluster`
  inheritance can actually be observed from the shell (`ls` the
  directory `forktest` was run from) — the pre-existing test only
  checked the parent/child PID split, never touched the filesystem.
- `user/selftest.c`: four new Phase 15 checks (`run selftest` now
  reports 11/11 instead of 7/7) — `mkdir`, a file write/read roundtrip
  done entirely inside the new subdirectory (the same scenario that
  exposed the `exec()`-cwd bug above, checked here at the FAT16/VFS
  level directly since `selftest` never `exec()`s), `fork()` correctly
  inheriting `cwd_cluster` (child creates a marker via a relative path,
  parent finds it in the same directory), and confirming neither file
  created inside the subdirectory can be opened by name after `cd`ing
  back to the root — the regression test for the exact leak the manual
  test caught. See `docs/testing.md` for the full breakdown.
- `docs/setup.md`: "Windows and macOS" section recommending
  `tools/docker_build.sh` under Docker Desktop (WSL2 backend on
  Windows, native Docker Desktop on macOS) as the only viable path,
  since neither OS has a working native `grub-mkrescue`. Explicitly
  flagged as untested by anyone on the project on either platform.
- `docs/setup.md`: "Arch Linux" subsection under Dependencies with the
  confirmed `pacman` package names for every dependency
  `tools/setup_env.sh` installs on Fedora/Debian (notably
  `libisoburn` for `xorriso` and `qemu-system-x86`), plus a note that
  Arch isn't auto-detected by `tools/setup_env.sh` yet (manual install
  only) and a pointer to that as a possible future improvement.

## [0.14.2] - Docs audit fixes, LICENSE, push-rule convention, `make debug`

Not a new phase — same PATCH convention as 0.14.1 (see CLAUDE.md,
"Convenções de fim de fase (versionamento)"): doc corrections, a new
Makefile debug target, and repo-config housekeeping, none of which
change the completed-phases count (still 14) or user-facing kernel
behavior beyond the new opt-in `make debug`.

### Added
- `tools/Makefile`: new `debug` target — reuses `run`'s `QEMU_FLAGS`
  variable (extracted from the inline flag list `run` used to have)
  plus `-cpu qemu32 -s -S`, so QEMU starts paused with a GDB stub on
  `:1234`. Debug-only (not added to `run`): it also uses
  `qemu-system-i386` instead of `qemu-system-x86_64` — the root fix
  for a "g packet reply is too long" GDB connection failure. That
  turned out to be a property of the `x86_64` binary itself (its
  gdbstub always reports the 64-bit register set over the wire
  regardless of `-cpu`), not the emulated CPU, so an earlier attempt
  at fixing it with `-cpu qemu32` alone on the `x86_64` binary didn't
  work; `qemu-system-i386` (same `qemu-system-x86` package on Fedora)
  reports plain `i386` as expected. `docs/setup.md` documents the
  two-terminal flow (`make debug`, then `gdb build/nullos.elf` +
  `target remote :1234`, no manual `set architecture` needed),
  replacing the note that said no such target existed.
- `LICENSE`: the file never existed despite `README.md` promising MIT
  since the start of the repo. Added the standard MIT text, copyright
  `2026 TheShannonDev`.
- `docs/kernel.md`: `kernel/serial.c/h` — an entire driver, initialized
  first in `kmain` and mirrored by every `vga_putchar()` call — wasn't
  mentioned anywhere in `docs/`. Added a "Kernel base" bullet and a
  file-list entry.

### Changed
- `.gitignore`: added common editor/IDE junk patterns (`.vscode/`,
  `.idea/`, `*.swp`/`*.swo`, `*~`, `.*.un~`, `.DS_Store`, `Thumbs.db`)
  as a preventive measure — none currently present in the repo, but
  standard practice to cover. No dead/unused rules found to remove.
- `README.md`: "## License" now links to `[LICENSE](LICENSE)` instead
  of just stating "MIT" with no reference to an actual file.
- `CLAUDE.md`: added an explicit "Regra de push" — `git push` is only
  recommended/done when at least one code file (`.c`/`.h`/`.asm`) was
  touched in the task; pure documentation changes (`README.md`,
  `ROADMAP.md`, `CHANGELOG.md`, `docs/*.md`, `PROGRESS.md`) stay
  committed locally without a push, unless the doc change is
  retroactively correcting an omission from an already-pushed code
  version (in which case push is allowed). When unclear, ask before
  suggesting push.

### Fixed
- `docs/setup.md`: cross-compiler instructions said `x86_64-elf-gcc`
  throughout (crosstool-ng target `x86_64-unknown-elf`, PATH
  `x-tools/x86_64-unknown-elf/bin`) — wrong architecture; every build
  actually targets 32-bit i686 (`-m32` / `-m elf_i386` in
  `tools/Makefile`/`user/Makefile`). Corrected to `i686-elf-gcc` /
  `i686-unknown-elf` throughout, and reframed i686 as the primary
  target instead of a "simpler alternative" footnote.
- `docs/setup.md`: Docker instructions referenced `ghcr.io/osdev/osdev-env`,
  an image the project doesn't actually use — the real working script
  `tools/docker_build.sh` pulls `randomdude/gcc-cross-i686-elf`
  (overridable via `NULLOS_DOCKER_IMAGE`). Corrected, and `tools/setup_env.sh`
  (the actual per-distro dependency-install script) is now mentioned.
- `docs/setup.md`: the example QEMU boot banner hardcoded a stale
  `v0.0.1 - Phase 0`. Replaced with a generic `vX.Y.Z - Phase N: <desc>`
  placeholder plus the real current boot-log tag sequence, pointing to
  `kernel/version.h`/`docs/kernel.md` instead of a number that would
  just go stale again next phase.
- `docs/setup.md`: claimed a `make debug` target exists — it doesn't,
  per `tools/Makefile`. Removed the claim; noted the manual GDB
  alternative instead.
- `docs/setup.md`: `tools/run_qemu.sh` was implied to attach
  `disk.img` like `make run` does — checked its source, it doesn't
  (no `-drive` flag at all). Now flagged explicitly so it isn't
  confused with `make run`.
- `docs/setup.md`: "Debug via serial" described serial output as a
  future Phase 1 addition — false; `kernel/serial.c` already exists
  and every `vga_putchar()` call already mirrors to it. Fixed.
- `docs/setup.md`: dependency list was missing `dosfstools`/`mtools`
  (needed by `tools/make_disk.sh`) and `python3` (needed by
  `tools/make_ramfs.py`); neither is installed by `setup_env.sh`
  either. Both now called out.
- `docs/memory.md`: PMM capacity said "64 MB" — wrong. `PMM_MAX_PAGES`
  (8192) × `PAGE_SIZE` (4096) = 32 MB, independent of how much RAM
  QEMU is actually given. Corrected with the exact constants cited.
- `docs/filesystem.md`: attributed disk-attaching behavior to both
  `tools/run_qemu`/`make run` — only `make run` actually attaches
  `disk.img`; `run_qemu.sh` is a separate, older script that doesn't.
  Corrected.
- `docs/shell.md`: commands list omitted `lspci`, a real shell command
  present in `user/shell.c`'s help text and dispatch table. Added.
- `docs/kernel.md`: "Using the ramfs" described a stale/nonexistent
  workflow (a "to be implemented" `tools/mkramfs` tool, manually
  editing `grub.cfg.in` per program) — `tools/make_ramfs.py` has long
  since been implemented and is invoked automatically by
  `tools/Makefile`, and `grub.cfg.in` already has a permanent ramfs
  module line. Rewritten to describe the real current workflow (the
  same steps used to add `user/selftest.c`).

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
