# Documentation TODO

Minimal trail of documentation still owed for work done on the `nightly`
branch. Whenever a relevant code change lands without its full write-up,
leave a one-line stub here instead of leaving no trace, for example:

- `WIP: document <feature> (files: X, Y, Z)`
- `TODO later doc. Related files: ...`

The stubs do not need to be complete at every commit. They are resolved
during the final polish of each version, before `nightly` is merged into
`main`: write the real description into the relevant `docs/<subject>.md`,
then delete the stub from this file. This file should be empty (headers
only) whenever a version is closed.

## Pending

Phase 17-A code changes, still to be written into `docs/*.md` at the
final polish of 0.17.0:

- WIP: document `vmm_map_page()`/`vmm_map_user_page()` returning `int`
  and the `VMM_ERR_RANGE`/`VMM_ERR_NOMEM` codes, plus how each caller
  handles failure (`heap_expand`, `exec`'s user stack, `elf_load`,
  `process_fork`) and that they free the just-allocated page.
  Target: `docs/memory.md`. Files: `kernel/memory/vmm.h`, `vmm.c`,
  `heap.c`, `kernel/exec.c`, `kernel/elf.c`, `kernel/process.c`.
- WIP: document the `virt < 0x800000` rejection in
  `vmm_map_user_page()` (protects the shared kernel identity map).
  Target: `docs/memory.md` and/or `docs/security.md`. File:
  `kernel/memory/vmm.c`.
- WIP: document `pmm_init()`'s overflow-free page count
  (`256 + mem_upper / 4`) and the `total_pages <= 256` guard.
  Target: `docs/memory.md`. File: `kernel/memory/pmm.c`.
- WIP: document `fat16_init()` rejecting `sectors_per_cluster == 0`.
  Target: `docs/filesystem.md`. File: `kernel/fs/fat16.c`.
- WIP: document `alloc_pid()` (EFLAGS save/restore, why not bare
  cli/sti). Target: `docs/scheduler.md`. File: `kernel/process.c`.
- WIP: `docs/pci.md` still describes the `bars:` line generically —
  document that only the BARs a header type has are read (type 0: 6,
  type 1: 2, type 2: 1, multi-function bit masked). Files:
  `kernel/drivers/pci.c`, `pci.h`.

Phase 17-B:

- WIP: document the Shift bit (bit 9) in the `SYS_READ_RAW` value and
  `edit.c`'s `sc_map_shift[]`. Targets: `docs/kernel.md` (keyboard),
  `docs/shell.md` or wherever the editor is described (the
  `SYS_READ_RAW` return format in `docs/syscalls.md` is already updated). Files: `kernel/keyboard.c`,
  `user/edit.c`.
- WIP: document `process_spawn_user()`'s atomic slot reservation
  (reserve as BLOCKED under saved-EFLAGS cli, fill fields, publish state
  last), the `irq_save()`/`irq_restore()` helpers, and the removal of
  `process_spawn()`/`scheduler_spawn()`. Target: `docs/scheduler.md`
  (already trimmed of the stale "unprotected slot claim" note). Files:
  `kernel/process.c`, `kernel/scheduler.c`.
- WIP: document `fat16_write_file()`'s directory guard. Target:
  `docs/filesystem.md`. File: `kernel/fs/fat16.c`.
- Known limitation, Phase 28 scope (not fixed in 17-B): `dir_buf` in
  `kernel/fs/fat16.c` is one global buffer shared by every FAT16 caller,
  and `fat16_write_file()` keeps a pointer into it across blocking
  `ata_write_sector()` calls, so a concurrent FAT16 call from another
  process can clobber it before the final dirent write. Fix belongs with
  the whole-operation FAT16 lock (ROADMAP Phase 28).

Phase 17-C (syscall table in `docs/syscalls.md` is already updated for
`SYS_GETCWD`/`SYS_REBOOT`/`SYS_SHUTDOWN` and the `SYS_WRITE`/
`SYS_WRITE_FILE` semantics):

- WIP: document `fat16_write_at()` (positional write, dirent looked up
  fresh each call, FAT flushed only when the chain grew, data -> FAT ->
  dirent order), `fat16_get_path()` (path rebuilt by walking `..`,
  8.3 uppercase names, 16-level cap), the shared
  `dirent_display_name()`, and the `vfs_write` (stream, at `fd->pos`) vs
  `vfs_write_all` (whole-file replace, for `SYS_WRITE_FILE`) split.
  Target: `docs/filesystem.md`. Files: `kernel/fs/fat16.c`, `fat16.h`,
  `vfs.c`, `vfs.h`, `kernel/syscall.c`.
