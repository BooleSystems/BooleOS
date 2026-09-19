# Processes, scheduler, and fork()

## Multitasking
- Process table with up to 16 entries
- Round-robin scheduler with preemption via IRQ0 (10-tick slice = 100ms at 100Hz)
- Context switch in assembly (saves/restores callee-saved registers via ESP)
- TSS configured for a per-process kernel stack (SS0:ESP0)
- Processes that never call yield are preempted by the timer automatically

## Program execution and foreground control
- `SYS_EXEC (10)`: receives a user virtual pointer to the program name; `sys_exec` copies the string byte by byte from user space via `vmm_get_phys_from_dir(cur->cr3, vaddr)` (identity-map), calls the kernel's `exec()`, and returns the new process's PID or -1
- The shell stores the returned PID in `foreground_pid`; typing another command resets `foreground_pid`
- **Ctrl+C**: IRQ1 detects scancode `0x1D` (Ctrl press/release) and `0x2E` (C); injects `0x03` into the ringbuffer; `SYS_READ` returns immediately with `buf[0]=0x03` and echoes `^C\n`; the shell calls `sys_kill(foreground_pid)` and resets the PID

## Current working directory (`cwd_cluster`)

- `process_t.cwd_cluster` (`process.h`): the FAT16 cluster of the process's current directory (0 = root). Every path-taking syscall (`SYS_OPEN`, `SYS_CREATE`, `SYS_READDIR`, `SYS_MKDIR`) resolves a relative path (one that doesn't start with `/`) against it — see `docs/filesystem.md` for the resolution itself.
- `SYS_CHDIR (26)` is the only thing that changes it, and only ever on confirmed success (`fat16_resolve_dir()` returning 1 — path exists and is a directory) — every failure path (doesn't exist, names a file, I/O error) returns early without touching it, so a failed `cd` never leaves the process half-moved.
- `process_fork()` copies `cwd_cluster` from parent to child alongside the other scalar fields (`user_esp`, etc.) — so `cd`'s effect on a process survives a subsequent `fork()`, the same as any other process state.
- **`exec()`/`SYS_EXEC` (`run`/`edit` in the shell) also inherit `cwd_cluster` from the calling process** — not just `fork()`. This was a real gap found and fixed during Phase 15's own manual testing: `exec()` spawns a **brand-new** process (`process_spawn_user()`), not a fork of the caller, so cwd inheritance had to be wired in separately from `process_fork()`'s. `SYS_EXEC` (`sys_exec` in `syscall.c`) passes `process_current()->cwd_cluster` down through `exec()` → `scheduler_spawn_user()` → `process_spawn_user()`, which now takes `cwd_cluster` as an explicit parameter instead of hardcoding `0`. This matches the intuitive "run a program from where I am" expectation, even though it makes NullOS's `exec()` more `posix_spawn()`-like than POSIX `exec()` (which replaces the calling process's own image rather than spawning an unrelated one) — a deliberate design choice, not an oversight.
- The only processes that still always start at the root (`cwd_cluster = 0`) are ones with **no calling process to inherit from**: the very first shell, spawned directly by `kmain` at boot (`exec("shell", 0)` in `kernel/main.c`), and anything spawned via the kernel-task path (`process_spawn()`, unrelated to user file paths).

## `fork()`

