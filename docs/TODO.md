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

Phase 19 (SDK / app-development experience), work in progress:

- WIP: `docs/testing.md`: the selftest now has 22 tests (exec of a FAT16-only
  program, rejection of garbage / truncated / missing programs, the printf
  family; leaves `st_cat.elf`, `st_bad.bin`, `st_trunc.elf` on disk).
  Files: `user/selftest.c`.
- WIP: `docs/filesystem.md`: exec() as a new consumer of `vfs_open()`; the
  FAT16 shared `sector_buf`/`dir_buf` hazard (Phase 28) now also applies to
  `exec()`'s blocking disk reads. Files: `kernel/exec.c`.
- At the phase close: README (file list: `sdk/`, `user/lib/nosstdio.c`,
  docs list), CHANGELOG (one `[0.19.0]` entry), ROADMAP (Phase 19 done; the
  Phase 28 item "elf_load never receives the file size / page_end overflow" is
  now fixed), `version.h`, PROGRESS.
