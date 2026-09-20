# Pipes and `cmd1 | cmd2` (Phase 16)

## Storage: a fixed static pool, not heap-allocated

`kernel/pipe.c` keeps a fixed array, `pipe_t pipe_table[PIPE_MAX]` (`PIPE_MAX=8`, `PIPE_BUF_SIZE=512` bytes per pipe), following the exact style already used everywhere else in this codebase (`process_table[PROCESS_MAX]`, `process_stacks[PROCESS_MAX][...]`, `g_gate_waiters[PROCESS_MAX]` in `kernel/drivers/ata.c`). A pipe's 512-byte circular buffer lives **inside** its `pipe_t` slot as a plain array — nothing is ever `kmalloc()`'d for a pipe. This is deliberate: `process_exit()` still doesn't free a process's memory (a known, documented leak since Phase 13), so adding a *second* kind of heap allocation with no cleanup path would just be a second version of the same unsolved problem. A pipe is "allocated" (`pipe_create()`: an unused slot claimed, counters reset) and "freed" (`used = 0`) purely by flipping a flag on a slot that already exists — there is nothing to leak.

**Single waiter per direction is a deliberate scope limit**, not an oversight — it mirrors `kernel/drivers/ata.c`'s `g_irq_waiter` precedent exactly (a single global waiter, explicitly documented there as sufficient because only one ATA command is ever in flight at a time). Here, the target use case — `cmd1 | cmd2` — has exactly one reader and one writer per pipe. A `pipe_t` has exactly one `read_waiter`/`write_waiter` slot each; a pipe shared by more than one reader or more than one writer concurrently isn't something this implementation handles robustly, and isn't needed yet.

## Fits the existing fd model as a new backend, not a new fd type

`vfs_fd_t` (`kernel/fs/vfs.h`) already generalizes over `VFS_RAMFS`/`VFS_FAT16`. Phase 16 adds two more: `VFS_PIPE_READ`/`VFS_PIPE_WRITE` (two tags, not one — so `vfs_close()` knows which end to release), reusing `vfs_fd_t.first` as the index into `pipe_table`. A pipe fd is just another `fd_table[PROCESS_MAX][FD_PER_PROC]` slot (`kernel/syscall.c`), dispatched through the same `fd >= FD_BASE` path `SYS_READ`/`SYS_WRITE` already had — no changes to how fds are numbered or looked up.

## The blocking mechanism — symmetric, reusing `PROCESS_BLOCKED`/`scheduler_block_current()`

Same shape as `ata_wait_irq()` (`kernel/drivers/ata.c`), applied once per direction, in `kernel/pipe.c`:

