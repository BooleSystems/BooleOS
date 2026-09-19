# PROGRESS.md — working memory for future sessions

This file is **not** end-user documentation — that's what `README.md` is for.
This file exists because each Claude Code session starts with zero memory of
prior sessions and only sees `CLAUDE.md`, the code, and `git log`. Its job is
to carry forward the "why" behind non-obvious code, and the list of known
technical debt, so it doesn't get silently rediscovered (or re-broken) every
session.

Do not duplicate README/docs content here (phase descriptions, syscall
tables, build instructions, file structure). Link to the relevant
README/docs/ section instead. Documentation is split as follows:
`README.md` is a lean index (completed-phases table, links, build
instructions); `ROADMAP.md` holds future/planned phases; `docs/<system>.md`
holds detailed per-system technical content (see `README.md` →
"Documentation" for the current list of docs/ files and what each covers).
When documenting a new system in detail, add/extend a `docs/<system>.md`
file and link it from `README.md` — don't put system detail back into
`README.md` itself.

## Current status

Last completed phase: **Phase 15** — FAT16 subdirectories: `mkdir`/`cd`,
path-aware `touch`/`edit`/`ls`; a shared `dir_lookup()`/`dir_insert()`/
`resolve_path()` core (resolves the Phase 10 duplicated-lookup tech
debt, previously tracked below); `SYS_CHDIR`/`SYS_MKDIR`; and a fix,
found during this phase's own manual QEMU testing, for `exec()`
(`run`/`edit`) not inheriting the caller's `cwd_cluster` (it always
started fresh processes at the root, unlike `fork()`, which already
had cwd inheritance). Current version: **0.15.0** (MINOR bump — Phase
15 completed). See `README.md` → "Completed phases" table +
`docs/filesystem.md` → "Subdirectories" + `docs/scheduler.md` +
`docs/syscalls.md` + CHANGELOG.md `[0.15.0]` for full detail.

Note: FAT16 subdirectories was originally planned and numbered "Phase
17" in `ROADMAP.md`, but was implemented ahead of the two
process-related phases (pipes, copy-on-write fork) that preceded it in
that list — it took the next real completed-phase slot, 15, and the
roadmap was renumbered accordingly (pipes/COW-fork are now Phases
16/17; everything from the networking phase onward kept its number).
See ROADMAP.md's own note on this and CHANGELOG.md `[0.15.0]`.

Prior to this, Phases 14.1/14.2 were PATCH-only intermediate work
(documentation reorganization, `user/selftest.c` + `pci_device_count()`,
a `make debug` target) — see CHANGELOG.md `[0.14.1]`/`[0.14.2]` and
`docs/testing.md`.

Future roadmap: see `ROADMAP.md` for the full per-phase breakdown and
priority order (Phases 16–22).

## Architecture decisions (non-obvious from reading the code alone)

- **User programs go through a shared syscall wrapper library
  (`user/lib/nullos.c/h`, "libnos", `nos_*`) instead of each writing
  its own `int $0x80` inline asm** — added post-Phase-15, not a
  numbered phase itself (infrastructure/compatibility work, tracked in
  CHANGELOG.md `[Unreleased]`). Motivation: before v1.0.0 the syscall
  *interface* stays free to change, but until this library existed,
  changing what a syscall does *underneath* an unchanged interface
  still meant touching every one of the six user programs that called
  it by hand. Now it means recompiling `lib/nullos.c` once (`user/
  Makefile` links every program against the same `lib/nullos.o`). See
  `docs/kernel.md` → "User-space syscall library (libnos)" for the
  full design, including two real (not purely renamed) differences
  found while unifying six independent copies of these wrappers:
  `nos_write`/`nos_read` gained an explicit `fd` argument (two of the
  four affected programs already used that fuller form; the others
  hardcoded `fd=1`/`fd=0` inline), and `nos_exec(name, arg)` replaced
  `shell.c`'s `sys_exec`/`sys_exec_arg` split — the latter's single-arg
  form never constrained `ecx` in its inline asm, so the kernel's
  `sys_exec` read whatever garbage was in `ecx` as the argument
  pointer (harmless in practice, since an invalid address just makes
  `copy_user_str` fail silently, but not a wrapper bug worth
  preserving).
