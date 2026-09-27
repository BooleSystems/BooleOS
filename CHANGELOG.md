# Changelog

All notable changes to BooleOS are documented in this file, grouped by the
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
- `.claude/skills/de-ai-writing/`: project skill for rewriting text without the signs of AI writing catalogued in Wikipedia's "Signs of AI writing" guide (`SKILL.md` with the workflow, `references/signs.md` with the full catalogue and a fix for each sign). `CLAUDE.md` gains a section requiring public text (README, CHANGELOG, CONTRIBUTING, code comments, social posts, issue templates) to go through it; facts, numbers and claims stay unchanged. The optional `scripts/check_ai_signs.py` scanner was not added; the manual pass over `references/signs.md` is the fallback. Not yet run over the existing docs.
- GitHub issue templates in `.github/ISSUE_TEMPLATE/` (bug report, feature request, boot/compatibility report).
- `CONTRIBUTING.md`: contribution guide with the pull request policy. PRs must be AI-generated, and the commit must prove it with a `Co-Authored-By` trailer naming the AI tool (Claude Code adds it automatically); PRs without the trailer are closed, and code that reads as hand-written despite it is sent back to be redone. It also lists what to read before opening a PR (`CLAUDE.md`, `ROADMAP.md`), the local build-and-boot expectation, and what kinds of contributions are welcome.

### Changed
- ROADMAP.md: Phase 26 gains sub-phase 26-E (USB HID keyboard driver) and the canonical input event, which was 26-E, becomes 26-F and is named **Deflection**; Phase 27 is split into 27-A **Cathode** (graphics API / framebuffer driver) and 27-B **Raster** (launcher/grid, the GUI delivered in the phase, on top of Cathode).
- `tools/prev/` now holds the v0.20.1 snapshot (kernel + ramfs built from the `v0.20.1` tag), so the "previous release" GRUB entry of the next version is a real release.

### Fixed
- `CONTRIBUTING.md`: removed a reference to a component (`cathode_host`, SDL2) that does not exist in the BooleOS kernel.
- Repository links in `README.md`, `docs/quickstart.md` and `docs/setup.md` (releases page, source link, `git clone` URL) now point to `github.com/BooleSystems/BooleOS`; they still had the pre-relocation path.

## [0.20.1] - 2026-09-25 - Patch: the `debug` boot argument works; BooleOS rename and repository move

### Added
- ROADMAP.md: new sub-phase 26-E (canonical `input_event_t` unifying PS/2 and USB HID input), closing Phase 26.
- Release tags for every version in this changelog: `v0.2.0` through `v0.16.0` were created retroactively (annotated, at the commit that closed each version, with the original commit date), joining the existing `v0.0.1`, `v0.1.0` and `v0.17.0`–`v0.20.0`. All 26 tags `v0.0.1`–`v0.20.0` now exist.
- Retroactive release packages for `v0.0.1`–`v0.20.0`, each built from a clean checkout of its tag: the bootable ISO (named as the build produced it, `nullos.iso`), a blank FAT16 `disk.img` for the versions that have one (0.10.0 and later), a per-version `README.txt` that describes what that specific ISO does and how to run it in QEMU, the original docs of that version untouched, and an English translation of the docs of the Portuguese-language versions (0.0.1–0.10.0).

### Changed
- **Project renamed from NullOS to BooleOS.** The old name was too generic and already saturated on GitHub (several unrelated projects called "NullOS"). The new name references George Boole and Boolean algebra, the mathematical basis of every digital circuit.
  - Prose and UI text (boot banner, ASCII logo, `fetch`, GRUB entries, README, ROADMAP, `docs/*.md`, `CLAUDE.md`) now say "BooleOS".
  - Macros/prefixes `NULLOS_*` -> `BOOLEOS_*` (`kernel/version.h`, build env vars, `@BOOLEOS_VERSION@` in `tools/grub.cfg.in`).
  - Build artifacts: `nullos.elf` -> `booleos.elf`, `nullos.iso` -> `booleos.iso`, `/boot/prev-booleos.elf`, release zip `booleos-X.Y.Z.zip`, FAT volume label `BOOLEOS`; `user/lib/nullos.{c,h}` -> `user/lib/booleos.{c,h}`; `tools/prev/nullos.elf` renamed only (the v0.20.0 snapshot content is untouched).
  - Deliberately unchanged: the `nos_*`/`libnos`/`nosstdio` API prefix, the repository links inside the docs (they still have to be pointed at the new repository location), C `NULL` and "null-terminated" terminology, and the historical entries of this changelog.
- Repository relocated to the BooleSystems organization (`github.com/BooleSystems/BooleOS`); project history and tags were carried over.
- Copyright and license attribution updated to reflect the current maintainer.
- Project renamed from NullOS to BooleOS across code, docs, build artifacts and macros (see the rename entry above).
- Git history rewritten for the move: author, committer and tagger identity, the copyright holder in `LICENSE` and the repository links in the historical docs were normalized. Every commit hash changed, so hashes quoted in older notes or issues no longer resolve; the commit messages, dates and tag names are unchanged in substance.
- `kernel/version.h` bumped to `0.20.1` (PATCH: no new phase, only the `debug` boot argument fix below); the README banner follows.

### Fixed
- **The `debug` boot argument of the "serial debug mode" GRUB entry did nothing.** GRUB passed it and the kernel could read the command line, but no code ever looked for it, so that entry booted exactly like the default one. `hal_boot_init()` now sets a global flag, `g_debug_boot`, when the word `debug` is on the command line. While it is set, `kmain` writes extra `[DEBUG]` detail to the serial port only (nothing changes on screen): the command line and the bootloader's memory map region by region, the boot configuration values that decide Safe Mode (`fail_count`, threshold, `safemode` flag, pending crash), and one line per boot step (ATA, boot config, PMM, paging, heap, scheduler, FAT16, PCI, ramfs/shell) with the PIT tick count. No new subsystem, no allocation. Without the argument the boot output is unchanged. Docs: `docs/setup.md` ("Debug via serial"), `docs/quickstart.md`, `docs/safemode.md`.

### Removed
- `kernel/main.c.save`: an editor backup file that had been committed by mistake.
- A stray `Untagged` tag (left over from a draft release) that did not correspond to any version.

## [0.20.0] - 2026-09-20 - Phase 20: Crash handler leads into Safe Mode

### Added

- **A crash restarts into Safe Mode.** An unhandled CPU exception used to end in
  a red screen and a `hlt` loop; now `exception_handler()` saves a crash record in
  the boot configuration sector (LBA 1, the same one as `boot_fail_count`), shows
  the red screen for ~3 s, and resets the machine (the 8042 pulse, then a triple
  fault if it does not answer within ~0.5 s counted on the raw PIT). The next
  boot goes straight to Safe Mode with a crash banner (`system crashed: #PF Page
  Fault at EIP 0x...`) and a new menu item "6. View last crash details" (exception,
  EIP, error code, CR2 and decoded flags for a page fault, uptime). The record
  (keys `crash_pending`, `crash_type`, `crash_eip`, `crash_err`, `crash_cr2`,
  `crash_ticks`) is acknowledged by "Reboot normally" and removed after the next
  complete normal boot, so entering and leaving Safe Mode never loses it. A crash
  that could not be saved (no disk yet, no config sector) halts with the screen
  readable, as before, instead of restarting. A second exception during the
  handling skips the dump and goes straight to the reset.
