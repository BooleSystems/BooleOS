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

Phase 18-A (HAL, first pass) — see `docs/hal.md`:

- WIP: `idt.c`'s exception handler still calls `vga_*` directly (runs in an
  unstable state; deliberately not moved behind the HAL). Decide whether it
  should get a minimal, dependency-free console path of its own. Files:
  `kernel/idt.c`, `kernel/hal.c`.
- WIP: `boot_get_memory_map()` has no caller yet; `kmain` still calls
  `pmm_init(64 * 1024)` with a hardcoded size. Feed the real map into the PMM
  (behavior change, needs its own test). Files: `kernel/main.c`,
  `kernel/memory/pmm.c`, `kernel/hal.c`.
- WIP: `kernel/keyboard.c` still includes `drivers/vga.h` directly (driver
  internals); revisit when `msg(ID)`/console work touches it.

Pre-existing bug (found while testing the HAL first pass, NOT caused by it):

- **`edit` with no file name: Ctrl+S shows the misleading "saved (no disk)"
  and saves nothing.** `user/edit.c` only calls `load_file()` when a name was
  given, so `file_fd` stays at its initial `-1` (`edit.c:59`); the Ctrl+S
  handler (`edit.c` ~L233) then takes its `else` branch, which covers two
  different cases — "no disk" and "no file name" — under one message
  ("saved (no disk)", originally "salvo (sem disco)" since commit
  `cf8a977`). It never reaches `nos_write_file()`, so no disk code is
  involved. Expected: with the `[no name]` buffer, Ctrl+S should ask for a
  name ("save as"), create the file, and only report "no disk" when the write
  really failed. Never caught because every earlier test used `edit <file>`.
  Files: `user/edit.c`.
