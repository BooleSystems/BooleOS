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
  `docs/syscalls.md` (`SYS_READ_RAW` return format), `docs/shell.md` or
  wherever the editor is described. Files: `kernel/keyboard.c`,
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