- The save path depends on nothing that may be broken: polling-only ATA I/O
  (`ata_crash_read_sector`/`ata_crash_write_sector`, `block_*_polled()` in the HAL —
  no lock, no IRQ, no scheduler, bounded waits, soft reset of a channel left
  mid-command), a static buffer, no heap. New `bootcfg_buf_*` functions (the text
  store on an explicit buffer, hex numbers, `bootcfg_remove()`), `timer_poll_delay_ms()`,
  `power_reboot_request()`, `exception_name()`.
- A temporary serial-only trace of every step of the ATA `probe()`
  (`[ATADBG]`, marked `TEMP-DEBUG(ata-probe)` in `kernel/drivers/ata.c`) stays in
  the kernel to catch the rare intermittent "no disk" at boot (see `PROGRESS.md`);
  it writes to the serial port only.
- **`crash <de|pf|gpf>` shell command**, a debug tool that faults on purpose
  (#DE, a read of `0xDEADBEEF`, #GP) so the whole pipeline can be tested repeatably
  with `make run-reboot-test`. Documented in `docs/safemode.md`.

### Changed

- **Roadmap renumbered:** the crash-handler phase took number 20, so every
  planned phase after it moved up by one (copy-on-write `fork()` is now 21,
  `unlink()` 22, memory release 23, `e1000` 24, AHCI 25, xHCI 26, framebuffer/GUI
  27, syscall deprecation 28, audit pass 2 29, polish 30, the DOOM port 31).
  References in `ROADMAP.md`, `PROGRESS.md`, the docs and code comments were
  updated; older CHANGELOG entries keep the numbers of their time.
- `power_reboot()` was split: `power_reboot_request()` only pulses the reset line.
- Documentation: `docs/safemode.md` (the crash handler, the record format, the
  flow, how to test), `docs/kernel.md`, `docs/hal.md` (the polled block I/O),
  `docs/shell.md` (`crash`), `docs/testing.md` (the manual crash test),
  `docs/setup.md` (`make run-reboot-test` for the crash test) and the README.

## [0.19.0] - 2026-09-20 - Phase 19: SDK / app-development experience

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
- selftest: three new tests (exec of a program that exists only on FAT16;
  malformed, truncated and missing programs rejected; the printf family) —
  21 tests.

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
- `tools/prev/` holds the v0.18.0 build (the "previous release" GRUB entry of
  this version).
- Documentation brought up to date for this phase: `docs/testing.md` (the 21-test
  selftest and `make test-elf`), `docs/filesystem.md` (`exec()` as a `vfs_open()`
  consumer and the shared-buffer hazard), `docs/syscalls.md` (`SYS_EXEC` /
  `SYS_EXEC_PIPE` name resolution), `docs/security.md` (the ELF loader does not
  trust the file), `docs/shell.md` (`run` a FAT16 program), `docs/setup.md`
  (`make test-elf`, the order-only rule) and the README (file list, docs list,
  `make test-elf`).
- `CLAUDE.md`: a "Definition of Done" checklist (literal, not prose) at the top —
  what must be done in every subtask commit (a `docs/TODO.md` trace, a
  `[Unreleased]` CHANGELOG entry) and in the closing commit of a phase (the
  consolidated CHANGELOG entry as a blocking item, README, ROADMAP,
  `version.h`, TODO resolved into real docs, PROGRESS, syscall table, selftest),
  ending with "read this checklist item by item against the real state of the
  files"; plus the release conventions: every version merged into `main` gets an
  annotated `vX.Y.Z` tag, `make snapshot` is run on the tagged tree and
  `tools/prev/` committed back to `nightly`, and a GitHub Release with a
  ready-to-run zip (bootable ISO, a blank disk image, a README with the QEMU
  command) is published — a version is not released with only the tag.
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

### Added
- `LICENSE`: MIT license file (previously the README just said "MIT" with no actual license file in the repo).
- `tools/Makefile`: new `make debug` target using `qemu-system-i386` (not `qemu-system-x86_64`) with `-s -S` for GDB — the x86_64 QEMU binary's gdbstub always reports the 64-bit register set over the wire regardless of `-cpu`, which GDB rejects with "g packet reply is too long"; the i386 binary reports plain i386 as expected. `make run` is unaffected and keeps using `qemu-system-x86_64`.
- `.gitignore`: editor/IDE artifacts (`.vscode/`, `.idea/`, `*.swp`/`*.swo`, `*~`, `.DS_Store`, `Thumbs.db`).
- `CLAUDE.md`: new "Regra de push" section (push to the working branch requires at least one `.c`/`.h`/`.asm` file changed in the task; documentation-only changes don't trigger a push unless they retroactively correct an already-pushed version) and a refined "`[Unreleased]`" CHANGELOG flow — intermediate work accumulates under `[Unreleased]` and `kernel/version.h` is only bumped when the user explicitly asks to close a version, replacing the previous rule of bumping PATCH immediately on every small task.

### Fixed
- `docs/setup.md`: corrected several stale/inaccurate claims found by auditing against the actual scripts — the cross-compiler must be `i686-elf-gcc` (32-bit), not `x86_64-elf-gcc` as the old text suggested; the Docker section now describes the project's actual `tools/docker_build.sh` instead of a generic `ghcr.io/osdev/osdev-env` image; the crosstool-ng instructions target `i686-unknown-elf` instead of `x86_64-unknown-elf`.
- `docs/kernel.md`: the "Using the ramfs" section previously described `tools/mkramfs` as "to be implemented" and gave manual build steps; corrected to reflect that `tools/make_ramfs.py` is fully implemented and already wired into `make`/`make all`, and replaced with the real steps for adding a new user program to the ramfs.
- `README.md`: license line now links to the new `LICENSE` file instead of just naming "MIT".

## [0.14.1] - Documentation reorganization + selftest tool

### Added
- `user/selftest.c`: an automated regression test suite (`run selftest` from the shell) covering file create/write/read via `SYS_CREATE`/`SYS_WRITE_FILE`/`SYS_READ`, `fork()`+`SYS_WAIT`, and `SYS_PCI_LIST`'s device count — a PASS/FAIL readout instead of manually exercising `touch`/`edit`/`ls`/`fork` by hand after every kernel change.
- `docs/kernel.md`, `docs/memory.md`, `docs/scheduler.md`, `docs/syscalls.md`, `docs/filesystem.md`, `docs/security.md`, `docs/pci.md`, `docs/shell.md`, `docs/testing.md`: per-system technical documentation split out of `README.md`, one file per subsystem.
- `kernel/drivers/pci.c`/`pci.h`: `pci_device_count()` — returns the count from the last `pci_scan_bus()` without rescanning, so a caller can check "found anything?" without parsing VGA text output.

### Changed
- `README.md` restructured into a lean index: short overview, version banner, a table of only *completed* phases, links to `ROADMAP.md` (new — future-phase planning moved there) and to each `docs/<subject>.md`, build instructions, license. The detailed Phase 15-21 planning section previously inline in the README moved to `ROADMAP.md` unchanged in content.
- `kernel/syscall.c`: `SYS_PCI_LIST` now returns `pci_device_count()` instead of always `0`.
- `CLAUDE.md`: documents the new README/ROADMAP/docs split and the `MAJOR.MINOR.PATCH` versioning scheme (MINOR reserved exclusively for a completed phase number; PATCH for intermediate work — doc reorganization, test tooling, small additive functions — that doesn't touch MINOR/`NULLOS_PHASE`).
- This release is itself a PATCH under that new scheme (`0.14.0` → `0.14.1`): no new phase, just the documentation split and `selftest.c`/`pci_device_count()`.

## [0.14.0] - Phase 14: kernel memory-safety hardening

### Added
- `PROGRESS.md`: new working-memory file for future sessions — current phase status, non-obvious architecture decisions, and known technical debt, explicitly not duplicating README content.
- `kernel/version.h`: single source of truth for the version string (`NULLOS_VERSION`/`NULLOS_PHASE`/`NULLOS_PHASE_DESC`, composed `NULLOS_BANNER`/`NULLOS_SHORT_BANNER`); `kernel/main.c`'s boot banner, `user/shell.c`'s `fetch`/`uname` (included directly since it's plain-text macros with no kernel types), and `tools/grub.cfg` (generated at build time from a new `tools/grub.cfg.in`) all read from it instead of each hardcoding the version string.
- `kernel/syscall.c`: `user_ptr_valid()`/`copy_from_user()`/`copy_to_user()` — confirm the whole `[addr, addr+len)` range is mapped **and** carries `VMM_USER` (via the new, stricter `vmm_get_user_phys_from_dir()`) before touching a single byte. `sys_write`, `sys_read`, `sys_write_file` and `sys_meminfo` are rewired to go through these, closing 4 confirmed ring 3 → ring 0 arbitrary memory read/write bugs where a process could point a syscall at the kernel's own identity-mapped memory (heap, page tables) to read or corrupt it.
- `kernel/memory/vmm.c`/`vmm.h`: `vmm_get_user_phys_from_dir()` — like the existing `vmm_get_phys_from_dir()` but also requires `VMM_USER` on both the PDE and PTE, not just "present" (every process's page directory clones the kernel's PDE0/PDE1, so that region is always "present" everywhere but never `VMM_USER`).
- `kernel/memory/heap.c`: `kmalloc()` now rejects any `size > HEAP_MAX - HEAP_START` up front, before the "align to 4 bytes" arithmetic (`size + 3`) that could otherwise silently overflow for a size close to `UINT32_MAX` and hand back a much smaller block than requested.

### Fixed
- `kernel/syscall.c`: `user_kptr()` (the byte-resolution helper behind `sys_open`/`sys_create`/`sys_exec`/`sys_getarg`'s filename/argument copying) now resolves through `vmm_get_user_phys_from_dir()` instead of the looser `vmm_get_phys_from_dir()`, closing the same kernel-memory-disclosure gap in those four syscalls with no change needed at any of their call sites.
- `sys_open`: removed leftover debug output.

## [0.13.0] - Phase 13: fork()

### Added
- `process_fork()` (`kernel/process.c`): full (not copy-on-write) address-space duplication — a fresh page directory, a fresh physical page for every page actually mapped in the parent (code/data/stack), and a duplicated fd table entry for every open file. Atomically claims a free process-table slot under `cli`/`sti` (pid assigned and state set to `PROCESS_BLOCKED` in one uninterruptible step) so two concurrent `fork()`s can't collide on the same slot.
- Child resumption via a fabricated kernel stack: the 13-word frame captured from the parent's syscall entry (8 `pusha` registers + the CPU's ring3→ring0 trap frame) is copied onto the child's stack with `eax` forced to 0, with the return address pointed at a new `isr128_resume` label in `isr.asm` (right before the existing `popa`/`iret`) instead of the normal bootstrap function. The first scheduler switch into the child lands on `isr128_resume`, resuming in ring 3 at the instruction after the parent's `fork()` call with `eax=0`.
- `g_syscall_frame` (`kernel/syscall.c`): set by `isr128` right after `pusha`; `sys_fork()` copies it into a local buffer as its very first action (before anything preemptible) so a different process's syscall entry can't overwrite it mid-copy.
- `SYS_FORK` (25): `fork() → child pid (parent) / 0 (child) / -1`. After `process_fork()` succeeds, duplicates `fd_table[parent_slot]` into `fd_table[child_slot]` (a shallow copy — each fd then tracks its own read/write position independently, not POSIX's shared-offset semantics).
- `user/forktest.c`: calls `fork()` and prints whether it's the parent or the child.
- `CLAUDE.md`: new "Documentação" section requiring the README to be updated as part of the same task whenever a phase/feature completes and testing is confirmed.

Known, explicitly out-of-scope limitations documented in code comments: `process_spawn()`/`process_spawn_user()` still claim a free slot without `cli`/`sti` protection; `process_exit()` never frees a process's `cr3` or its mapped pages (forked children included); the page-copy assumes physical pages fall in the identity-mapped first 8MB, the same assumption `elf_load()` already makes.

## [0.12.0] - Phase 12: IRQ-driven ATA

### Changed
- `kernel/drivers/ata.c`: `ata_read_sector`/`ata_write_sector` now wait for command completion via IRQ14/15 instead of busy-wait polling, whenever called from a scheduled process; the original polling path is kept as a fallback when there's no current process (early boot, e.g. `fat16_init()` reading the BPB before the scheduler runs anything). `probe()` and the reset/detect path are unchanged — they only ever run once at boot.
- `kernel/isr.asm`/`idt.c`: new `irq14`/`irq15` stubs (vectors 46/47) following the existing IRQ0/IRQ1 pattern; `ata_init()` registers the handler and unmasks the detected channel's line, plus the master PIC's IRQ2 cascade line (required for any slave-PIC IRQ to reach the CPU).
- `ata_irq_handler()`: intentionally minimal — acknowledges the drive's IRQ (reading Status clears the line) and flips a waiting process back to `PROCESS_READY`; never touches the scheduler or performs a context switch itself.
- `ata_wait_irq()`: checks an "IRQ already fired?" flag and, if not set, marks the process `PROCESS_BLOCKED` and calls the new `scheduler_block_current()` — both the check and the state transition happen inside the same `cli`/`sti` section so an IRQ firing before the block completes is never lost.
- New exclusion gate (`ata_gate_acquire`/`ata_gate_release`) serializing ATA controller access across processes, since a process no longer holds the CPU for a whole disk operation; a second process waiting on the gate also blocks for real (no busy-wait) and rechecks the gate state after being woken.

### Added
- `PROCESS_BLOCKED` (`kernel/process.h`): a new process state distinct from `PROCESS_SLEEPING`, so the timer's tick-based wake-up (`process_wake_sleepers`) never touches a process waiting on an external event like a disk IRQ.
- `scheduler_block_current()` (`kernel/scheduler.c`): a sibling of `scheduler_yield()` that switches away from the CPU without forcing the process's state back to `READY` — the caller (or whoever owns the wait) is responsible for that transition. Both the IRQ handler and the gate re-check that a waiter is still `PROCESS_BLOCKED` before waking it, so a process killed while waiting can't resurrect a possibly-reused process-table slot.

## [0.11.0] - Phase 11: PCI bus enumeration

### Added
- `kernel/drivers/pci.c`/`pci.h`: PCI configuration space access via the legacy Configuration Mechanism #1 (32-bit `outl`/`inl` on `CONFIG_ADDRESS`/`0xCF8` and `CONFIG_DATA`/`0xCFC`); `pci_config_read32/16/8` always read the containing dword and shift/mask for the narrower widths.
- `pci_scan_bus()`: iterates bus 0-255 × device 0-31 × function 0-7 (vendor ID `0xFFFF` = empty slot; only probes functions 1-7 when the function-0 header type declares itself multi-function via bit `0x80`), storing vendor/device ID, class/subclass/prog IF, header type and all 6 raw BARs for up to 64 devices in a static table.
- `pci_print_list()`: dumps that table to VGA as `bus:device.function  vendor=XXXX device=XXXX class=XX/XX progif=XX htype=XX`, plus a `bars:` line for devices with at least one non-zero BAR.
- `SYS_PCI_LIST` (24): reprints the table captured at boot without rescanning; `user/shell.c` gains the `lspci` command.
- `kmain` calls `pci_scan_bus()` + `pci_print_list()` right after FAT16 init (`[PCI]` in the boot log) — independent hardware discovery, not on the disk-mount critical path.

## [0.10.1] - Phase 10: English translation

Pure text/comment translation pass — no functional change. Every kernel, userland and tool source file with Portuguese comments, boot/error/help strings, and identifiers had them translated to English (`boot/boot.asm`, `docs/setup.md`, all of `kernel/drivers/`, `kernel/fs/`, `kernel/memory/`, `kernel/{gdt,idt,isr,keyboard,main,pic,process,ramfs,scheduler,syscall,timer,usermode}.*`, `tools/{Makefile,grub.cfg,make_disk.sh,make_ramfs.py,setup_env.sh}`, `user/{edit,init,shell,spintest}.c`), touching 46 files. `README.md` was rewritten fully in English (previously mixed Portuguese/English).

### Changed
- `CLAUDE.md`: expanded substantially — new "Debug e instrumentação temporária" section (serial-only debug prints, never duplicate a mirrored `serial_putchar` call, remove temporary instrumentation after investigating, always reproduce with a clean disk state) and "Convenções técnicas" section (C99 strict, no libc, no allocation outside `kmalloc`/`kfree`, care with fixed-size FAT16 dirent arrays and undefined behavior, watch for duplicated find/compare logic between `fat16_find` and `fat16_write_file`, distinguish "primary I/O operation failed" from "flush/confirmation failed" in low-level disk functions). Adds a new **"Idioma do código"** rule: starting from this version, all new code (comments, boot messages, error strings, help text, identifiers) must be written in English, and touching an existing file is an opportunity to translate nearby Portuguese text still in scope — chat prompts stay in Portuguese, only code/system output becomes English going forward.

## [0.10.0] - Phase 10: persistent disk (ATA PIO + FAT16)

### Added
- `kernel/drivers/ata.c`/`ata.h`: polling-only ATA PIO driver (no IRQ/DMA). `ata_init()` probes all four possible drives (primary/secondary × master/slave) with `IDENTIFY` (`0xEC`) and rejects ATAPI devices by their `LBA_MID=0x14`/`LBA_HI=0xEB` signature; `ata_read_sector`/`ata_write_sector` do LBA28 `READ SECTORS`(`0x20`)/`WRITE SECTORS`(`0x30`)+`CACHE FLUSH`(`0xE7`), polling the `BSY`/`DRQ` status bits (`wait_not_busy`/`wait_drq`) rather than using a fixed delay.
- `kernel/fs/fat16.c`/`fat16.h`: FAT16 filesystem over the ATA driver. Reads the BPB from sector 0, caches the whole FAT table in the kernel heap; `fat16_find`/`fat16_readdir` scan the root directory (8.3 names via `to_8_3`); `fat16_read_at` follows the cluster chain from an arbitrary byte offset; `fat16_write_file` frees the file's old cluster chain, allocates a fresh one (`fat16_alloc_cluster`), writes the data and flushes the updated FAT back to disk (`fat16_flush_fat`); `fat16_create` finds a free/deleted root directory entry and writes an empty dirent (idempotent — succeeds silently if the file already exists).
- `kernel/fs/vfs.c`/`vfs.h`: single dispatcher used by the file syscalls — `vfs_open` tries the (read-only) ramfs first, then FAT16; `vfs_create` tries `vfs_open` first and only creates a FAT16 entry if nothing was found; `vfs_write` refuses to write to a ramfs-backed fd.
- Eleven new syscalls: `SYS_READ_RAW` (13, blocking raw-scancode read with no echo, `scancode | (ctrl<<8)`), `SYS_GOTOXY` (14), `SYS_CLEAR` (15), `SYS_GETARG` (16, reads the argument string passed by the extended `SYS_EXEC`), `SYS_KBD_FLUSH` (17), `SYS_SETCOLOR` (18), `SYS_SET_RAW_MODE` (19, disables `SYS_READ`'s echo), `SYS_WAIT` (20, blocks until a PID exits), `SYS_READDIR` (21, lists ramfs + FAT16 entries via VGA), `SYS_WRITE_FILE` (22, writes to a FAT16-backed fd), `SYS_CREATE` (23, open-or-create against FAT16).
- `kernel/keyboard.c`/`.h`: a second, parallel raw-scancode ring buffer (`kb_raw_buf`) alongside the existing translated-character one, plus `keyboard_raw_nowait()` and `keyboard_flush()` (empties both buffers) backing `SYS_READ_RAW`/`SYS_KBD_FLUSH`.
- `user/edit.c`: a full-screen text editor — loads a file into an in-memory buffer (`load_file`, creating it via `SYS_CREATE` if it doesn't exist and keeping the fd open), renders with `SYS_GOTOXY`/`SYS_SETCOLOR`/`SYS_CLEAR`, reads raw keystrokes via `SYS_READ_RAW` with `SYS_SET_RAW_MODE` enabled, and saves the whole buffer with `SYS_WRITE_FILE` on Ctrl+S (status line shows "salvo" or "salvo (sem disco)" if there's no backing disk); Ctrl+Q closes the fd and exits.
- `user/shell.c`: new `touch <name>` (creates an empty file via `SYS_CREATE`+`SYS_CLOSE`) and `ls` (lists ramfs and FAT16 entries separately via `SYS_READDIR`) commands; `run` now also passes an argument string through the extended `SYS_EXEC`/`SYS_GETARG`.
- `tools/make_disk.sh`, `tools/Makefile` (`disk` target): generates a 32 MB FAT16 `build/disk.img` via `mkfs.vfat` only if it doesn't already exist, so on-disk data persists across rebuilds; `make run` attaches it as `-drive file=build/disk.img,format=raw,if=ide`.
- `tools/readme.txt`: template file copied onto the generated FAT16 disk image.

### Changed
- `kmain` now calls `ata_init()` and `fat16_init()` right after the scheduler starts; if no disk is present or it isn't valid FAT16, boot continues normally and file operations against the disk simply return -1 instead of crashing.
- `SYS_WRITE` (fd 1/2) still only writes to VGA; file writes go through the new dedicated `SYS_WRITE_FILE`, which routes through `vfs_write`/`fat16_write_file` — the ramfs stays read-only.

## [0.9.0] - Phase 9: `SYS_OPEN`/`SYS_CLOSE`/`SYS_READ` for ramfs files

### Added
- `SYS_OPEN` (11): `open(name) → fd (≥3) or -1`. Copies the name from userland (same `user_kptr` technique as `SYS_EXEC`), looks it up with `ramfs_find`, and allocates the first free slot in a new per-process file descriptor table.
- `SYS_CLOSE` (12): `close(fd) → 0 or -1`, frees the fd slot.
- `kernel/syscall.c`: a global `fd_table[PROCESS_MAX][FD_PER_PROC]` (8 descriptors per process, fds 0-2 reserved for stdin/stdout/stderr, file fds start at 3) tracking `{used, offset, size, pos}` for each open ramfs file, indexed by `proc_slot()` (the process's index in the process table). `sys_exit()` now zeroes every fd slot belonging to the exiting process to prevent slot leaks.
- `kernel/ramfs.h`: exposes `ramfs_base` (previously file-local to `ramfs.c`) so `syscall.c` can read file bytes directly out of the mounted image.

### Changed
- `SYS_READ` is now polymorphic: fd 0 still blocks on the keyboard with echo/backspace as before; fd ≥ 3 reads up to `len` bytes from `ramfs_base + offset + pos` for an fd opened via `SYS_OPEN`, advances the position non-blockingly, and returns 0 at EOF instead of blocking.

## [0.8.0] - Phase 8: `SYS_EXEC`, Ctrl+C, foreground PID

### Added
- `SYS_EXEC` (10): `exec(name) → pid or -1`. `sys_exec()` copies the program name byte-by-byte out of the calling process's virtual address space via `vmm_get_phys_from_dir(cur->cr3, vaddr)` (the new `user_kptr()` helper), then calls the kernel's `exec()` and returns the new process's PID.
- `kernel/keyboard.c`: tracks left-Ctrl press/release (scancodes `0x1D`/`0x9D`); when Ctrl is held and `C` (`0x2E`) is pressed, injects `0x03` into the ring buffer instead of a normal character.
- `kernel/syscall.c`: `sys_read()` returns immediately with `buf[0] = 0x03` and echoes `^C\n` when it sees that byte, instead of treating it as an ordinary keystroke.
- `user/shell.c`: new `run <program>` command (calls `sys_exec`); the shell tracks the spawned PID in `foreground_pid` and, on receiving Ctrl+C from `sys_read`, calls `sys_kill(foreground_pid)` and clears it. `foreground_pid` is reset whenever a different command is typed.

## [0.7.0] - Phase 7: `SYS_READ` + interactive userland shell

### Added
- Five new syscalls: `SYS_READ` (5, `read(fd, buf, len)` — reads from the keyboard ring buffer), `SYS_UPTIME` (6, PIT ticks since boot), `SYS_MEMINFO` (7, `meminfo(*pmm_pages, *heap_bytes, *nprocs)`), `SYS_PS` (8, dumps the process table to VGA), `SYS_KILL` (9, `kill(pid)`).
- `kernel/keyboard.c`/`.h`: `keyboard_getchar_nowait()`, a non-blocking read of the ring buffer (returns -1 if empty), used by `sys_read`'s poll loop.
- `kernel/syscall.c`: `sys_read(fd=0, buf, len)` polls `keyboard_getchar_nowait()` with `scheduler_sleep_current(1)` between attempts (so it doesn't starve other processes), echoes each character to VGA, and handles backspace by decrementing the write index without buffering it.
- `kernel/memory/heap.c`/`.h`: `heap_free_bytes()` — sums free block sizes without the `heap_dump()` VGA output, for `SYS_MEMINFO` to consume.
- `user/shell.c`: the first interactive shell. Reads a line via `SYS_READ`, dispatches to `help`, `uname`, `fetch` (ASCII banner with OS/arch/uptime/free PMM/free heap/running process count), `ps`, `mem`, `echo <text>`, `kill <pid>`, `clear`, `exit`. Its own hand-rolled `sh_strlen`/`sh_strcmp`/`sh_strncmp`/`sh_uitoa` (no libc).
- `tools/Makefile`, `user/Makefile`: build and package `shell.elf`.

### Changed
- `kernel/keyboard.c`: the IRQ1 handler no longer echoes characters to VGA directly (`vga_putchar(c)` removed from `keyboard_callback`) — echo is now the responsibility of `sys_read`, since a blind hardware-level echo would double up with a syscall-level one.
- `kernel/process.c`: `process_exit()` now sets the process back to `PROCESS_UNUSED` (freeing its table slot for reuse) instead of `PROCESS_ZOMBIE`, which the scheduler never reclaimed.
- `kernel/main.c`: the `heartbeat_task`/`heap_watch_task` background kernel debug tasks from Phase 3 are removed, and `kmain` now `exec()`s `shell` instead of `init`/`spintest` — the shell owns VGA/keyboard exclusively at boot.
- `user/init.c`: simplified to `SYS_WRITE` + `SYS_EXIT` (previously looped forever calling `SYS_YIELD`).

## [0.6.0] - Phase 6: syscall return values + IRQ0 preemption

### Added
- `kernel/timer.c`: the PIT IRQ0 handler now enforces a preemption time slice — after `scheduler_tick()`, if the currently running process has held the CPU for `PREEMPT_TICKS` (10 ticks = 100 ms at 100 Hz), it calls `scheduler_yield()` on its behalf, so a process that never calls `yield`/`sleep` no longer monopolizes the CPU.
- `user/spintest.c`: a new userland test program with a tight busy loop that never yields (prints "spintest: still spinning" every 5,000,000 iterations), added specifically to exercise the new preemption path.
- `kmain` now `exec()`s both `init` and `spintest` when a ramfs module is present.
- `tools/Makefile`, `user/Makefile`: build and package `spintest.elf` into the ramfs image alongside `init.elf`.

### Fixed
- `kernel/isr.asm`: `isr128` now writes the syscall's return value into the saved `eax` slot of the `pusha` frame before `popa`, so a ring-3 program actually receives the syscall's result in `eax` after `int 0x80` — this had been implemented once in v0.4.0 and then reverted in v0.5.0.
- `user/init.c`: `sys_write`/`sys_yield` inline `int $0x80` blocks gained `"=a"`/`"0"` output/input constraints so GCC doesn't assume `eax` is unchanged across the syscall (without this, `-O2` could reuse a stale `eax` value for the next syscall instead of reloading the syscall number).

## [0.5.0] - Phase 5: ramfs + ELF32 loader + `exec()`

### Added
- `kernel/multiboot2.h`: minimal Multiboot2 info parser (`multiboot2_find_module()`) that walks the MBI's 8-byte-aligned tag list looking for a type-3 (module) tag, returning the module's physical start/end address if GRUB loaded one.
- `kernel/ramfs.c`/`ramfs.h`: flat read-only ramfs over a GRUB module — `ramfs_init(data, size)` reads a `uint32_t` entry count followed by an array of `ramfs_entry_t` (32-byte name + offset + size), and `ramfs_find(name)` does a linear scan with a hand-rolled `strcmp` (no libc).
- `kernel/elf.c`/`elf.h`: minimal ELF32 loader (`elf_load(cr3, elf_data, &entry_point)`). Validates the ELF magic, class (32-bit), endianness (little-endian), type (`ET_EXEC`) and machine (`EM_386`); walks `PT_LOAD` program headers, allocates and zeroes one physical page per page covered by `[vaddr, vaddr+memsz)`, maps them into the target process's page directory via `vmm_map_user_page`, and copies `p_filesz` bytes of file data into the mapped pages (resolving each destination through the new `vmm_get_phys_from_dir()` since the target CR3 isn't active yet).
- `kernel/exec.c`/`exec.h`: `exec(name)` ties ramfs lookup, ELF loading and process spawning together — finds the file in the ramfs, creates a fresh page directory, loads the ELF into it, maps an 8 KB (`USER_STACK_PAGES` = 2) user stack at `0x02000000`, and spawns it with `scheduler_spawn_user`.
- `tools/make_ramfs.py`: builds the ramfs image (`num_entries` header + fixed-size entry table + concatenated file data) from `name=path` arguments on the command line.
- `user/Makefile`, `user/link.ld`, `user/init.c`: the first userland program. Statically linked at `0x01000000` with `-nostdlib`, it calls `SYS_WRITE`/`SYS_YIELD` directly via inline `int $0x80` and prints `"init: hello from userland!\n"` in a `sys_yield()` loop.
- `tools/Makefile`: new `user` target builds `user/init.c` via `user/Makefile`; a `$(RAMFS_IMG)` rule runs `make_ramfs.py` against the built `init.elf`, and the ISO rule now copies `ramfs.img` alongside the kernel and registers it with `module2` in `tools/grub.cfg` (for both the default and serial-debug GRUB entries).

### Changed
- `kernel/main.c`: the Phase 3b/4 embedded ring-3 test blob (`user_blob`, `spawn_user_test`) and the unused `test_heap()` helper are both removed; `kmain` now looks for a GRUB module, and if present, calls `ramfs_init()` on it and `exec("init")`. Absence of a module is non-fatal — the kernel logs it and continues without a user process.
- `kernel/isr.asm`: `isr128` no longer writes the syscall's return value into the saved `eax` slot before `popa` — a ring-3 program now gets its own original `eax` back after `int 0x80`, not the syscall's result. (`SYS_GETPID`'s return value, for instance, is no longer observable in userland after this change; no caller currently depends on it.)

## [0.4.0] - Phase 3b + Phase 4: context switch, exception handlers, ring 3 usermode, syscalls

The project's own version banner stayed at `v0.4.0` across both phases in this entry — Phase 4 shipped without its own version bump (the diff below is combined rather than split into an invented intermediate version the project never actually used).

### Added
- `CLAUDE.md`: first version of the project's verification rules (never boot QEMU automatically, no long sleeps, always report the exact command for the user to run manually).
- `kernel/tss.c`/`tss.h`: Task State Segment (`tss_entry_t`, the full 26-field x86 TSS layout) — `tss_init()` zeroes it and sets `ss0`/`esp0` (the ring-0 stack the CPU loads on a ring 3 → ring 0 transition) plus the ring-3 segment values; `tss_set_stack()` updates `ss0`/`esp0` per-process before each context switch; a new GDT entry (`GDT_TSS`, selector `0x28`) is loaded with `ltr` in `gdt_init()`.
- `kernel/usermode.asm`/`usermode.h`: `jump_to_usermode(entry, user_esp)` builds the 5-word `iret` frame (`ss`, `esp`, `eflags` with `IF` forced on, `cs`, `eip`) targeting the ring-3 selectors (`CS=0x1B`, `SS/DS/ES/FS/GS=0x23`) and executes `iret` to drop into ring 3. Does not return.
- `kernel/syscall.c`/`syscall.h`: syscall dispatcher (`syscall_handler(num, arg1, arg2, arg3)`) with four syscalls — `SYS_WRITE` (writes to VGA only, no real fd table yet), `SYS_EXIT` (exits and yields, never returns), `SYS_YIELD`, `SYS_GETPID`. Unknown syscall numbers print a warning and return `-1`.
- `kernel/idt.c`/`kernel/isr.asm`: full 256-entry IDT (up from 48), CPU exception handling for vectors 0-31 via an `ISR_NOERRCODE`/`ISR_ERRCODE` NASM macro pair generating `isr0`-`isr31`, a shared `isr_exc_common` stub that calls the new `exception_handler(int_no, err_code, eip)` in C — which prints the exception name (a 32-entry name table), EIP, error code, and (for `#PF`) `CR2` plus decoded page-fault flags (protection/write/user) before halting. The syscall gate (`isr128`, vector `0x80`) is installed with DPL=3 so ring 3 can invoke it directly via `int 0x80`; it copies `eax`/`ebx`/`ecx`/`edx` into cdecl call arguments for `syscall_handler` and restores registers via `popa` before `iret`.
- `kernel/serial.c`/`serial.h`: COM1 (`0x3F8`) driver (`serial_init`, `serial_putchar`) mirroring all VGA output to the serial port for `-serial stdio` capture; `vga_putchar()` now also calls `serial_putchar()` for every character (including the automatic `\r` on `\n`).
- `kernel/memory/vmm.c`/`vmm.h`: `vmm_create_directory()` allocates a new page directory (must be within the first 8 MB), clones the two kernel page-table entries (0-8 MB identity map) into it, and leaves the rest zeroed; `vmm_map_user_page(pd_phys, virt, phys)` maps a page with user+writable flags in an arbitrary (non-active) directory by walking it via its identity-mapped physical address; `vmm_get_kernel_directory()`, `vmm_switch_directory()`.
- `kernel/process.c`/`process.h`: adds `cr3`, `user_stack`, `user_esp` fields; `process_spawn()` now creates a private page directory per process (falling back to the kernel's if creation fails); new `process_spawn_user()` builds a process whose `arg` field holds the ring-3 entry EIP and `user_esp` holds the ring-3 stack pointer.
- `kernel/scheduler.c`/`scheduler.h`: `scheduler_spawn_user()` wraps `process_spawn_user` with a `user_task_bootstrap` trampoline that updates the TSS (`tss_set_stack`) and calls `jump_to_usermode`. `scheduler_run_once()` now also calls `tss_set_stack()` before switching into any process (kernel or user) so a subsequent `int 0x80`/exception returns to the right kernel stack.
- `kernel/context_switch.asm`: `context_switch()` gained a third argument, `new_cr3` — if non-zero, it's loaded into `CR3` between saving the old stack and switching to the new one, giving each process its own address space across a context switch.
- `kernel/main.c`: adds an embedded ring-3 test program (`user_blob`, raw x86 machine code performing `SYS_WRITE`, `SYS_GETPID`, then a `SYS_YIELD` loop) and `spawn_user_test()`, which creates an isolated page directory, maps the blob's code and stack pages, and spawns it via `scheduler_spawn_user`.
- `tools/Makefile`: adds `kernel/{serial,syscall,tss}.c` and `kernel/usermode.asm`; switches `grub-mkrescue` to `grub2-mkrescue`.

`isr128`'s return path also writes `syscall_handler`'s result into the `eax` slot of the saved `pusha` frame before `popa`, so ring 3 receives the syscall's return value in `eax` after `int 0x80` (this was undone in v0.5.0 — see that entry).

## [0.3.0] - Phase 3a: cooperative scheduler

### Added
- `kernel/process.c`/`process.h`: fixed-size process table (`PROCESS_MAX` = 16), each with its own 4 KB kernel stack (`process_stacks[PROCESS_MAX][PROCESS_STACK_SIZE]`). `process_spawn()` builds an initial stack frame via `build_initial_stack()` (pushes the bootstrap entry point plus four zeroed callee-saved slots so `context_switch`'s `pop`s land correctly on first run) and assigns a name (`copy_name`, defaulting to `"kernel-task"`), a monotonically increasing PID, and `PROCESS_READY` state. States: `UNUSED`/`READY`/`RUNNING`/`SLEEPING`/`ZOMBIE`. `process_sleep`/`process_wake_sleepers` (tick-based), `process_exit` (marks `ZOMBIE`, no reclamation yet), `process_dump()` for a PID/state/runs/ESP/name table.
- `kernel/scheduler.c`/`scheduler.h`: cooperative round-robin scheduler on top of `process.c`. `scheduler_spawn()` wraps `process_spawn` with a `scheduler_task_bootstrap` trampoline that calls the task entry point, then `process_exit`s and `scheduler_yield()`s when it returns. `scheduler_run_once()` scans the process table starting after the last-run index and switches into the first `READY` one found; `scheduler_yield()`/`scheduler_sleep_current(ticks)` let a running task voluntarily give up the CPU. `scheduler_tick()` (called from the PIT IRQ) wakes sleeping processes whose deadline has passed.
- `kernel/context_switch.asm`: `context_switch(uint32_t *old_esp, uint32_t new_esp)` — saves `ebp`/`ebx`/`esi`/`edi` onto the current stack, stores `esp` into `*old_esp`, switches `esp` to `new_esp`, and restores the four registers before returning (now on the new stack).
- `kernel/timer.c`: the PIT IRQ0 callback now also calls `scheduler_tick(ticks)` on every tick.
- `kernel/main.c`: spawns two kernel tasks via `scheduler_spawn` — `heartbeat_task` (prints a beat counter every 100 ticks) and `heap_watch_task` (dumps the heap every 250 ticks) — and the main loop now calls `scheduler_run_once()` before each `hlt` instead of just halting.
- `tools/docker_build.sh`, `tools/run_qemu.sh`: new helper scripts wrapping the Docker cross-compile image (`randomdude/gcc-cross-i686-elf`) and the QEMU invocation, replacing the manual instructions in `tools/setup_env.sh`.
- `tools/Makefile`: cross-compiler switched from `x86_64-elf-gcc`/`-ld` to `i686-elf-gcc`/`-ld`; adds `kernel/context_switch.asm` to `ASM_SRCS` and `kernel/{process,scheduler}.c` to `C_SRCS`.

### Fixed
- `kernel/memory/heap.c`: `kmalloc()`'s heap-expansion path now appends the newly grown region onto the existing free-block list (walking to the tail and coalescing with it if free) instead of overwriting `heap_start_ptr` with a block that only covers the last page — the v0.2.0 behavior effectively leaked every page allocated before the first expansion.
- `kernel/memory/pmm.c`: the region marked used for the kernel image in `pmm_init()` grew from `0x10000` (64 KB) to `0x300000` (3 MB) bytes starting at `0x100000`, now that the process stacks and other statically-placed structures need more of low memory reserved.
- `boot/boot.asm`: `MB2_CHECKSUM` is now computed as `0x100000000 - (...)` instead of a bare negation, avoiding relying on NASM's handling of a negative immediate for an unsigned 32-bit field.
- `boot/linker.ld`: `.multiboot2` section gets `ALIGN(8)` and its header is wrapped in `KEEP(...)` so the linker can't garbage-collect it as unreferenced.

## [0.2.0] - Phase 2: PMM, VMM, kernel heap

### Added
- `kernel/memory/pmm.c`/`pmm.h`: bitmap-based Physical Memory Manager. Fixed bitmap at `0x202000` (right after the IDT), tracks up to 8192 pages (32 MB); `pmm_init(mem_upper)` marks everything used by default, frees high memory above 1 MB, then re-marks the kernel image (`0x100000`, 64 KB), the IDT region (`0x200000`, 12 KB) and low memory (`0x0F0000`, 64 KB) as used. `pmm_alloc_page`/`pmm_free_page`/`pmm_mark_used`/`pmm_mark_free`/`pmm_free_pages`/`pmm_total_pages`/`pmm_dump`.
- `kernel/memory/vmm.c`/`vmm.h`: Virtual Memory Manager with a page directory at the fixed physical address `0x10A000` and page tables starting at `0x10B000`. `vmm_init()` identity-maps the first 8 MB (two page tables, `pt0`/`pt1`) and enables paging by setting `CR3` and the `CR0.PG` bit. `vmm_map_page`/`vmm_unmap_page`/`vmm_get_phys`/`vmm_dump`; page tables for not-yet-mapped directory entries are allocated on demand from a bump pointer (`pt_next`) into the reserved `PAGE_TABLE_START`-`PAGE_TABLE_END` range.
- `kernel/memory/heap.c`/`heap.h`: kernel heap (`kmalloc`/`kfree`) starting at `0x400000` (4 MB), growing up to `0x800000` (8 MB) via `heap_expand()`, which allocates physical pages from the PMM and maps them through the VMM. Blocks are a doubly-linked list of `block_header_t` (magic `MAGIC_FREE`/`MAGIC_USED`, size, next/prev), first-fit allocation with block splitting, coalescing of adjacent free blocks on `kfree`, and `heap_dump()` for free/used byte counts. (A known issue at this point: growing the heap when no free block fits recreates a single block covering only the newly-added page, rather than appending it to the existing free list — fixed in v0.3.0.)
- `kernel/main.c`: adds a `test_heap()` helper exercising `kmalloc`/`kfree`/`heap_dump` (allocates 64/128/256-byte blocks, frees the middle one, reallocates, frees the rest), wires PMM → VMM → heap init into the boot sequence with `OK`/dump output for each, and updates the Phase 2 status banner. `test_heap()` itself isn't called from the normal boot path in this commit.
- `tools/Makefile`: adds `kernel/memory/{pmm,vmm,heap}.c` to `C_SRCS` and creates `build/kernel/memory/` in `dirs`.

## [0.1.0] - Phase 1: GDT, IDT, PIC, PIT, PS/2 keyboard

### Added
- `kernel/gdt.c`/`gdt.h`: 5-entry GDT (null, kernel code, kernel data, user code, user data — the ring-3 entries are defined but not yet used by any code path) built with `gdt_set_entry()` and loaded via the new `kernel/gdt_flush.asm` (`lgdt` + segment register reload + far jump to reload `CS`).
- `kernel/idt.c`/`idt.h`, `kernel/isr.asm`: 48-entry IDT placed at the fixed physical address `0x200000`, `idt_flush` (`lidt`), a generic `isr_handler()` that prints `*** EXCEPTION N ***` and halts, an `irq_handler()` dispatch table (`idt_register_handler`) that sends PIC EOI (`0x20`, plus `0xA0` for the slave) before invoking the registered callback, and hand-written `irq0`/`irq1` stubs in assembly (`pusha`/`call irq_handler`/`popa`/`iret`).
- `kernel/pic.c`/`pic.h`: 8259 PIC driver — `pic_init()` remaps IRQ0-7 to interrupt vectors 32-39 and IRQ8-15 to 40-47 (ICW1-ICW4 sequence with `io_wait()` delays), plus `pic_mask_irq`/`pic_unmask_irq`.
- `kernel/timer.c`/`timer.h`: PIT driver at a configurable frequency (100 Hz from `kmain`) using channel 0 (`0x40`)/command port (`0x43`), registered as the IRQ0 handler; `timer_get_ticks()` and a busy-`hlt` `timer_sleep(ms)`.
- `kernel/keyboard.c`/`keyboard.h`: PS/2 keyboard driver — a 128-entry US-layout scancode table, a 256-byte circular buffer fed by the IRQ1 handler (`keyboard_callback`, port `0x60`), `keyboard_getchar()` (blocks on `hlt` until a key is buffered) and `keyboard_haschar()`. Every received character is also echoed straight to `vga_putchar`.
- `kernel/main.c`: `kmain` now runs GDT → PIC (all 16 IRQs masked initially) → IDT → PIT (100 Hz) → keyboard init, then `sti`, and prints an `OK`/`FALHOU` status line per step (`print_tag`/`print_ok`) instead of the Phase 0 output.
- `tools/Makefile`: `CFLAGS`/`ASM_SRCS`/`C_SRCS` extended for the five new files; the `debug`/GDB target and `-nostdinc` flag from Phase 0 were dropped in this pass.

### Fixed
- None — this is the first phase past the initial boot skeleton, so there's nothing to regress against yet.

Note: a stray `kernel/main.c.save` editor backup (an accidental duplicate of `main.c` from mid-session with a couple of `#include` typos, e.g. `#include "timer.c"` instead of `timer.h`) was committed alongside this phase and stayed in the tree until it was removed in v0.20.1.

## [0.0.1] - Phase 0: boot + VGA driver

Initial commit. State of the tree at this tag:

### Added
- `boot/boot.asm`: Multiboot2 header (`mb2_header`, magic `0xE85250D6`, i386 arch tag, checksum computed as a negative sum, one mandatory end tag) and `_start`, which disables interrupts, sets up a 16 KB `.bss` stack (`stack_bottom`/`stack_top`), pushes the Multiboot magic/info pointer GRUB leaves in `eax`/`ebx`, and calls `kmain`.
- `boot/linker.ld`: links the kernel at `0x100000` (1 MB, where GRUB loads it), with `.multiboot2`, `.text`, `.rodata`, `.data`, `.bss` sections in that order and `.comment`/`.note*`/`.eh_frame*` discarded.
- `kernel/drivers/vga.c`/`vga.h`: VGA text-mode 80x25 driver writing directly to `0xB8000`. `vga_init`/`vga_clear`/`vga_set_color`/`vga_set_cursor`/`vga_putchar`/`vga_puts`/`vga_puthex`/`vga_putdec`; `vga_putchar` handles `\n`/`\r`/`\t`/`\b`, scrolls the buffer up a line via `vga_scroll` once past row 25, and moves the hardware cursor through I/O ports `0x3D4`/`0x3D5`.
- `kernel/main.c`: `kmain(multiboot_magic, multiboot_info_addr)` initializes VGA, prints an ASCII "NullOS" banner and a "Fase 0" status line, validates the Multiboot2 magic (`0x36d76289`) and halts with an error message if it doesn't match, prints the kernel load address and confirms VGA is active, then hangs in a `cli`/`hlt` loop (no scheduler yet).
- `tools/Makefile`: cross-compiles with `x86_64-elf-gcc`/`x86_64-elf-ld`/`nasm` (`-m32 -std=c99 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -nostdinc -O2 -Wall -Wextra`), links with the custom `linker.ld`, and packages a bootable ISO via `grub-mkrescue`. Targets: `all`, `run` (QEMU with serial stdio, no reboot/shutdown), `debug` (QEMU + GDB attached at `kmain`), `clean`.
- `tools/grub.cfg`, `tools/setup_env.sh`: GRUB menu entry and a helper script documenting cross-compiler setup options.
- `docs/setup.md`: build/toolchain setup notes.
- `README.md`: project pitch ("an experimental operating system built entirely with AI assistance"), phase roadmap table (0 done, 1–9 planned), and a text architecture diagram (`User Applications` / `Core Apps + GUI` / `libnull` / `System Services` / `NullKernel` / `Hardware`).

All source comments, boot messages and the README were in Portuguese/mixed Portuguese-English at this point (the English-only rule wasn't introduced until v0.10.1).
