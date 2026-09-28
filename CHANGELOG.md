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
- `SECURITY.md` (repository root, the file GitHub's Security tab shows): supported versions (only the latest tag, `v0.21.0` today), how to report a vulnerability (email `theshannondev@gmail.com`, best effort, no SLA, no bounty, receipt of the report is confirmed), and a "Phase 21: Copy-on-write fork() hardening" section (per-page reference count, `user_kptr_write()` breaking copy-on-write before kernel writes, `CR0.WP`, known limits). The Phase 14 and Phase 19 material stays in `docs/security.md` and is linked, not copied.

### Changed
- `docs/security.md`: the short Phase 21 note became a full section, "Copy-on-write fork() and memory safety (Phase 21)", and the "Relevant files" block lists the Phase 21 files.
- `tools/prev/` now holds the v0.21.0 snapshot (kernel + ramfs built from the `v0.21.0` tag in a clean worktree), so the "previous release" GRUB entry of the next version is v0.21.0.

## [0.21.0] - 2026-09-27 - Phase 21: Copy-on-write fork()

### Added
- **Copy-on-write `fork()` (Phase 21).** `process_fork()` no longer copies the parent's pages. Each user page is mapped into the child at the same physical frame, a writable page becomes read-only with the new `VMM_COW` PTE bit (bit 9, one of the bits the CPU leaves to the OS) in both processes, and the frame's reference count goes up by one. The parent's TLB entry is flushed with `invlpg` right after its PTE changes. The whole pass runs with interrupts off. A page that was already read-only without `VMM_COW` stays read-only in both, and a write to it is still a genuine fault.
- Per-page reference count in the PMM (`pmm_refcount[]`, a `uint16_t` per page, 4 KB in `.bss`; `PROCESS_MAX` is 16, and a compile-time check fails the build if it ever reaches the type's limit). Every allocation starts at 1; `pmm_page_ref()` adds one, and `pmm_free_page()` now drops one and returns the page to the pool only at 0. A page mapped with no count (reserved with `pmm_mark_used()`) counts as having one owner, so sharing it can never make it look free. Allocation, reference and free run with interrupts off.
- `vmm_cow_break()`: makes a copy-on-write page privately writable. The last owner gets the page back writable in place with no copy; any other owner gets a fresh copy and drops its reference to the shared one. The page-fault handler calls it for a write to a present page (`exception_handler()`, error code bits 0 and 1) and returns so the CPU retries the write; with no memory for the copy the fault takes the fatal path as before.
- Kernel writes into user memory (`copy_to_user()`, `SYS_GETARG`) go through a new `user_kptr_write()` that breaks copy-on-write first. The kernel writes through physical addresses, so the read-only PTE alone would not stop it from writing into a frame another process still maps.
- `SYS_PAGEREF` (34) / `nos_pageref(addr)`: reference count of the page behind a user address (1 = private, N = shared by N processes), for tests.
- `kernel/irq.h`: `irq_save()`/`irq_restore()`, moved out of `process.c` and shared with `pmm.c`/`vmm.c`.
- selftest tests 22–24 (24 tests total): parent and child writes stay private in both orders, with the refcount at 2 after `fork()` and 1 after the first write; three generations share one page and the refcount goes 3 → 2 → 1 as the youngest and then the middle process exit without writing, with the page content intact; four children rewrite four shared pages for ~300 ms each under timer preemption, and the parent ends with its own content and a refcount of 1 on every page.
- Documentation for the phase: a "Copy-on-write fork()" section in `docs/memory.md` (the `VMM_COW` bit, the reference count, what `fork()` and the page-fault handler do, `CR0.WP`, what `process_exit()` releases and what still leaks), `SYS_PAGEREF` in `docs/syscalls.md`, and updates to `docs/scheduler.md`, `docs/security.md` and `docs/testing.md` (tests 22 to 24, 24 tests in total).
- `.claude/skills/de-ai-writing/`: project skill for rewriting text without the signs of AI writing catalogued in Wikipedia's "Signs of AI writing" guide (`SKILL.md` with the workflow, `references/signs.md` with the full catalogue and a fix for each sign). `CLAUDE.md` gains a section requiring public text (README, CHANGELOG, CONTRIBUTING, code comments, social posts, issue templates) to go through it; facts, numbers and claims stay unchanged. The optional `scripts/check_ai_signs.py` scanner was not added; the manual pass over `references/signs.md` is the fallback. Not yet run over the existing docs.
- GitHub issue templates in `.github/ISSUE_TEMPLATE/` (bug report, feature request, boot/compatibility report).
- `CONTRIBUTING.md`: contribution guide with the pull request policy. PRs must be AI-generated, and the commit must prove it with a `Co-Authored-By` trailer naming the AI tool (Claude Code adds it automatically); PRs without the trailer are closed, and code that reads as hand-written despite it is sent back to be redone. It also lists what to read before opening a PR (`CLAUDE.md`, `ROADMAP.md`), the local build-and-boot expectation, and what kinds of contributions are welcome.

### Changed
- `process_exit()` now drops the exiting process's reference to each of its user pages, so a page is freed when no process maps it any more (the data-page part of ROADMAP 23-A/23-C, pulled into Phase 21). The page directory and page tables are still leaked (23-B). The release, clearing `cr3` and marking the slot unused happen in one interrupt-off section, and a slot claimed by `fork()`/spawn starts with `cr3 = 0`, so killing a half-built process never walks the previous occupant's directory.
- CR0.WP is set together with paging, so a ring-0 write through a read-only user PTE faults (and is resolved if it is copy-on-write) instead of writing into the shared frame.
- `vmm_map_user_page()` is now a wrapper over `vmm_map_user_page_flags()`; new `vmm_get_user_pte()`.
- ROADMAP.md: Phase 26 gains sub-phase 26-E (USB HID keyboard driver) and the canonical input event, which was 26-E, becomes 26-F and is named **Deflection**; Phase 27 is split into 27-A **Cathode** (graphics API / framebuffer driver) and 27-B **Raster** (launcher/grid, the GUI delivered in the phase, on top of Cathode).
- `tools/prev/` now holds the v0.20.1 snapshot (kernel + ramfs built from the `v0.20.1` tag), so the "previous release" GRUB entry of the next version is a real release.
- ROADMAP.md: Phase 21 is marked closed; the data-page parts of 23-A and 23-C are marked done in Phase 21's work.
- `kernel/version.h` bumped to `0.21.0`, phase `21` "Copy-on-write fork()".

### Fixed
- `make test-elf` failed on whichever program it loaded last once `selftest.elf` grew (its new page-aligned test buffers): `tools/test_elf_load.c` never unmapped the pages and image buffers of earlier loads, and its thousands of fuzzing loads ran the host out of 32-bit `mmap` space. Each load is now released before the next one. The kernel's ELF loader was not involved.
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
- Git history rewritten for the move: author, committer and tagger identity, the copyright holder in `LICENSE` and the repository links in the historical docs were normalized. Every commit hash changed, so hashes quoted in older notes or issues no longer resolve; the commit messages, dates and tag names are unchanged in substance.
- `.gitignore` gains `*.log` (serial logs captured by hand with `make run ... | tee boot_serial.log` shouldn't be tracked — see Removed).
- `kernel/version.h` bumped to `0.20.1` (PATCH: no new phase, only the `debug` boot argument fix below); the README banner follows.

### Fixed
- **The `debug` boot argument of the "serial debug mode" GRUB entry did nothing.** GRUB passed it and the kernel could read the command line, but no code ever looked for it, so that entry booted exactly like the default one. `hal_boot_init()` now sets a global flag, `g_debug_boot`, when the word `debug` is on the command line. While it is set, `kmain` writes extra `[DEBUG]` detail to the serial port only (nothing changes on screen): the command line and the bootloader's memory map region by region, the boot configuration values that decide Safe Mode (`fail_count`, threshold, `safemode` flag, pending crash), and one line per boot step (ATA, boot config, PMM, paging, heap, scheduler, FAT16, PCI, ramfs/shell) with the PIT tick count. No new subsystem, no allocation. Without the argument the boot output is unchanged. Docs: `docs/setup.md` ("Debug via serial"), `docs/quickstart.md`, `docs/safemode.md`.

### Removed
- `kernel/main.c.save`: an editor backup file that had been committed by mistake.
- `tools/boot_serial.log`: a 491-line hand-captured serial transcript from testing the v0.20.0 crash handler, committed by mistake in that version's polish; untracked now that `.gitignore` excludes `*.log`.
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
  handling skips the dump and goes straight to the reset. New `kernel/crashdump.c/h`
  hold the record format and the save/load/clear logic.
- The save path depends on nothing that may be broken: polling-only ATA I/O
  (`ata_crash_read_sector`/`ata_crash_write_sector`, `block_*_polled()` in the HAL —
  no lock, no IRQ, no scheduler, bounded waits, soft reset of a channel left
  mid-command), a static buffer, no heap. New `bootcfg_buf_*` functions (the text
  store on an explicit buffer, hex numbers, `bootcfg_remove()`), `timer_poll_delay_ms()`
  (new `kernel/timer.c/h`), `power_reboot_request()` (split out of `power_reboot()`),
  `exception_name()`.
- A temporary serial-only trace of every step of the ATA `probe()`
  (`[ATADBG]`, marked `TEMP-DEBUG(ata-probe)` in `kernel/drivers/ata.c`) stays in
  the kernel to catch the rare intermittent "no disk" at boot (see `PROGRESS.md`);
  it writes to the serial port only.
- **`crash <de|pf|gpf>` shell command**, a debug tool that faults on purpose
  (#DE, a read of `0xDEADBEEF`, #GP) so the whole pipeline can be tested repeatably
  with `make run-reboot-test`. Documented in `docs/safemode.md`.
- `tools/boot_serial.log`: a hand-captured serial transcript of the `crash`
  command exercising all three fault types, added to the tree during this
  phase's testing and later found to have been committed by mistake (removed
  in 0.20.1).

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
- **`printf` family in libnos** (`user/lib/nosstdio.c`, 253 new lines):
  `printf`, `vprintf`, `sprintf`, `snprintf`, `vsnprintf` with the standard libc
  names — `%d %i %u %x %X %c %s %p %%`, flags `- 0 + space`, width and
  precision (also `*`), `h`/`hh`/`l`. No floating point, no 64-bit, no `#`. A
  separate object linked only into the programs that use it.
- **`make test-elf`** (`tools/test_elf_load.c`, 151 new lines): a host-side test
  of the real `kernel/elf.c` on every built user program, plus truncation,
  header fuzzing and crafted hostile headers.
- selftest: three new tests (exec of a program that exists only on FAT16;
  malformed, truncated and missing programs rejected; the printf family),
  154 new lines in `user/selftest.c` — 21 tests total.
- `docs/quickstart.md` (124 new lines) and `docs/sdk.md` (103 new lines).

### Changed

- `elf_load()` (`kernel/elf.c`/`elf.h`) takes the file size and no longer trusts
  the image: the program header table and every segment's file data are
  checked against it, segments must lie in `[0x00800000, 0x02000000)`, and all
  checks use 64-bit arithmetic so a 32-bit field cannot wrap. All headers are
  validated before anything is mapped. This also closes the "`elf_load` never
  receives the file size / `page_end` overflow" item of the Phase 28 plan.
  `exec()`/`exec.h` thread the size through.
- The kernel heap is grown to 256 KB when it is initialized, and `heap_expand()`
  takes exactly the physical page at `heap_end` (`pmm_alloc_page_at()`, new in
  `kernel/memory/pmm.c/h`) instead of the lowest free one — the heap's virtual
  addresses are the identity-mapped physical ones (see Fixed). The heap can no
  longer grow once processes exist, a limit recorded in `PROGRESS.md` for
  Phase 22.
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
  column; not a translation system): `kernel/messages.h/.c` (279/288 new
  lines) for the kernel (boot log, errors, dumps, exception names, `ps`
  states) and `user/lib/messages.h/.c` (85/98 new lines) for the shell,
  editor and `cat` (`umsg_id_t`, `UMSG_*`, linked only into those programs).
  `msg()` never returns NULL (`"(?)"` for a bad ID) and works from the
  exception handler. Output is unchanged. `selftest`/`forktest` and
  `init`/`spintest` are not migrated.
- **Safe Mode** (`docs/safemode.md`), a recovery environment inside the same
  kernel binary that runs in ring 0 before the PMM, VMM, heap, scheduler,
  `exec` and syscalls exist:
  - a boot configuration store in one raw sector (LBA 1, inside FAT16's
    reserved region, read and written only through the HAL block I/O, magic
    line `# nullos-config v1`, empty defaults when the sector is invalid,
    guarded by the boot sector's `reserved_sectors`) — `kernel/bootcfg.h/.c`
    (64/175 new lines);
  - a boot failure counter (`boot_fail_count`): incremented right after the
    disk is up, reset on the first keyboard read; at
    `BOOTCFG_FAIL_THRESHOLD` (3) failed boots in a row, or with `safemode` on
    the boot command line, the kernel enters Safe Mode instead of booting;
  - a text UI (`kernel/safemode.c`, 392 new lines, static buffers, no heap):
    reboot normally (resets the counter), a reboot submenu, disk info from the
    raw boot sector, and a sector hexdump; Safe Mode is left only by rebooting;
  - a restricted read-only shell (menu item 5, `kernel/safeshell.c`, 180 new
    lines) that initializes the PMM, VMM, heap and FAT16 on demand, once per
    session, and offers `help`, `ls`, `cat`, `pwd`, `cd`, `back` calling FAT16
    directly — no processes, no `exec`;
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

### Fixed
- README.md/PROGRESS.md: "For planned Phases 17–31" corrected to "17–30" (ROADMAP was restructured to end at Phase 30, the DOOM port, with v1.0.0 closing right after it).
- README.md: `make run` documented as keeping `-no-reboot`; the new `make run-reboot-test`/`make inject` targets from `[0.17.0]` documented in the "Build and run" section.
- docs/testing.md: expected selftest output block updated to the real 18/18-test output introduced in `[0.17.0]` (it still showed the old 13/13 block).

### Changed
- PROGRESS.md: noted the permanent "NullOS vX.Y.Z (anterior)" GRUB fallback entry CLAUDE.md requires at every merge into `main` was never actually implemented for the `v0.16.0`/`v0.17.0` merges, and is owed to ROADMAP's Phase 18-B.

## [0.17.0] - Phase 17: Cleanup A (audit fixes, libnos/shell tools, test/build infrastructure)

### Added
- **Power management** (`kernel/power.c/h`): `power_reboot()` pulses the 8042 controller (`0xFE` → port `0x64`) after draining its input buffer, printing a failure message if the machine is still running afterward; `power_shutdown()` finds the PIIX4 power-management function (8086:7113) via `pci_find_device()` and writes SLP_EN/SLP_TYP=0 to its `PM1a_CNT` I/O register (ACPI S5, works under QEMU). `SYS_REBOOT (31)`/`SYS_SHUTDOWN (32)` and shell `reboot`/`shutdown` commands.
- `pci_find_device(vendor, device, &bus, &dev, &fn)` and `SYS_PCI_FIND (33)` / `nos_pci_find()`, so `selftest` can check for a specific device (Intel 440FX, 8086:1237) instead of only "found ≥ 1"; noted as expected to break once Phase 24 moves QEMU to `-machine q35`. PCI BAR reads are now per header type (type 0: 6 BARs, type 1 bridge: 2, type 2 CardBus: 1).
- `fat16_get_path(cluster, out, size)` / `SYS_GETCWD (30)` / `nos_getcwd()` / shell `pwd`: rebuilds a directory's absolute path by walking `..` entries and looking up each child's name in its parent, capped at `FAT16_PATH_MAX_DEPTH` (16) against a corrupted chain.
- `vfs_write_all()` (whole-file replace, backs `SYS_WRITE_FILE`) is now distinct from `vfs_write()` (stream write at `fd->pos`, backs `SYS_WRITE`); `fat16_write_at(parent_cluster, name, pos, ...)` does the positional write, growing the cluster chain as needed, re-looking-up the dirent fresh every call, writing data → FAT → dirent in that order so an I/O failure can't leave a dirent claiming more than was written.
- Shell: `cat [file]` (`user/cat.c` extended to print a named file, or copy stdin when given none), `cmd < file` / `cmd > file` (`run_redirected()`) — the file is opened by the shell and passed as an fd through the same `SYS_EXEC_PIPE` the pipe operator uses (external programs only, not combinable with `|`, builtins can't be redirected).
- `user/lib/nullos.c/h` gains standard-named string/memory helpers (`memcpy`/`memset`/`memmove`/`memcmp`/`strlen`/`strcmp`/`strncmp`, plus `nos_uitoa`) — needed because GCC itself can emit calls to `memcpy`/`memset`/`memmove` (struct copies, loop-idiom recognition); the `mem*` ones use `rep movsb`/`rep stosb` so GCC can't turn the implementation into a call to itself. Per-program copies of these helpers were removed.
- `tools/Makefile`: `make run-reboot-test` (same as `make run` but without `-no-reboot`, so `reboot` actually restarts the guest instead of looking like a shutdown); `make inject FILE=... [NAME=...]` copies a file onto `build/disk.img` via `mcopy` without rebuilding the ISO (host-side only — `exec()` still can't load from FAT16, that's Phase 19).
- `docs/TODO.md`: a running stub file for documentation owed but not yet written, to be emptied at each version's polish (per the new CLAUDE.md "Definition of Done" convention).
- `user/selftest.c`: 6 more tests (18 total) — `SYS_WRITE` accumulating past 128 bytes across a cluster boundary, the 440FX PCI check, a real two-process pipeline (`fork()` + `SYS_EXEC_PIPE` cat), `waitpid` correctness across 3 concurrently-exiting children, and 3-level-deep `mkdir`/`cd`/`pwd`.

### Fixed
- **`vmm_map_page()`/`vmm_map_user_page()` used to return `void`, silently swallowing an out-of-range address or an exhausted page-table pool.** They now return `int` (0 success, `VMM_ERR_RANGE`/`VMM_ERR_NOMEM`); every caller (`heap_expand()`, `exec()`'s user-stack loop, `elf_load()`, `process_fork()`) checks the result, and the three `vmm_map_user_page` call sites now free the physical page they'd already allocated instead of leaking it on failure.
- `vmm_map_user_page()` now rejects `virt < 0x800000`: the first 8 MB is the kernel identity map shared by every process's page directory, so a user mapping there would have altered it for every process.
- **`pmm_init(mem_upper)`'s page-count formula, `(1024 + mem_upper) * 1024 / PAGE_SIZE`, overflowed `uint32_t` for a large `mem_upper` and wrapped to a tiny page count.** Replaced with `256 + mem_upper / 4`; freeing memory above 1 MB is now skipped with a warning if `total_pages <= 256`.
- `alloc_pid()` used a bare `cli`/`sti`, which would re-enable interrupts too early when called from inside `process_fork()`'s own `cli` section; switched to `irq_save()`/`irq_restore()` (saves/restores EFLAGS).
- `process_spawn_user()` now reserves its process-table slot atomically (`cli`/EFLAGS-save, pick slot, mark `PROCESS_BLOCKED`, assign pid) the same way `process_fork()` already did, closing the previously-documented race between two concurrent spawns and the scheduler running a slot whose `esp`/`cr3` weren't built yet.
- `fat16_init()` now rejects a BPB with `sectors_per_cluster == 0` before using it as a divisor.
- `fat16_write_file()` refuses a directory entry and no longer re-reads the dirent sector `dir_lookup()` just left in `dir_buf`.
- `sys_read()` no longer echoes a backspace on an empty line, which used to blank the shell's own `> ` prompt.
- `SYS_READ_RAW` now packs a Shift bit (bit 9) alongside Ctrl's (bit 8), since the kernel consumes the Shift make/break scancodes itself before `user/edit.c` ever sees them; `edit.c` gains its own `sc_map_shift[]` table.
- `sys_exec_pipe()` clears the global `exec_arg`, since it carries no argument of its own — without this, a leftover `arg` from an earlier plain `exec()` could reach the new process's `SYS_GETARG` (e.g. a piped `cat` seeing a stale filename instead of just reading its redirected stdin).

### Removed
- The dead kernel-task spawn path, `process_spawn()`/`scheduler_spawn()` and `scheduler_task_bootstrap()` (unused since real user processes replaced it), was removed.
- `tools/run_qemu.sh`, the older standalone script that booted the ISO without attaching the disk, was removed in favor of `cd tools && make run`; `tools/setup_env.sh`'s printed next-steps updated to match.
- `-no-reboot` was removed from the base `QEMU_FLAGS` (kept only in the new `QEMU_FLAGS_RUN` used by `run`/`debug`) so `run-reboot-test` can omit it.

### Changed
- `docs/setup.md`/`docs/kernel.md`/`docs/pipes.md`/`docs/filesystem.md`/`docs/scheduler.md`/`docs/syscalls.md`/`docs/shell.md`/`docs/testing.md`/`docs/pci.md`/`docs/memory.md` all updated for the above; `PROGRESS.md` reorganized/condensed per its own >200-300-line maintenance rule (closed-phase detail moved out into one line each, referring to CHANGELOG.md/README.md).

## [0.16.0] - Phase 16: pipes and real waitpid

### Added
- **Inter-process pipes** (`kernel/pipe.c/h`): a fixed static pool, `pipe_table[PIPE_MAX]` (`PIPE_MAX=8`, `PIPE_BUF_SIZE=512` bytes each), never `kmalloc()`'d — following the same fixed-array style as `process_table`/`g_gate_waiters`, and deliberately avoiding a second unreclaimed heap allocation on top of the known `process_exit()` leak. Single reader/single writer per pipe, mirroring `ata.c`'s `g_irq_waiter` precedent.
- `vfs_fd_t` gains two backend tags, `VFS_PIPE_READ`/`VFS_PIPE_WRITE`, and a new `vfs_dup()` that bumps a pipe's refcount on duplication (`fork()`, `SYS_EXEC_PIPE`) — needed because a raw struct copy without it would let one holder's `close()` drop the pipe's refcount below the number of copies still genuinely open.
- Blocking read/write on pipes, reusing `PROCESS_BLOCKED`/`scheduler_block_current()`: a full-buffer write blocks on `pipe->write_waiter`, an empty-buffer read blocks on `pipe->read_waiter` (unless `write_refs == 0`, in which case it returns EOF immediately); each side wakes the other under `cli`/`sti` when it frees space/produces data. Writer exit propagates EOF to a blocked reader; reader exit propagates a broken-pipe error (-1) to a blocked writer.
- `SYS_PIPE (28)` / `nos_pipe(fds)`: creates a pipe, returns a read fd and a write fd usable with `SYS_READ`/`SYS_WRITE` like any other fd.
- `SYS_EXEC_PIPE (29)` / `nos_exec_pipe(name, stdin_fd, stdout_fd)`: launches a pipeline stage without `fork()`+`dup2()`+`exec()` (which doesn't apply here — NullOS's `exec()` spawns a brand-new process rather than replacing the caller's image, the same reason the Phase 15 `cwd_cluster` bug existed). `process_spawn_user()` gains a `start_blocked` parameter, threaded through `scheduler_spawn_user()`/`exec()` the same way `cwd_cluster` was in Phase 15: the new process is left `PROCESS_BLOCKED` until `sys_exec_pipe()` seeds its `fd_table` with the redirected end(s) and sets `stdin_redirect`/`stdout_redirect` (new `process_t` fields, default `-1`), then calls the new `process_make_ready()` to flip it to `PROCESS_READY`.
- `sys_read()`/`sys_write()` transparently resolve fd 0/1/2 through `stdin_redirect`/`stdout_redirect` when set, so a program (e.g. `user/cat.c`, a minimal pipe sink reading stdin and writing stdout) needs zero pipe-awareness.
- Shell: `cmd1 | cmd2` (`run_pipeline()` in `user/shell.c`) — both sides must be real ramfs programs, not builtins (builtins write straight to VGA, never through fd 1). The shell closes its own copies of the pipe's read/write fds after launching both stages (skipping this would leave the write end's refcount above 0 even after the real writer exits, hanging the reader forever) and calls `nos_wait()` on both pids.
- Real blocking `waitpid`: `process_t.waiting_for_pid` set right before blocking and cleared right after waking (inside one `cli`/`sti` section, same discipline as `ata_wait_irq()`); `process_exit()` scans for any `PROCESS_BLOCKED` process whose `waiting_for_pid` matches and wakes only those. `SYS_WAIT`'s interface (already a specific pid, not "any child") is unchanged — only the previous 100ms-polling implementation (`scheduler_sleep_current(10)`) is replaced.
- `docs/pipes.md`: new file with the full pipe design (storage, blocking protocol, refcounting/EOF, `SYS_EXEC_PIPE`, manual test).
- `user/cat.c`: minimal pipe sink, added specifically to have a real second endpoint to test `forktest | cat` against (existing builtins like `ps`/`echo` bypass fd 1 entirely).

### Fixed
- **`kernel/keyboard.c` had no Shift handling at all** — found because it blocked typing `cmd1 | cmd2` in the shell (`Shift+\` never produced `|`, `Shift+5` never produced `%`). There was no shifted scancode table and no Shift press/release tracking (only `ctrl_pressed` existed); every character came from the single unshifted `scancode_map`. Fixed by tracking Shift (`0x2A`/`0x36` press, `0xAA`/`0xB6` release) and adding a second, index-matched `scancode_map_shift` table. The raw-scancode path used by `user/edit.c`'s own `sc_map` table has the identical gap and was deliberately left unfixed (out of scope — see below).
- `sys_exit()`'s fd cleanup used to zero `fd_table[slot][j].used` directly, bypassing `vfs_close()` — harmless for ramfs/FAT16 (no-op beyond that flag) but for pipes this was the only place a process's pipe-end references were ever released on exit. `sys_exit()` now calls `vfs_close()` per used fd.

### Changed
- `SYS_FORK`'s existing raw `fd_table` copy is now followed by `vfs_dup()` on every duplicated entry, so a forked child's pipe-end reference is properly counted (no-op for ramfs/FAT16 entries).
- `process_fork()` also copies the new `stdin_redirect`/`stdout_redirect` fields parent→child.

### Removed / known limitations
- `user/edit.c`'s raw-scancode input still has no Shift support (typing an uppercase letter or a shifted symbol inside the editor is unaffected by this phase's keyboard fix) — tracked as known technical debt, needs its own fix in both `keyboard.c`'s raw path and `edit.c`'s `sc_map` table.
- `SYS_EXEC_PIPE` has no `arg` parameter — all 3 syscall registers are spent on `name`+`stdin_fd`+`stdout_fd`, so a piped command can't take a `SYS_EXEC`-style filename argument in this first cut.

## [0.15.1] - libnos: shared user-space syscall wrapper library

### Added
- `user/lib/nullos.c/h` ("libnos"): one thin `nos_*` wrapper per syscall (`nos_write`, `nos_open`, `nos_fork`, `nos_mkdir`, etc.), doing `int $0x80` with the matching `SYS_*` number included straight from `kernel/syscall.h` and returning whatever the kernel put in `eax` — no added logic, no retries. Replaces six independent, hand-written copies of the same wrappers previously duplicated across `shell.c`, `edit.c`, `forktest.c`, `selftest.c`, `init.c`, `spintest.c`.
- `user/Makefile`: `lib/nullos.c` compiles once to `$(BUILD)/lib/nullos.o`, which every program's link line now includes alongside its own `.c` file.
- `docs/kernel.md`: new "User-space syscall library (libnos)" section documenting the design and the steps to add a syscall wrapper for a new syscall (one function in `nullos.c`/`nullos.h`, no changes to programs that don't need it).

### Changed
- `nos_write`/`nos_read` take an explicit `fd` argument. The wrappers they replaced had `fd` hardcoded inline (`1` for write, `0` for read) in some programs even though the real `SYS_WRITE`/`SYS_READ` syscalls always took `(fd, buf, len)`; `edit.c` and `selftest.c` had already independently converged on the explicit form, so this standardizes on that.
- `nos_exec(name, arg)` replaces `shell.c`'s old `sys_exec(name)` / `sys_exec_arg(name, arg)` split with the syscall's real 2-argument signature (`arg` may be `NULL`).

### Fixed
- The old single-argument `sys_exec(name)` inline asm never constrained `ecx`, so the kernel's `sys_exec` read whatever garbage was in `ecx` as the argument pointer. Harmless in practice (an invalid address just made `copy_user_str` fail silently), but fixed by `nos_exec`'s unified 2-argument form rather than preserved.

## [0.15.0] - Phase 15: FAT16 subdirectories

### Added
- **FAT16 subdirectories**: `mkdir`/`cd`, and path-aware `touch`/`edit`/`ls` (e.g. `edit docs/notes.txt`). A subdirectory is an ordinary dirent with `ATTR_DIRECTORY` whose `first_cluster` starts a regular FAT chain (grows like file data); its first sector holds `.`/`..` entries. The root stays the old fixed-size region with no chain and no `.`/`..`.
- Shared lookup/insert/path-walk core in `kernel/fs/fat16.c`, replacing the old root-only scan, so every FAT16 code path goes through the same logic instead of duplicating it: `dir_iter_t`/`dir_iter_next_sector()` (steps through a directory's sectors, root or chain), `dir_lookup()` (the one place that builds the 11-byte name and compares it), `dir_insert()` (the one place that finds a free/deleted slot and writes a dirent, extending a subdirectory's chain via `fat16_alloc_cluster()` when full), `resolve_path()` (walks a `"/"`-separated path via `dir_lookup()`, stopping right before the final component), `fat16_flush_fat()`, `fat16_free_chain()`. `fat16_find`, `fat16_create`, `fat16_mkdir`, `fat16_readdir`, `fat16_resolve_dir` are now thin wrappers over this core.
- `process_t.cwd_cluster` (`kernel/process.h`): the process's current directory (0 = root). Copied parent→child in `process_fork()`. `exec()` gained an explicit `cwd_cluster` parameter (`kernel/exec.c/.h`), threaded through `scheduler_spawn_user()`/`process_spawn_user()`, so `run`/`edit` (which spawn a brand-new process, not a fork of the caller) also inherit the launching process's cwd instead of always starting at the root.
- `SYS_CHDIR (26)` / `chdir(path)`: changes the caller's `cwd_cluster`, only on confirmed success (`fat16_resolve_dir()` returning 1) — every failure path leaves it untouched.
- `SYS_MKDIR (27)` / `mkdir(path)`: creates a directory; idempotent if a directory of that name exists, fails if a file does.
- `SYS_READDIR`'s signature changed to take an optional path argument (NULL/empty = caller's cwd); a given path is resolved via `fat16_resolve_dir()` before anything is printed, and an unresolvable path is reported without falling back to the cwd. `ls` output now marks directories with `<DIR>` instead of a byte size.
- `user/selftest.c`: four new tests (`mkdir`, file write/read roundtrip inside a subdirectory, `fork()` inheriting `cwd_cluster`, subdirectory files not leaking into the root), bringing the suite to 11/11. Test filenames were deliberately renamed to have distinct FAT 8.3 encodings (`st_root.txt`/`st_sub.txt`/`st_mark.txt`) after an earlier revision using `selftest_*.txt` names collided (see Fixed).
- `docs/setup.md`: Arch Linux manual package list, and a Windows/macOS section documenting the `tools/docker_build.sh` path (untested on either platform, flagged as such).

### Fixed
- **`to_8_3()` dropped the extension of any base name longer than 8 characters** (e.g. `"selftest_tmp.txt"` → `"SELFTEST"` instead of `"SELFTEST.TXT"`): the base-name loop stopped scanning at the 8-char cap without checking further ahead for a real `.`. It now keeps scanning (without writing) past the cap until it finds the `.` or the end of the string. This had already produced a real collision: `selftest_tmp.txt`/`selftest_sub.txt`/`selftest_fork_marker.txt` all packed to the identical on-disk name `SELFTESTTXT`, causing a false test failure in `user/selftest.c`'s subdirectory-isolation check (fixed by renaming the test files, not by changing the truncation rule further — an inherent 8.3 limitation).
- `to_8_3(".")`/`to_8_3("..")` now special-cased up front instead of going through the normal name/extension split, which treated the leading `.` as the extension separator and produced the wrong bytes for both — without this, `resolve_path()` could never match the `.`/`..` entries `fat16_mkdir` writes, breaking `cd .`/`cd ..` in every subdirectory.
- **`g_irq_fired` (`kernel/drivers/ata.c`) is now reset the moment a new ATA command is issued** (`ata_read_sector`/`ata_write_sector`, for `CMD_READ`/`CMD_WRITE`/`CMD_FLUSH`), not only when a wait for one finishes. Found via an intermittent bug during this phase's testing: `fat16_init()`'s ~65 boot-time reads go through the polling fallback (no current process yet) and never consume `g_irq_fired`, so a real completion IRQ from one of them could sit stale until a later, genuinely IRQ-driven wait mistook it for its own command's completion and returned without actually waiting.
- `run`/`edit` (`exec()`/`SYS_EXEC`) previously always started the new process at the root regardless of the caller's cwd, unlike `fork()`, which already inherited it — found during this phase's own manual testing.
- A freshly allocated directory cluster is now explicitly zeroed sector-by-sector before `.`/`..` are written into it, so leftover data from a reused cluster can't be misread as real dirents on the first scan.

### Changed
- `fat16_find`/`fat16_create`/`fat16_mkdir`/`fat16_resolve_dir` all now take an explicit directory cluster (cwd) parameter and resolve relative paths against it; `vfs_open`/`vfs_create` (`kernel/fs/vfs.c`) pass the caller's `cwd_cluster` through. `vfs_fd_t` caches `parent_cluster` + the final path component at open/create time rather than the full path, so a later `vfs_write()` doesn't depend on the process's cwd at write time (which could have changed via `cd` since the file was opened).
- `fat16_write_file(parent_cluster, name, buf, len)` now takes an already-resolved parent cluster and single final path component instead of a full path.
- Name-collision policy: `fat16_create` fails if a directory already exists under that name; `fat16_mkdir` fails if a file already exists under that name (both remain idempotent for a matching-type existing entry).
- ROADMAP.md phase numbers 15+ shifted by one: FAT16 subdirectories was originally planned as "Phase 17" but landed before the two process-related phases (pipes, copy-on-write fork) that preceded it in that list, taking the next available completed-phase slot (15); those two are renumbered 16/17.

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
