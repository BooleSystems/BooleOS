# PROGRESS.md — working memory for future sessions

This file is **not** end-user documentation — that's what `README.md` is for.
Each Claude Code session starts with zero memory of prior sessions and only
sees `CLAUDE.md`, the code, and `git log`. This file answers two questions:
"where are we now" and "what's left", plus the non-obvious "why" behind
code that would otherwise be silently re-broken.

Do not duplicate README/docs content here. `README.md` is a lean index
(completed-phases table, links, build); `ROADMAP.md` holds future phases;
`docs/<system>.md` holds detailed per-system content; `CHANGELOG.md` and
`git log` hold history. Closed-phase detail does NOT belong here.

## Current status

Current version: **0.23.0** (`kernel/version.h`). Last closed phase:
**Phase 23** (process memory release: `process_exit()` frees the page
directory and page tables), released as `0.23.0`, tagged `v0.23.0`.
Working on `0.24.0-nightly` (`BOOLEOS_PHASE` stays `23` until Phase 24 closes).

### In progress: 0.24.0 / 24-A (raw e1000 driver)

Status: implemented on `nightly`, **not yet validated in QEMU**. Investigation,
from the code:

a) **PCI (`kernel/drivers/pci.c`)** had reads only (`pci_config_read32/16/8`),
   no config writes and no BAR sizing. Added `pci_config_write32/16`
   (16-bit uses `outw` on `0xCFC + (offset & 2)`, so writing the command
   register never rewrites the status register's write-1-to-clear bits) and
   `pci_bar_size()` (memory decode off, write all ones, read, restore).
   `pci_find_device()` finds a vendor:device in the boot scan table.
b) **VMM:** `vmm_map_page()` maps into the kernel directory only, with any low
   flag bits (so PCD 0x10 | PWT 0x08 work). `vmm_create_directory()` copied
   ONLY PDE 0/1, so a kernel PDE added for MMIO would reach no process. Three
   things had to change, all in `vmm.c`/`process.c`:
   - `pt_next` started at `PAGE_TABLE_START` although `vmm_init()` already
     used the first two tables there (known debt): the first new kernel page
     table (the MMIO one) would have overwritten the 0–4 MB identity map.
     Now starts after them.
   - `vmm_create_directory()` copies every present kernel PDE, not just 0/1.
     The PDE points at the same kernel page table, so later PTE changes inside
     it reach every directory; a PDE added to the kernel directory AFTER a
     directory was created does not. The driver maps at boot, before the
     first `exec()`, so every process directory gets it.
   - `process_fork()` walked every present PDE ≥ 2 as user memory: it would
     have tried `pmm_page_ref()` on MMIO frames and failed every fork. It now
     skips PDEs without `VMM_USER`. `vmm_destroy_directory()` already skipped
     them (Phase 23). `vmm_map_user_page_flags()` now refuses a virt whose
     PDE is present without `VMM_USER`, instead of writing a user PTE into a
     shared kernel table.
c) **PMM:** `pmm_alloc_page()` returns frames below 8 MB (`PMM_LIMIT_ADDR`),
   all identity-mapped, so virt == phys and the frame address goes straight
   into a DMA descriptor (32-bit, high half 0).
d) **Boot order (`kernel/main.c`):** `sti` + PIT run from early boot, so
   `timer_get_ticks()` advances during init. The driver goes right after
   `pci_scan_bus()`/`pci_print_list()` and before the ramfs/"Loading shell".
e) **`tools/Makefile`:** `run` = `qemu-system-x86_64` + `QEMU_FLAGS_RUN`
   (cdrom, IDE disk, 256 MB, serial stdio, sdl, -no-shutdown, -no-reboot), no
   network option at all, so QEMU's `pc` machine adds its default NIC: an
   e1000 at 00:03.0 on a user-mode (slirp) backend. `make run` therefore
   already has the device and, normally, an ARP reply. `run-net` gives the
   same device explicitly (a `-netdev` option turns the default NIC off) plus
   a pcap dump.

**24-A RX review (QEMU showed TX working, the ARP reply in the pcap, but the
boot self-test saw no reply).** Checked against `kernel/drivers/e1000.c`
(`setup_rings()`, `e1000_poll_rx()`, `read_mac()`, `e1000_init()`):
a) all 16 descriptors are filled with their buffer address, then RDBAL/
   RDBAH/RDLEN/RDH=0, then RDT=15, then RCTL with EN: correct.
b) the ring is read through `desc()`, a `volatile uint32_t *`, on every
   poll: correct. Added a compiler barrier after the DD check anyway.