- **Writer, pipe full** (`pipe_write()`): copies as many bytes as currently fit; if the buffer fills before `len` is exhausted, it registers `pipe->write_waiter = self`, sets `self->state = PROCESS_BLOCKED`, and calls `scheduler_block_current()` — all inside one `cli`/`sti` section around the check, so a wakeup that becomes available between the check and the block can never be lost (the same "check-then-block atomically" discipline `ata_wait_irq()` documents). On resume, it rechecks and continues until all `len` bytes are enqueued (or an error — see below). `SYS_WRITE` never returns a short write for this reason.
- **Reader wakes the writer** (`pipe_read()`): after removing bytes from a buffer that was full, checks `pipe->write_waiter` and flips it to `PROCESS_READY` directly (no scheduler call, matching `ata_irq_handler()`'s own "just flip the state" style).
- **Reader, pipe empty** (`pipe_read()`): blocks symmetrically (`read_waiter = self`) if `count == 0` **and** `write_refs > 0` (someone could still write). If `write_refs == 0` already, it returns `0` (EOF) immediately, without blocking. Unlike a write, a read returns as soon as *any* data is available — like a real pipe, it never waits to fill the whole requested `len`.
- **Writer wakes the reader** (`pipe_write()`): after depositing bytes into a buffer that was empty, wakes `pipe->read_waiter` the same way.

**Why this can't deadlock in the classic "both waiting on each other" sense**: a full-buffer writer's unblock condition ("space freed") is created by the reader's own reads, and an empty-buffer reader's unblock condition ("data available") is created by the writer's own writes — the standard bounded-buffer producer/consumer, not a circular wait, as long as the scheduler keeps giving both processes CPU time (it does — nothing here holds a lock across a blocking wait).

## Reference counting and the symmetric EOF / broken-pipe protocol

Each `pipe_t` tracks `read_refs`/`write_refs` — how many `fd_table` entries across *all* processes currently reference each end. A pipe is only actually freed (`used = 0`) once **both** reach 0.

- `pipe_add_read_ref()`/`pipe_add_write_ref()`: called via `vfs_dup()` (`kernel/fs/vfs.c`) whenever a pipe-backed `vfs_fd_t` is copied into a *second* `fd_table` slot by anything other than `SYS_PIPE`'s own initial creation — `fork()`'s full `fd_table` row duplication (`sys_fork()`), and `SYS_EXEC_PIPE` seeding a new process's `fd_table` with the caller's pipe end (see below). Skipping this is a real bug class: a raw struct copy without a matching refcount bump means a `close()` on just *one* of the two copies would decrement a refcount that was never incremented for that duplication, making the pipe look fully closed on that end while the other copy is still genuinely open.
- `pipe_release_read()`/`pipe_release_write()`: called by `vfs_close()`. **This is also why `sys_exit()`'s fd cleanup had to change** (Phase 16 fix, `kernel/syscall.c`): it used to zero `fd_table[slot][j].used` directly for every fd, bypassing `vfs_close()` entirely — harmless before, since `vfs_close()` was a no-op for ramfs/FAT16 beyond that same flag, but for pipes this was the *only* place a process's pipe-end references would ever get released on exit. `sys_exit()` now calls `vfs_close()` per used fd.
- **Writer exits (or closes) while a reader is blocked**: releasing the write end decrements `write_refs`; hitting 0 wakes `pipe->read_waiter` (if any) *even though no data arrived* — the reader, on resume, rechecks its loop condition, finds `write_refs == 0`, and returns `0` (EOF) instead of re-blocking.
- **Reader exits while a writer is blocked** (the symmetric, less common case for `cmd1 | cmd2`, but a real deadlock risk if unhandled): releasing the read end to 0 wakes `pipe->write_waiter`; on resume, `pipe_write()` checks `read_refs == 0` and returns `-1` (broken pipe) instead of waiting forever for space nobody will ever free.

Both directions get identical treatment: wake the other side whenever *your* refcount hits zero, and have the woken side re-check its actual condition rather than assume why it woke — the same discipline the ATA exclusion gate already uses ("more than one waiter can be released at once, so recheck after waking").

## `SYS_EXEC_PIPE` — why launching a pipeline stage isn't `fork()` + `dup2()` + `exec()`

The POSIX idiom for `cmd1 | cmd2` is: fork the shell, have the child remap its fd 0/1 and `exec()`. **That doesn't work in NullOS**, because `exec()` here does not replace the calling process's image the way POSIX `exec()` does — it spawns a **brand-new, independent process** via `process_spawn_user()`, while the calling process keeps running. Forking the shell and then calling `exec()` in the child would leave the forked child running as a third, superfluous process, with the real pipeline stage spawned as a fourth. This is the same category of wrong assumption that caused the Phase 15 `cwd_cluster`/`exec()` bug (`exec()` not inheriting the caller's state because it isn't a POSIX-style in-place replacement) — so the fix follows the exact same shape: **explicit, exec-time parameter threading**, not fork-based inheritance.

The shell instead calls `exec()` directly for each pipeline stage — exactly like `run`/`edit` already do today, no `fork()` involved — via a syscall dedicated to the redirected case:

```
SYS_EXEC_PIPE(name, stdin_fd, stdout_fd)
  → exec(name, cwd_cluster, start_blocked=1)
    → scheduler_spawn_user(..., cwd_cluster, start_blocked=1)
      → process_spawn_user(..., cwd_cluster, start_blocked=1, ...)
        → new process_t, left PROCESS_BLOCKED (not yet runnable)
  → sys_exec_pipe() seeds fd_table[new_slot] with the redirected end(s),
    sets p->stdin_redirect / p->stdout_redirect, then:
  → process_make_ready(p)
```

Two things make this safe:

1. **`fd_table` is per process slot, and `exec()` builds a brand-new slot** — nothing about `exec()` (unlike `fork()`) copies the caller's fd table into the new process. So `stdin_redirect`/`stdout_redirect` (two new `int` fields on `process_t`, `-1` = "no redirect" — the default for every process, unchanged behavior for `SYS_EXEC`/`fork()`) are just indices; `sys_exec_pipe()` (`kernel/syscall.c`) must also copy the actual pipe end (`fd_table[new_slot][j] = *stdin_src` or `*stdout_src`, plus `vfs_dup()` to bump its refcount) into the new process's own table before it can mean anything.
2. **The new process must not run before that seeding is done.** `process_spawn_user()`'s new `start_blocked` parameter (threaded through `scheduler_spawn_user()`/`exec()`, the same shape `cwd_cluster` was threaded through in Phase 15) leaves the process `PROCESS_BLOCKED` instead of immediately `PROCESS_READY` — the identical "reserved but not runnable yet" discipline `process_fork()` already uses for the same class of reason. Without it, a preemptive timer tick landing between `process_spawn_user()` returning and the `fd_table` seeding finishing could let the scheduler run the new process with a redirect index that doesn't point at anything yet. `sys_exec_pipe()` calls `process_make_ready()` once seeding is complete — see `docs/scheduler.md`.

`SYS_READ`/`SYS_WRITE` themselves need only one small addition to make the redirect transparent to the running program: `sys_read()` resolves `fd == 0` through `cur->stdin_redirect` (if set) before doing anything else; `sys_write()` does the same for `fd == 1`/`2` through `cur->stdout_redirect`. **The payoff: `cmd1`/`cmd2`'s own code needs zero pipe-awareness** — they call `nos_write(1, ...)`/`nos_read(0, ...)` exactly as every program already does (see `user/cat.c`), and the kernel transparently routes the bytes through the pipe.

`SYS_EXEC_PIPE`'s 3 registers (`name`, `stdin_fd`, `stdout_fd`) are fully spent — there's no room left for `SYS_EXEC`'s optional `arg` string in the same call. A piped command can't take an `edit`-style filename argument in this first cut; a stated scope limit, not an oversight.

## The shell's own responsibility: closing unused pipe ends

Because there's no `fork()` in this launch path, the "someone must close their own unused copy of each pipe end" duty falls on the **shell alone** (`run_pipeline()`, `user/shell.c`). Sequence:

1. `nos_pipe(fds)` → shell's own `fd_table` gets `read_fd`/`write_fd`.
2. `nos_exec_pipe(cmd1, -1, write_fd)` → `cmd1`'s new process gets its own `fd_table` entry for the write end (refcount bumped via `vfs_dup()`), `stdout_redirect` set.
3. `nos_exec_pipe(cmd2, read_fd, -1)` → same for `cmd2`'s read end / `stdin_redirect`.
4. **The shell closes both `read_fd` and `write_fd` in its own `fd_table`.** This is not optional: if skipped, the shell's own lingering reference means the write end's refcount never reaches 0 even after `cmd1` (the real writer) exits and closes its own copy — so `cmd2` never sees real EOF and blocks forever waiting for data that will never come. This is the textbook Unix "close unused pipe ends in the parent" bug, adapted to a model with no `fork()` in this particular path.
5. `nos_wait(pid1)` then `nos_wait(pid2)` — real blocking (see `docs/scheduler.md`), so the shell uses no CPU while either stage runs.

## File redirection reuses the same path (Phase 17)

`cmd < file` / `cmd > file` open the file in the shell and pass its fd to `SYS_EXEC_PIPE` as `stdin_fd`/`stdout_fd`; the kernel copies that fd row exactly like a pipe end (`vfs_dup()` is a no-op for FAT16 fds). `sys_exec_pipe()` also clears the global `exec_arg`, since it carries no argument itself and a leftover one from an earlier plain `exec()` would otherwise reach the new process's `SYS_GETARG` (e.g. `cat` opening a stale file name instead of reading its redirected stdin).

## Manual test: `forktest | cat`

None of the shell's existing builtins (`ps`, `echo`, ...) can sit on either side of a real pipe — they write straight to VGA via syscalls that never go through fd 1 at all (e.g. `SYS_PS` calls `process_dump()` directly), so there was nothing suitable already in the tree to demonstrate an actual two-process pipeline end to end. `user/cat.c` was added for exactly this: it reads all of stdin and writes it to stdout, the minimal real pipe sink. `forktest` already writes several lines via `nos_write(1, ...)`, making it a usable (if incidental) pipe source. See `docs/testing.md` for the exact manual test steps and expected output.

## Relevant files

```
kernel/
  pipe.c/h            Fixed pipe pool, blocking read/write, refcounting
  fs/vfs.c/h          VFS_PIPE_READ/VFS_PIPE_WRITE backend, vfs_dup()
  syscall.c/h         SYS_PIPE, SYS_EXEC_PIPE, stdin_redirect/stdout_redirect
                        resolution in sys_read()/sys_write(), the sys_exit()
                        vfs_close() fix, sys_wait()'s real-blocking rewrite
  process.c/h         stdin_redirect/stdout_redirect/waiting_for_pid fields,
                        process_spawn_user()'s start_blocked, process_make_ready()
user/
  lib/nullos.c/h      nos_pipe(), nos_exec_pipe()
  shell.c             "cmd1 | cmd2" parsing and launch (run_pipeline())
  cat.c               minimal pipe sink, used only to demonstrate/test pipes
```

See `docs/scheduler.md` for `waitpid`'s real-blocking rewrite and `process_make_ready()`, and `docs/syscalls.md` for the full `SYS_PIPE`/`SYS_EXEC_PIPE` signatures.