- **FAT16 subdirectories (`kernel/fs/fat16.c`) go through one shared
  lookup/insert/path-walk core, deliberately, because of the Phase 10
  duplicated-lookup bug** (previously tracked in "Known technical debt"
  below, now resolved): `dir_iter_t`/`dir_iter_next_sector()` is the one
  place that steps through a directory's sectors (fixed root region or a
  subdirectory's FAT chain), `dir_lookup()` the one place that builds the
  combined 11-byte name and compares it, `dir_insert()` the one place
  that writes a new dirent. `fat16_find`/`fat16_create`/`fat16_mkdir`/
  `fat16_readdir`/`fat16_resolve_dir`/`fat16_write_file` are all thin
  callers of these, plus `resolve_path()` for anything that takes a
  `"/"`-separated path. See `docs/filesystem.md` → "Subdirectories" for
  the full design (including why the root and a normal subdirectory are
  structurally different, and how that difference is contained inside
  `dir_iter_next_sector()` instead of leaking into every caller).
  **Gotcha already fixed once during this phase, worth knowing if
  `to_8_3()` is ever touched again:** `to_8_3(".")`/`to_8_3("..")` must
  NOT go through the normal name/extension split (which treats the
  first `.` as the extension separator and silently produces the wrong
  bytes for both) — `to_8_3()` special-cases exactly `"."` and `".."`
  up front for this reason. Without that special case, `resolve_path()`
  can never match the `.`/`..` entries `fat16_mkdir` writes, silently
  breaking `cd .` and `cd ..` everywhere, not just at the root.
  **Second gotcha, also fixed during this phase:** the base/extension
  split must locate the real `.` in the ORIGINAL string before
  deciding there isn't one — stopping the base-name scan at the 8-char
  cap and checking `*name == '.'` right there (the original,
  Phase-10-era logic) means any base longer than 8 chars whose `.`
  comes later (e.g. `"selftest_tmp.txt"`) never finds its extension at
  all. Two names that only differ after the 8th character then
  silently collide on the same on-disk 8.3 entry. This isn't just
  theoretical — it's exactly what made `user/selftest.c`'s own test
  filenames (`selftest_tmp.txt` / `selftest_sub.txt` /
  `selftest_fork_marker.txt`, all sharing the `"selftest"` prefix and
  `"txt"` extension) collide with each other; see `docs/testing.md`
  and CHANGELOG.md `[0.15.0]`. Any new FAT16 test/tool filename must
  have a distinct 8.3 encoding from every other one in use, not just a
  visually distinct name — check with the actual truncation rule, not
  by eye.
- **ATA's `g_irq_fired` (`kernel/drivers/ata.c`, Phase 12) is reset
  when a NEW command is issued, not just when a wait for one
  finishes** — found via a real intermittent bug during this phase's
  own testing (a second `ata_read_sector()`/`ata_write_sector()` call,
  right after a successful first one, could skip its own IRQ wait and
  fail). Root cause: `fat16_init()`'s ~65 boot-time reads run through
  the polling fallback (`process_current()` is `NULL` that early), but
  the drive still raises a real completion IRQ for every one of them
  — since the polling path never calls `ata_wait_irq()`, that real IRQ
  sets `g_irq_fired = 1` with nobody to consume it, and it can sit
  there stale until a *later*, genuinely IRQ-driven wait mistakes it
  for its own command's completion and returns without actually
  waiting. If this file is ever refactored, the reset MUST stay at
  "right after `outb(REG_CMD, ...)`" for every command (`CMD_READ`,
  `CMD_WRITE`, `CMD_FLUSH`) — moving it back to only "after a
  successful wait" reopens this exact race.
- **`vfs_fd_t` caches `parent_cluster` + the file's final path component
  at open/create time, not the full path** (`kernel/fs/vfs.h/.c`). A
  later `vfs_write()` re-finds the entry directly from that cached
  location instead of re-resolving the original path against the
  process's *current* `cwd_cluster` — otherwise a `cd` between opening a
  file and writing to it would silently target the wrong directory (or
  fail) depending on timing. This was designed in deliberately, not
  discovered as a bug — see the review discussion that approved Phase 15
  before implementation.
