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

Current version: **0.18.0-nightly** (last release: `v0.17.1` on `main`).
Last closed phase: **Phase 17** (Cleanup A).

### Closed phases (one line each; detail in CHANGELOG.md / README.md)

- Phases 1–14 — see the README "Completed phases" table (Phase 14 closed
  as `0.14.0`; `0.14.1`/`0.14.2` were PATCH-only docs/test-tool work).
- Phase 15 — FAT16 subdirectories, `cwd_cluster` — `0.15.0`
  (`0.15.1` PATCH: libnos syscall wrapper library).
- Phase 16 — inter-process pipes, real blocking `waitpid` — `0.16.0`.
- Phase 17 — Cleanup A (audit fixes, technical debt, libnos/shell tools +
  reboot/shutdown, test/build infrastructure) — `0.17.0`.

### Current work: Phase 18 code complete (18-A + 18-B); closing checklist pending

18-A (HAL, `msg(ID)`, PMM on the real memory map) is done and committed. 18-B
(Safe Mode, `docs/safemode.md`) is done in 5 passes: 1 config sector + cmdline
flags, 2 failure counter + entry + stub, 3 tier-1 TUI, 4 tier-2 restricted
shell (committed through `b02d25b`), 5 **done, uncommitted, awaiting the QEMU
check:** "previous release" GRUB entry, `tools/prev/` (tracked), `make snapshot`.

**Before releasing 0.18.0:** (a) replace `tools/prev/` with a build of the
v0.17.1 tag (today it holds the 0.18.0-nightly build, taken as the first
snapshot); (b) the Phase 18 closing checklist (version.h 0.18.0 / phase 18,
README table + banner, CHANGELOG `[0.18.0]` single entry, docs, syscall grep,
merge to `main`, tag); (c) then, on the tag, `make snapshot` and commit
`tools/prev/` for the next release.

**Real pre-existing debt (Phase 22):** the kernel accesses physical pages
through the 0–8 MB identity map (elf.c:54, process.c:262-265); the 8 MB PMM cap
is only a mitigation. See `docs/TODO.md`. `process_exit()` never frees, so ~10
pages leak per process (1024 free at boot).

Version is `0.18.0-nightly`; `NULLOS_PHASE`/`DESC` stay 17 / "Cleanup A" until
Phase 18 closes. Deferred, not blocking: test `docs/setup.md` on Windows
(Phase 29).

### Future roadmap

See `ROADMAP.md` for the full per-phase breakdown and priority order
(Phases 17–30, v1.0.0 closes right after Phase 30, the DOOM port). A
package manager phase was deliberately decided against — don't add one.

## Architecture decisions (non-obvious; detail lives in the linked docs)

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
  written". Don't "fix" it into a failure. (Doc stub in `docs/TODO.md`.)
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
- **libnos (`user/lib/nullos.c/h`, `nos_*`)** is the single syscall wrapper
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
  not the fix (Phase 22). See `docs/memory.md`.
- **HAL (`kernel/hal.h`) is a forwarding layer, not a rewrite**: the
  interface is arch-neutral, `hal.c` just calls the existing drivers; the
  exception handler (`idt.c`) and driver bring-up deliberately bypass it.
  See `docs/hal.md`.
- **`kernel/version.h` is macros-only** so userland may include it; it is
  the single version source. Any shared kernel/user header needs the same
  "macros only" property.
- **`SYS_PCI_LIST` returns the device count** (for `selftest`); `lspci`
  ignores it. See `docs/testing.md`.

## Known technical debt

- **ATA `probe()` intermittently reports `no disk`** (first seen in 17-B,
  before any power.c change). 9 boot/reboot cycles with tracing in 17-C did
  not reproduce it and showed no evidence that `reboot` causes or worsens
  it; every traced probe took the success path (status 0x50 after select,
  0x58 after IDENTIFY). Pre-existing; not chased further. (`docs/TODO.md`)

- **`exec_arg` (`kernel/syscall.c`) is one global shared by all
  processes**; `SYS_GETARG` can read an arg clobbered by another exec.
  `sys_exec_pipe()` clears it (17-C). Needs per-process argument storage;
  `SYS_EXEC_PIPE` also has no argument register left. (`docs/TODO.md`)
- **`pt_next` in `kernel/memory/vmm.c` starts at `PAGE_TABLE_START`, but
  `vmm_init()` writes two page tables there without advancing it.** The
  first `map_page_early()` creating a NEW page table would overwrite the
  0–4 MB identity map. Harmless today (heap 4–8 MB is inside the present
  PDE 1). Fix: init `pt_next` to `PAGE_TABLE_START + 2 * PAGE_SIZE`.
- **No exit-code syscall:** `SYS_EXIT` ignores its code and `SYS_WAIT`
  returns nothing but 0 (the selftest passes child results through pipes).
- **`dir_buf`/`sector_buf` in `fat16.c` are global buffers held across
  blocking ATA writes** — a concurrent FAT16 call from another process can
  clobber them. Fix = whole-operation FAT16 lock, Phase 28
  (`docs/filesystem.md`).
- **Shell redirection limits:** builtins can't be redirected; `>`/`<`
  can't combine with `|` and pass no arguments (`docs/shell.md`).
- **`SYS_WRITE` chunks at 128 bytes**, each chunk doing its own dirent
  lookup (slow for large redirected output).
- **`process_exit()` never frees `cr3` or mapped pages** (accepted leak;
  slots stay reusable). Needs refcounting from Phase 20 (COW fork); the
  actual fix is Phase 22.
- **No unlink/delete syscall or FAT16 delete path** (cluster-chain free +
  0xE5 dirent). Test files can be overwritten, never removed.
- **No syscall exposes `kmalloc()` to userland**, so userland can't test a
  real heap allocation (`SYS_MEMINFO` only reads counters).

## Maintenance rule for this file

If this file grows past roughly 200–300 lines, consolidate before adding
more: remove resolved debt (git log is the history), one line per closed
phase, and move design rationale into `docs/`.