- WIP: document the new shell features — `pwd`, `cat <file>`,
  `cmd < in` / `cmd > out` (`run_redirected()`), `reboot`, `shutdown` —
  and that file fds go through `SYS_EXEC_PIPE` exactly like pipe ends.
  Targets: `docs/shell.md`, `docs/pipes.md`. Files: `user/shell.c`,
  `user/cat.c`.
- WIP: document the libnos string/memory helpers (standard libc names on
  purpose; `memcpy`/`memset`/`memmove` are `rep movsb`/`rep stosb` to avoid
  GCC's loop-idiom recursion) and `nos_uitoa`. Target: `docs/kernel.md`
  ("User-space syscall library (libnos)"). Files: `user/lib/nullos.c/h`.
- WIP: document `kernel/power.c` (`power_reboot()`/`power_shutdown()`,
  the 8042 reset, PIIX4 PMBA + `PM1a_CNT`, QEMU's `-no-reboot`/
  `-no-shutdown` effect on testing them) and `pci_find_device()`.
  Targets: `docs/kernel.md`, `docs/pci.md`, `docs/testing.md`. README file
  list: add `kernel/power.c/h` when Phase 17 closes.
- Known limitations / future work (17-C scope cuts, not bugs):
  - Builtins (`ls`, `ps`, `mem`, `lspci`, `echo`, ...) cannot be
    redirected: they print from the shell process or the kernel, and a
    process can't redirect its own fd 1. Would need a dup2-style syscall.
  - `cmd > f` / `cmd < f` cannot be combined with `|`, and neither path
    passes arguments (`SYS_EXEC_PIPE` has no register left for one).
  - `exec_arg` in `kernel/syscall.c` is ONE global shared by every
    process: `SYS_GETARG` reads whatever the last `sys_exec()` stored, so
    an argument can be overwritten by another exec before the child reads
    it. 17-C only made `sys_exec_pipe()` clear it. Proper fix: per-process
    argument storage.
  - `fat16_write_at()` uses the global `sector_buf` across blocking disk
    writes (like the read path) — same shared-buffer hazard as `dir_buf`,
    Phase 28 whole-operation FAT16 lock.
  - `SYS_WRITE` still chunks at 128 bytes and every chunk does its own
    dirent lookup, so large redirected output is slow-ish.
- WIP: document the new selftest case (14 tests now, `st_big.txt` left
  on disk) and the serial backspace mirror (`\b \b`) / prompt-safe
  backspace echo. Targets: `docs/testing.md`, `docs/kernel.md` (VGA/serial
  mirroring). Files: `user/selftest.c`, `kernel/drivers/vga.c`,
  `kernel/syscall.c`.

### Pre-existing debt (not a 17-C item): intermittent ATA `probe()` "no disk"

- First seen on a 17-B boot (before any `power.c`/`syscall.c`
  change). Briefly suspected to be tied to `reboot` (one boot after a real
  reboot printed `no disk` / `no FAT16 disk`), but 9 boot/reboot cycles
  (5 with real reboot in `make run-reboot-test`, 4 plain `make run`) with
  serial tracing of `probe()` never reproduced it: always BSY cleared,
  status 0x50 after select, 0x58 after IDENTIFY. No evidence that `reboot`
  causes or worsens it. Still unexplained; the tracing was removed.
  Ideas if it returns: timing right after reset/SRST, stale controller
  state, QEMU-specific behaviour (confirm before assuming).

### Resolved / clarified in 17-C

- **`reboot` "looked like" `shutdown` under `make run`: NOT a kernel bug.**
  `make run` passes `-no-reboot`, which makes QEMU turn a guest reset into
  a shutdown; together with `-no-shutdown` it ends paused. Without
  `-no-reboot`, `reboot` restarts the guest for real (boot banner reappears
  on serial). `-no-reboot` stays on `run`/`debug` on purpose (post-mortem
  state on a triple fault). New target `make run-reboot-test` omits it.
  (Exposed the ATA bug above.)
- **Bare `run` printed `[EXEC] not found:`: fixed.** `cmd_run()` in
  `user/shell.c` now trims, validates the name, and returns the pid; the
  `_start()` foreground path and `run_command()` both call it (single
  implementation).