c) RAL0/RAH0 hold the MAC with AV set (from the card, or written back from
   the EEPROM), MTA zeroed, RCTL = EN|BAM|SECRC, BSIZE 2048, BSEX 0: correct.
d) `g_rx_next` starts at 0 (= RDH), advances mod 16, status cleared and
   RDT set to the consumed index: correct.
e) buffer addresses are PMM frames (< 8 MB, virt == phys), high half 0:
   correct.
Nothing wrong found in the RX path itself. Two suspects in the self-test
wait instead: (1) its loop also stopped after 10M spins of RAM-only polls,
which can be far less than its 100-tick budget; (2) QEMU's e1000 model
(from memory of hw/net/e1000.c, `flush_queue_timer`, not checked against the
QEMU source here) refuses to receive for ~1 s after every RCTL write and
queues frames meanwhile, so a reply to a request sent right after init only
arrives after that. The wait is now 300 ticks with a PIT-counted delay per
empty poll, and a `[NET] diag:` serial line (RX registers, descriptor 0,
GPRC/MPC/RNBC) prints when no reply comes. Pending QEMU validation.

### Closed phases (one line each; detail in CHANGELOG.md / README.md)

- Phases 1–14 — see the README "Completed phases" table (Phase 14 closed
  as `0.14.0`; `0.14.1`/`0.14.2` were PATCH-only docs/test-tool work).
- Phase 15 — FAT16 subdirectories, `cwd_cluster` — `0.15.0`
  (`0.15.1` PATCH: libnos syscall wrapper library).
- Phase 16 — inter-process pipes, real blocking `waitpid` — `0.16.0`.
- Phase 17 — Cleanup A (audit fixes, technical debt, libnos/shell tools +
  reboot/shutdown, test/build infrastructure) — `0.17.0` (`0.17.1` PATCH: docs).
- Phase 18 — Safety/portability foundation: HAL, `msg(ID)`, PMM on the real
  memory map, Safe Mode (counter, TUI, restricted shell, previous-release GRUB
  entry) — `0.18.0`.
- Phase 19 — SDK / app-development experience: `exec()` from FAT16 (via
  `vfs_open()`), size-checked ELF loader, `printf` family in libnos, `sdk/` +
  `docs/sdk.md`, `make test-elf` — `0.19.0`.
- Phase 20 — Crash handler leads into Safe Mode: an unhandled exception saves a
  record (polling-only ATA I/O), resets, and Safe Mode shows the crash; `crash
  <de|pf|gpf>` test command — `0.20.0` (`0.20.1` PATCH: the `debug` boot
  argument works). The roadmap was renumbered (old 20–30 are now 21–31).
- Phase 21 — Copy-on-write `fork()`: `VMM_COW`, a per-page refcount in the PMM,
  `vmm_cow_break()`, `CR0.WP`, `SYS_PAGEREF`; `process_exit()` releases data
  pages (ROADMAP 23-A and the data-page half of 23-C) — `0.21.0`.
- Phase 22 — `unlink()`/`rmdir()`: `fat16_unlink()`/`fat16_rmdir()` (dirent
  `0xE5` first, then the FAT chain freed), `SYS_UNLINK`/`SYS_RMDIR` (35/36),
  `nos_unlink()`/`nos_rmdir()`, `sys_kill()` now closes fds too; selftest
  expanded to 31 tests — `0.22.0` (`0.22.1` PATCH: VGA console race fixed,
  selftest test 32).
- Phase 23 — Process memory release (23-B): `vmm_destroy_directory()`,
  `process_exit()` frees the PD and page tables, selftest expanded to 34
  tests — `0.23.0`. 23-A/23-C had landed in Phase 21.

### Next: Phase 24 — `e1000` driver + minimal TCP/IP

See ROADMAP.md. Release routine after tagging: `make clean && make &&
make snapshot` on the tagged tree, commit `tools/prev/`, and publish the
GitHub Release with the zip (see the Definition of Done in CLAUDE.md).
Deferred, not blocking: test `docs/setup.md` on Windows (Phase 30). The
heap/identity-map redesign below has NO phase scheduled and must happen
before the DOOM port (Phase 31).

### Future roadmap

See `ROADMAP.md` for the full per-phase breakdown and priority order
(Phases 24–31, v1.0.0 closes right after Phase 31, the DOOM port). A
package manager phase was deliberately decided against — don't add one.

## Architecture decisions (non-obvious; detail lives in the linked docs)

- **Every process directory copies all kernel PDEs present at creation
  (24-A), and kernel PDEs never have `VMM_USER`.** That is how a boot-time
  kernel mapping above 8 MB (the e1000 MMIO window) reaches every process.
  Code walking a process directory must skip non-`VMM_USER` PDEs
  (`process_fork()`, `vmm_destroy_directory()`), and kernel mappings above
  8 MB must be made before the first `exec()`. See `docs/memory.md`,
  `docs/network.md`.