- **`process_t.cwd_cluster` is copied in `process_fork()`**
  (`kernel/process.c`), same as every other scalar process field — a
  `cd` a shell process did before forking is expected to still apply to
  its child, exactly like an inherited environment variable would be in
  a POSIX shell.

- **`PROCESS_BLOCKED` is a separate state from `PROCESS_SLEEPING`**
  (`kernel/process.h`). `PROCESS_SLEEPING` is driven by `wake_tick` and
  is only cleared by `process_wake_sleepers()` on a timer tick
  (`kernel/scheduler.c` → `scheduler_tick`). If a process blocked on an
  external event (a disk IRQ, the ATA exclusion gate) had reused
  `PROCESS_SLEEPING`, a coincidental timer-driven wake check could mark it
  `READY` before the event it's actually waiting on has happened.
  `PROCESS_BLOCKED` is only ever flipped back to `READY` by the specific
  IRQ handler or event owner that's responsible for it — never by the timer.

- **Check-then-block is done atomically under `cli`/`sti`**, in two places:
  the ATA IRQ wait (`kernel/drivers/ata.c`, around `ata_wait_irq`) and the
  ATA exclusion gate (`ata_gate_acquire`/`ata_gate_release`, same file).
  The pattern is: `cli` → check the condition (IRQ already fired? gate
  free?) → if not satisfied, register as the waiter and set
  `state = PROCESS_BLOCKED` → `sti` → `scheduler_block_current()`. Doing the
  check and the state transition inside the same `cli`/`sti` section is
  required: without it, an IRQ could fire (and try to wake a waiter that
  isn't registered yet) in the gap between the check and setting
  `PROCESS_BLOCKED`, permanently losing the wakeup and hanging the process.

- **`fork()` resumes the child via `isr128_resume` + `g_syscall_frame`**
  (`kernel/isr.asm`, `kernel/process.c` → `process_fork`). Right after
  `pusha` in the `int 0x80` handler, `esp` points at a 13-word block: the 8
  `pusha` registers followed by the CPU's ring3→ring0 trap frame (eip, cs,
  eflags, user_esp, user_ss) — everything needed to resume that exact
  ring-3 point via `popa; iret`. That `esp` is saved into `g_syscall_frame`
  (valid only synchronously, for the syscall being dispatched right now).
  `process_fork()` copies that whole block onto the child's own kernel
  stack with the `eax` slot forced to `0`, and points the child's saved
  return address at `isr128_resume` (skipping the normal bootstrap path —
  see `process.c`: `child->entry = 0`). The first time the scheduler
  switches to the child, it lands on `isr128_resume`, does `popa; iret`,
  and returns to ring 3 at the exact point the parent called `fork()` —
  just with `eax = 0` instead of the child's pid.

- **`ata_write_sector`'s cache-flush failure is deliberately not
  propagated as a write failure** (`kernel/drivers/ata.c`,
  `ata_write_sector`). The physical `WRITE SECTORS` command's completion
  is checked (and can return `-1`) *before* the `CACHE FLUSH` command is
  even issued. If the flush times out afterward, the function still
  returns `0`: a flush timeout only means the drive didn't confirm the
  cache was committed to media within the deadline, not that the
  already-confirmed write was lost. This was a past source of confusion
  (a flush timeout looking like "nothing was written") — recorded here so
  it isn't "fixed" back into a false failure by a future session.

- **Userland pointer validation is centralized in `kernel/syscall.c`,
  not `kernel/memory/vmm.c`** (`user_ptr_valid()`, `copy_from_user()`,
  `copy_to_user()`, added in Phase 14). Every syscall that reads or
  writes through a userland-supplied address MUST go through one of
  these before touching it — see the block comment above
  `user_ptr_valid()` for the full rationale. The key mechanism they're
  built on is `vmm_get_user_phys_from_dir()` (`kernel/memory/vmm.c`),
  which is deliberately stricter than the pre-existing
  `vmm_get_phys_from_dir()`: it also requires `VMM_USER` on both the
  PDE and PTE, not just "present". This distinction is the whole fix —
  every process's page directory clones the kernel's own PDE0/PDE1 (the
  identity-mapped first 8MB: kernel heap, page tables, ...), so that
  region is always "present" in every process, just never `VMM_USER`.
  A validator that only checked "present" (like the one first tried
  during this fix) would still treat that shared kernel region as a
  legitimate buffer.
  `user_kptr()` itself (the byte-resolution helper `copy_from_user()`/
  `copy_to_user()`/`copy_user_str()` are all built on) was initially
  left resolving through the looser `vmm_get_phys_from_dir()` — the
  same gap, just reachable through `sys_open`/`sys_create`/`sys_exec`/
  `sys_getarg`'s filename/argument strings instead of an
  arbitrary-length buffer. Closed in the same phase by switching
  `user_kptr()` to `vmm_get_user_phys_from_dir()` too, which fixed all
  four call sites at once with no change needed in any of them.