- Full duplication (not copy-on-write): a fresh page directory, a fresh physical page for every page the parent has mapped (code, data, stack — whatever's actually present, not a fixed list of regions), and a duplicated fd table entry for every open file
- `process_fork()` (`process.c`) does the heavy lifting:
  1. Atomically claims a free process-table slot under `cli`/`sti` (assigns the pid and sets `PROCESS_BLOCKED` — reserved but not runnable — in one uninterruptible step), so two `fork()`s interleaved by the preemptive timer can't pick the same slot
  2. Walks the parent's page directory beyond the two shared kernel PDEs, allocating a new physical page and calling `vmm_map_user_page()` (the same helper `exec()` uses) for every present mapping
  3. Fabricates the child's kernel stack: the 13-word frame captured from the parent's syscall entry (8 `pusha` registers + the CPU's ring3→ring0 trap frame), with `eax` forced to 0, preceded by the same 4-dummy-words-plus-return-address prologue `build_initial_stack()` uses for brand-new processes — except the return address is `isr128_resume` (a new label in `isr.asm`, right before the existing `popa`/`iret`) instead of a bootstrap function
  4. Only then flips the child to `PROCESS_READY`
- The first time the scheduler runs the child, `context_switch()`'s `ret` lands on `isr128_resume`, which does `popa; iret` off that fabricated frame — resuming in ring 3 at the exact instruction after the parent's `fork()` call, with `eax=0`
- `g_syscall_frame` (`syscall.c`): set by `isr128` right after `pusha` to a pointer at the 13-word frame above; `sys_fork()` copies it into a **local** buffer as its very first action, before anything that could be preempted, and nothing else ever reads the global again for that call — otherwise a different process's syscall entry could overwrite it mid-copy and corrupt the child being built
- `SYS_FORK (25)`: after `process_fork()` succeeds, duplicates `fd_table[parent_slot]` into `fd_table[child_slot]` — each fd then tracks its own read/write position independently, not POSIX's shared-offset semantics. Since Phase 16 (pipes), this raw struct copy is followed by `vfs_dup()` on every duplicated entry: `vfs_fd_t` used to be plain data with nothing to refcount, but a pipe end now is — see `docs/pipes.md` for why skipping this would let one process's `close()` drop a pipe's refcount to 0 while the other (the fork duplicate) is still genuinely using it. `vfs_dup()` stays a no-op for ramfs/FAT16 entries, unchanged.
- `process_t.stdin_redirect`/`stdout_redirect` (Phase 16, both default `-1` = "no redirect") are copied parent→child by `process_fork()` too, for the same reason the rest of the fd table is duplicated — see `docs/pipes.md` for what they do and how `SYS_EXEC_PIPE` sets them on a freshly spawned process instead.
- Slot exhaustion or an out-of-memory mid-copy both return -1 to the parent with no process left behind; a partial address-space copy is unwound by walking the child's own (partially built) directory and freeing its data pages, since `process_fork()` runs on the caller's small fixed-size kernel stack and can't afford a separate allocation-tracking array
- Known, explicitly out-of-scope limitations (documented in code comments, not fixed here): `process_spawn()`/`process_spawn_user()` still claim a free slot without `cli`/`sti` (same class of race `process_fork()` closes for itself); `process_exit()` never frees a process's `cr3` or its mapped pages, forked children included; the page-copy assumes physical pages fall in the identity-mapped first 8MB, the same assumption `elf_load()` already makes
- `exec()`, `elf.c`, and `scheduler_yield()`/`scheduler_block_current()` are unchanged — `fork()` reuses the existing scheduler rather than adding a parallel path
- `user/forktest.c`: calls `fork()` and prints "I'm the parent, child=PID" or "I'm the child, pid=PID" depending on the return value

## `waitpid` — real blocking (Phase 16)

- `SYS_WAIT (20)`'s **interface never changed** — it already took a specific pid, not a generic "any child" — only its implementation did. Before Phase 16, `sys_wait()` polled: `scheduler_sleep_current(10)`, rechecking every 100ms whether the target pid was still alive. It now blocks for real, the same `PROCESS_BLOCKED`/`scheduler_block_current()` machinery `fork()` and the ATA IRQ wait already use.
- `process_t.waiting_for_pid` (0 = not waiting): `sys_wait(pid)` sets it right before blocking and clears it right after waking — matching `ata_wait_irq()`'s discipline exactly, the "is it still alive?" scan and the `PROCESS_BLOCKED` transition happen inside one `cli`/`sti` section, so a target process that exits between the check and the block can never be missed. Not inherited by `process_fork()` (it's per-syscall-in-progress state, not process identity — a fresh/forked process always starts at 0).
- `process_exit()` (`process.c`) is what wakes a waiter: after marking the exiting process `PROCESS_UNUSED`, it scans the table for any `PROCESS_BLOCKED` process whose `waiting_for_pid` matches the pid that just exited, and flips only those to `PROCESS_READY` — a process waiting on one specific child is never woken by an unrelated sibling's exit. No `cli`/`sti` guard around this scan, matching `process_wake_sleepers()` right below it in the same file (also an unprotected full-table scan) — `process_exit()` only ever runs synchronously inside `sys_exit()`/`sys_kill()`, never from IRQ context, so there's no concurrent mutator to race against here.
- `nos_wait()` (libnos) and every existing caller (the shell waiting on `edit`, `selftest.c` reaping a forked child) are unaffected — same syscall number, same blocking-until-that-pid-exits behavior, just without the 100ms polling latency and busy-wakes.

## Spawning a process "not ready yet" (`process_make_ready`, Phase 16)

- `process_spawn_user()` gained a `start_blocked` parameter, threaded through `scheduler_spawn_user()`/`exec()` from their sole callers (mirroring exactly how `cwd_cluster` was threaded through in Phase 15). `0` (used by plain `SYS_EXEC`, and by `kmain`'s boot-time `exec("shell", 0, 0)`) is the original behavior: the new process is `PROCESS_READY` immediately.
- `1` (used only by `SYS_EXEC_PIPE`, see `docs/pipes.md`) leaves the new process `PROCESS_BLOCKED` — reserved but not schedulable — because `SYS_EXEC_PIPE` still needs to seed one or two `fd_table` entries in the new process's own (otherwise empty) row, and `fd_table` lives in `syscall.c`, not `process.c`. If the process were already `PROCESS_READY` in the gap between `process_spawn_user()` returning and that seeding finishing, a preemptive timer tick could let the scheduler run it with a `stdin_redirect`/`stdout_redirect` index that doesn't point at anything yet — the exact same class of premature-scheduling bug `process_fork()` already avoids by keeping a child `PROCESS_BLOCKED` until it's fully formed (see `fork()`'s own step 1, above).
- `process_make_ready(p)` is the explicit, single-purpose counterpart: flips a `PROCESS_BLOCKED` process to `PROCESS_READY`, and does nothing otherwise (so calling it twice, or on a process that already failed/exited, can't resurrect an unrelated later occupant of the same table slot). `SYS_EXEC_PIPE` calls it once, after its `fd_table` seeding is fully done — see `docs/pipes.md`.

## Relevant files

```
kernel/
  process.c/h         Process table + process_fork()
  scheduler.c/h       Cooperative round-robin
  context_switch.asm  ESP context switch
  usermode.asm        jump_to_usermode
  tss.c               Task State Segment
  pipe.c/h            In-kernel pipes (Phase 16) — see docs/pipes.md
user/
  forktest.c          calls fork(), prints the parent/child paths and PIDs
```