- **`vga_putchar()`/`vga_clear()`/`vga_set_cursor()` run with interrupts off
  for their whole body (0.22.1).** Any process can reach them through
  `SYS_WRITE`/`SYS_CLEAR`/`SYS_GOTOXY` (echo included, since `SYS_READ`'s
  fd 0 echo goes through `vga_putchar()` too), and the scheduler preempts on
  a timer tick regardless of what instruction is running. A preempted write
  left `term_col`/`term_row`/the VGA buffer half-updated for whoever ran
  next. Found as real screen corruption while validating Phase 22 (typing
  at the shell while `run selftest` printed): a torn `vga_scroll()` could
  duplicate or drop a line, which looked like a command running again by
  itself. `irq_save()`/`irq_restore()` are safe here for the whole body
  (unlike `fat16.c`'s disk I/O) because nothing in `vga.c` blocks on an IRQ.
  This does not stop two processes' output interleaving character-by-
  character when both print at once; that's normal shared-terminal
  behavior. See `docs/hal.md`, `docs/testing.md` (test 32 and its manual
  companion).
- **`process_exit()` frees the whole address space immediately; no zombie,
  no reaper (23-B).** `vmm_destroy_directory()` loads the kernel directory
  first if the dying process is running on the one being freed (same 0–8 MB
  identity map, so its kernel code and static per-slot kernel stack stay
  mapped), then frees user page references, PTs with `VMM_USER` at PDE ≥ 2,
  and the PD, all in one interrupt-off section with `PROCESS_UNUSED`. Never
  free a PDE 0/1 table or one without `VMM_USER` (kernel). `sys_kill()` of
  one's own pid must not return to user mode. `pmm_free_page()` of a page
  that isn't allocated reports on serial. See `docs/memory.md`.
- **exec() is NOT POSIX exec — it spawns a brand-new `process_t`.** Any
  per-process state (cwd, redirects, ...) must be threaded explicitly
  through `syscall → exec()/fork() → scheduler_spawn_user →
  process_spawn_user`; only `process_fork()` copies fields, each one
  explicitly (e.g. `cwd_cluster`). Hence `SYS_EXEC_PIPE(name, stdin_fd,
  stdout_fd)` passes redirects down `exec()`'s chain instead of
  fork+dup2, and `process_spawn_user(start_blocked)` keeps the new process
  unschedulable until `sys_exec_pipe()` seeds its `fd_table`
  (`process_make_ready()`). See `docs/pipes.md`, `docs/scheduler.md`.
- **Pipes are a fixed static pool** (`kernel/pipe.c`, `PIPE_MAX=8`, 512 B),
  never `kmalloc()`'d, because `process_exit()` doesn't free memory (leak
  below). One waiter per direction (precedent: ATA `g_irq_waiter`).
  `vfs_dup()` bumps pipe refcounts on fork/exec duplication. See
  `docs/pipes.md`.
- **`SYS_WAIT` blocks for real** via `process_t.waiting_for_pid` +
  `PROCESS_BLOCKED`, woken by `process_exit()`; interface unchanged. See
  `docs/scheduler.md`.
- **`PROCESS_BLOCKED` ≠ `PROCESS_SLEEPING`.** Sleeping is timer-woken;
  blocked is only ever woken by the owning IRQ/event, never the timer.
- **Check-then-block is atomic under `cli`/`sti`** (ATA IRQ wait, ATA
  exclusion gate): check condition → register waiter + set
  `PROCESS_BLOCKED` in the same section, else a wakeup can be lost. See
  `docs/filesystem.md`.
- **ATA `g_irq_fired` is reset right after each `outb(REG_CMD, ...)`**
  (READ/WRITE/FLUSH), not after a wait completes — boot-time polling reads
  leave a stale flag that a later IRQ-driven wait would mistake for its
  own completion. Never move the reset. See `docs/filesystem.md`.
- **`ata_write_sector` returns 0 if only the CACHE FLUSH times out** — the
  WRITE was already confirmed, so it must not be reported as "nothing
  written". Don't "fix" it into a failure.
- **Copy-on-write `fork()`: the refcount counts mappings, and
  `pmm_free_page()` means "drop one reference".** Every allocation starts at
  1; never free a user page any other way. A write-protected PTE does NOT
  protect a page from the kernel, which writes user memory through physical
  addresses: every kernel write into user memory must go through
  `user_kptr_write()`/`copy_to_user()` (they call `vmm_cow_break()` first).
  `VMM_COW` (PTE bit 9) is the only thing that makes a read-only page
  copy-on-write. fork's sharing pass, the break, and exit's release are each
  one interrupt-off section; `process_exit()` clears `cr3` and marks the slot
  unused in the same section (a tick in between would switch to CR3 = 0).
- **`fork()` resumes the child via `isr128_resume` + `g_syscall_frame`**
  (copies the 13-word trap block, `eax` forced to 0). See
  `docs/scheduler.md`.
- **FAT16 has one shared lookup/insert/path-walk core** (`dir_iter_t`,
  `dir_lookup`, `dir_insert`, `resolve_path`) because of the Phase 10
  duplicated-lookup bug. `to_8_3()` gotchas: special-case `"."`/`".."`, and
  find the real `.` in the ORIGINAL string before the 8-char truncation.
  Any new FAT16 test filename must have a distinct 8.3 encoding (check the
  truncation rule, not by eye). See `docs/filesystem.md`, `docs/testing.md`.
- **`vfs_fd_t` caches `parent_cluster` + final path component** at
  open/create, so a later write isn't affected by an intervening `cd`.
- **`vfs_write` = stream write (`fd->pos`); `vfs_write_all` = whole-file
  replace** (`SYS_WRITE_FILE`). `fat16_write_at` re-looks-up the dirent
  every call, so stale cached `fd->first`/`fd->size` is harmless.
- **Keyboard Shift:** `keyboard.c` tracks Shift and uses an index-matched
  `scancode_map_shift`. The raw path (`SYS_READ_RAW`, `edit.c` only) packs
  a Shift bit (bit 9, next to Ctrl's bit 8) because the kernel swallows
  Shift scancodes; `edit.c` keeps its own deliberate copy of the shifted
  table. See `docs/kernel.md`.
- **Userland pointers are validated in `kernel/syscall.c`**
  (`user_ptr_valid`/`copy_from_user`/`copy_to_user`/`user_kptr`), built on
  `vmm_get_user_phys_from_dir()`, which requires `VMM_USER` on PDE and PTE
  (the shared kernel identity map is present but never USER). Every
  syscall touching a user address must use them. See `docs/security.md`.
- **libnos (`user/lib/booleos.c/h`, `nos_*`)** is the single syscall wrapper
  layer for all user programs (`0.15.1`), so changing a syscall's internals
  means recompiling one file. See `docs/kernel.md`.
- **`msg(ID)`: fragments, not format strings; only OUTPUT text** (never
  strcmp keys / exec names / file names); kernel and userland get separate
  tables (user programs can't call the kernel). See `docs/hal.md`.
- **The previous release is kept as files in the repo (`tools/prev/`: kernel +
  ramfs together, ABI must match) and refreshed by hand with `make snapshot`
  after a release tag** — never automatically. See `docs/safemode.md`.
- **Safe Mode config lives in raw sector LBA 1 (FAT16 reserved region), not a
  file**, so it works when FAT16/VFS/heap are broken; unavailable (defaults,
  no writes) if the boot sector's `reserved_sectors` < 2. See
  `docs/safemode.md`.
- **PMM manages only 0–8 MB (`PMM_LIMIT_ADDR`)** because the kernel touches
  frames by physical address and only 0–8 MB is identity-mapped; a mitigation,
  not the fix (no phase scheduled). See `docs/memory.md`.
- **HAL (`kernel/hal.h`) is a forwarding layer, not a rewrite**: the
  interface is arch-neutral, `hal.c` just calls the existing drivers; the
  exception handler (`idt.c`) and driver bring-up deliberately bypass it.
  See `docs/hal.md`.
- **`kernel/version.h` is macros-only** so userland may include it; it is
  the single version source. Any shared kernel/user header needs the same
  "macros only" property.
- **`unlink`/`rmdir` write the `0xE5` dirent before freeing the FAT chain**
  (a crash loses space, never leaves a live entry on free clusters), refuse a
  file open in any process and a directory that is any process's cwd (via a
  `fat16_busy_fn` callback from `syscall.c`, so `fat16.c` stays process-free),
  and are not recursive. See `docs/filesystem.md`.
- **`SYS_PCI_LIST` returns the device count** (for `selftest`); `lspci`
  ignores it. See `docs/testing.md`.

## Known technical debt

- **The heap is virt == phys inside the process page pool (no phase scheduled; prerequisite of the DOOM port, Phase 31).** The
  kernel heap (4–8 MB virtual) is the identity-mapped range that the PMM also
  hands to processes; `heap_expand()` takes the exact physical page at
  `heap_end` and `heap_init()` pre-grows to 256 KB, but the heap cannot grow
  after processes exist. An `exec()` of a program bigger than what is free in
  the heap fails ("out of memory to load the program file"). Found when the
  first `exec()` from FAT16 grew the heap with the lowest free page and
  repointed the identity view of a process's page directory.
- **The kernel writes to physical pages through the 0–8 MB identity map
  without checking (pre-existing, real; no phase scheduled).** The PMM can hand out
  frames above 8 MB while only 0–8 MB is identity-mapped, yet `elf.c:54`
  (`memzero8((uint8_t *)phys, ...)`) and `vmm_cow_break()` (the copy-on-write
  copy via `old_phys`/`new_phys`) access a frame by its physical
  address with no range check; only the page-table allocations are guarded
  (`vmm.c:126`, `:146`). Once the low region is used up that is a kernel page
  fault. **Mitigation (18-A):** the PMM ceiling is 8 MB (`PMM_LIMIT_ADDR`), so
  exhaustion is now a failed allocation, at the cost of ~4 MB of free pages
  (1024 at boot; since 23-B `process_exit()` leaks nothing). Real fix: stop touching frames by physical
  address (a temporary-mapping mechanism, or a kernel direct map at a high
  address) and then lift the cap. The widening is not trivial: user code lives
  at 16 MB and `vmm_map_user_page()` rejects `virt < 0x800000`.
- **`edit` with no file name cannot save.** `user/edit.c` only calls
  `load_file()` when a name was given, so `file_fd` stays -1 and Ctrl+S takes
  its `else` branch, reporting the misleading "saved (no disk)" — a message
  that covers two cases ("no disk", "no file name"). Expected: ask for a name
  (save as). Pre-existing since the editor got file saving.
- **Safe Mode gaps:** no erase action (`fat16_unlink()` exists since Phase 22,
  the menu action doesn't), no fsck-like
  verify/repair submenu (its own future sub-phase), no GUI-debug/Text-mode
  entries (Phase 27). `kmain` keeps its own copy of the module/boot-info PMM
  reservations that could use `boot_get_module()`/`boot_get_info_region()`.
- **The selftest's Intel 440FX check (`8086:1237`) breaks by design in Phase 25**
  (QEMU `-machine q35`): update the IDs then (noted in ROADMAP Phase 25).

- **ATA `probe()` intermittently reports `no disk`** (a temporary serial trace,
  `TEMP-DEBUG(ata-probe)`, is left in `ata.c` to catch it; see `docs/TODO.md`)
  — (first seen in 17-B,
  before any power.c change). 9 boot/reboot cycles with tracing in 17-C did
  not reproduce it and showed no evidence that `reboot` causes or worsens
  it; every traced probe took the success path (status 0x50 after select,
  0x58 after IDENTIFY). Pre-existing; not chased further. 

- **`exec_arg` (`kernel/syscall.c`) is one global shared by all
  processes**; `SYS_GETARG` can read an arg clobbered by another exec.
  `sys_exec_pipe()` clears it (17-C). Needs per-process argument storage;
  `SYS_EXEC_PIPE` also has no argument register left. 
- **No exit-code syscall:** `SYS_EXIT` ignores its code and `SYS_WAIT`
  returns nothing but 0 (the selftest passes child results through pipes).
- **`dir_buf`/`sector_buf` in `fat16.c` are global buffers held across
  blocking ATA writes** — a concurrent FAT16 call from another process can
  clobber them. `unlink`/`rmdir` share the gap: two processes rewriting one
  directory sector can lose an entry. Two concurrent selftests fail cleanup
  (30/31), but fixed shared names + the cwd refusal explain that too, so it is
  not proof of the race (`docs/filesystem.md`). Fix = whole-operation FAT16 lock, Phase 29
  (`docs/filesystem.md`).
- **Shell redirection limits:** builtins can't be redirected; `>`/`<`
  can't combine with `|` and pass no arguments (`docs/shell.md`).
- **`SYS_WRITE` chunks at 128 bytes**, each chunk doing its own dirent
  lookup (slow for large redirected output).
- **No shell command for `unlink`/`rmdir`** (no `rm`/`rmdir` in `shell.c`);
  only programs calling `nos_unlink()`/`nos_rmdir()` can delete.
- **No syscall exposes `kmalloc()` to userland**, so userland can't test a
  real heap allocation (`SYS_MEMINFO` only reads counters).

## Maintenance rule for this file

If this file grows past roughly 200–300 lines, consolidate before adding
more: remove resolved debt (git log is the history), one line per closed
phase, and move design rationale into `docs/`.