- **`kernel/version.h` is a plain-macro header shared across the
  kernel/userland boundary** (added end of Phase 14). It's the single
  source of truth for the version string — `kernel/main.c`'s boot
  banner, `user/shell.c`'s `fetch`/`uname`, and `tools/grub.cfg`
  (generated at build time from `tools/grub.cfg.in` via a Makefile
  rule) all read from it, so the version can no longer drift between
  them the way it already had twice. `user/shell.c` including a header
  that lives under `kernel/` looks like it violates the kernel/
  userland separation, but it's safe here specifically because
  version.h contains only string/text `#define`s — no kernel types,
  structs, or function declarations a userland translation unit
  shouldn't see. Any future shared-header candidate needs the same
  "macros only, nothing kernel-internal" property before it's safe to
  include from `user/`.

- **`SYS_PCI_LIST` now returns the device count instead of always `0`**
  (`kernel/syscall.c`, `kernel/drivers/pci.c/h` → `pci_device_count()`).
  Changed alongside adding `user/selftest.c` (see `docs/testing.md`),
  which needed a way to assert "PCI enumeration found ≥ 1 device"
  without parsing `pci_print_list()`'s VGA text output. `lspci` in the
  shell still just discards the return value, so this is additive —
  no existing caller's behavior changed.

## Known technical debt

- **`process_exit()` never frees `process->cr3` or its mapped pages**
  (`kernel/process.c`, comment above `process_exit`). A process's entire
  address space (and, for a forked child, its independent copy of every
  page) is abandoned, not reclaimed, on exit. Documented as an accepted
  leak: slots stay safely reusable because `process_spawn()`/
  `process_fork()` always allocate a fresh `cr3` for whatever runs next in
  that slot, but physical memory is never returned to the PMM. Relevant
  to Phase 17 (copy-on-write fork), which will need real refcounting
  before this can be fixed properly.

- **`process_spawn()`/`process_spawn_user()` scan for a free slot without
  `cli`/`sti` protection** (`kernel/process.c`, comment above
  `process_spawn`). Explicitly flagged as out-of-scope when `fork()`
  (Phase 13) was implemented — `process_fork()` itself does the
  equivalent slot search race-safely, but the two original spawn paths
  still don't. Two processes spawning concurrently (e.g. from two
  different IRQ-resumed contexts) could theoretically race on the same
  `PROCESS_UNUSED` slot.

- **No unlink/delete syscall exists yet** (surfaced by `user/selftest.c`,
  see `docs/testing.md`). Any file created for testing (or by a user)
  can be overwritten but never removed from FAT16. `fat16_write_file`/
  `fat16_create` would need a sibling that frees the cluster chain and
  marks the dirent deleted (0xE5) instead of just adding a new syscall
  number — the FAT16 side has no delete path at all right now.

- **No syscall exposes `kmalloc()` to userland** (surfaced by
  `user/selftest.c`'s memory test, see `docs/testing.md`). The closest
  available userland-visible memory operation is `SYS_MEMINFO`, which
  only reads `pmm_free_pages()`/`heap_free_bytes()` — it doesn't
  allocate anything itself. Not a bug, just means there's currently no
  way to test a real heap allocation from userland.

## Maintenance rule for this file

If this file grows past roughly 200–300 lines, the next session that
notices should consolidate it before adding more: resolved tech debt
items get removed entirely (not kept as history — `git log` is the
history), and old-but-still-relevant architecture decisions get
tightened to their essential point rather than left at full length.
