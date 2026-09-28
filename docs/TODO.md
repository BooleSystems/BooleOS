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

Known, intermittent, NOT blocking any phase:

- TODO later: `probe()` in `kernel/drivers/ata.c` occasionally reports "no disk"
  at boot (seen in 17-B, in a Phase 20 test session, never reproduced on demand:
  9 cycles in 17-C and a 6-boot crash/reboot session in Phase 20 all showed
  `bsy cleared` in 1-2 ticks and status `0x58` after IDENTIFY). The temporary
  serial-only trace `TEMP-DEBUG(ata-probe)` (`[ATADBG]` lines, with its `dbg_*`
  helpers, in `kernel/drivers/ata.c`) is deliberately LEFT in the kernel to catch
  the next occurrence: when it happens, keep the serial log of that boot and of the
  one before it, look at `altstatus BEFORE soft reset` and the step that failed,
  fix the cause, then REMOVE the trace. Related files: kernel/drivers/ata.c.
