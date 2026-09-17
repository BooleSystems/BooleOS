# PROGRESS.md — working memory for future sessions

This file is **not** end-user documentation — that's what `README.md` is for.
This file exists because each Claude Code session starts with zero memory of
prior sessions and only sees `CLAUDE.md`, the code, and `git log`. Its job is
to carry forward the "why" behind non-obvious code, and the list of known
technical debt, so it doesn't get silently rediscovered (or re-broken) every
session.

Do not duplicate README content here (phase descriptions, syscall tables,
build instructions, file structure). Link to the README section instead.

## Current status

Last completed phase: **Phase 14** — kernel memory-safety hardening:
`user_ptr_valid()`/`copy_from_user()`/`copy_to_user()` at the syscall
boundary, fixing 4 confirmed ring 3 → ring 0 memory read/write bugs
(`sys_write`, `sys_read`, `sys_write_file`, `sys_meminfo`) plus a
`kmalloc()` integer-overflow bug; extended to close the same gap in
`sys_open`/`sys_create`/`sys_exec`/`sys_getarg` via `user_kptr()`; also
removed leftover debug output in `sys_open` and centralized the
version string in `kernel/version.h`.
See `README.md` → Roadmap table + "What's implemented" (Usermode and
syscalls) + CHANGELOG.md `[0.14.0]` for full detail.

Future roadmap: see `README.md` → "Future roadmap — detailed planning
(Phases 15–21)" for the full per-phase breakdown and priority order.

## Architecture decisions (non-obvious from reading the code alone)

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

## Known technical debt

- **Duplicated dirent lookup: `fat16_find` vs. `fat16_write_file`**
  (`kernel/fs/fat16.c`). `fat16_write_file` has its own inline copy of the
  8.3 name-matching loop instead of calling `fat16_find`. This already bit
  the project once: an out-of-bounds `name[8]`/`ext[3]` bug was fixed in
  `fat16_find` but left in place in the `fat16_write_file` copy, since
  they're separate code paths. Both currently build an explicit 11-byte
  buffer correctly (see the comment at `fat16.c:319`), but the duplication
  itself is still there — Phase 17 (FAT16 subdirectories) is flagged in
  the README roadmap as a good point to unify this into one function
  before extending it further.

- **`process_exit()` never frees `process->cr3` or its mapped pages**
  (`kernel/process.c`, comment above `process_exit`). A process's entire
  address space (and, for a forked child, its independent copy of every
  page) is abandoned, not reclaimed, on exit. Documented as an accepted
  leak: slots stay safely reusable because `process_spawn()`/
  `process_fork()` always allocate a fresh `cr3` for whatever runs next in
  that slot, but physical memory is never returned to the PMM. Relevant
  to Phase 16 (copy-on-write fork), which will need real refcounting
  before this can be fixed properly.

- **`process_spawn()`/`process_spawn_user()` scan for a free slot without
  `cli`/`sti` protection** (`kernel/process.c`, comment above
  `process_spawn`). Explicitly flagged as out-of-scope when `fork()`
  (Phase 13) was implemented — `process_fork()` itself does the
  equivalent slot search race-safely, but the two original spawn paths
  still don't. Two processes spawning concurrently (e.g. from two
  different IRQ-resumed contexts) could theoretically race on the same
  `PROCESS_UNUSED` slot.

## Maintenance rule for this file

If this file grows past roughly 200–300 lines, the next session that
notices should consolidate it before adding more: resolved tech debt
items get removed entirely (not kept as history — `git log` is the
history), and old-but-still-relevant architecture decisions get
tightened to their essential point rather than left at full length.
