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

Found during Phase 23-B, outside its scope (not memory release):

- TODO later: a process killed while blocked on an ATA command leaves
  `g_irq_waiter` (or an entry of `g_gate_waiters[]`) in
  `kernel/drivers/ata.c` pointing at its slot. If the slot is reused by a
  process that is `PROCESS_BLOCKED` when the IRQ arrives (a spawn reserves a
  slot as BLOCKED while it builds it), the handler wakes the wrong process.
  Killed while holding the gate (`g_ata_busy`), it is never released.
  Pre-existing. Related files: kernel/drivers/ata.c, kernel/process.c.
- TODO later: if a parent is killed between reserving a child (fork() or
  `SYS_EXEC_PIPE`'s start-blocked exec()) and making it ready, the child
  stays `PROCESS_BLOCKED` forever, holding its slot and its address space.
  Pre-existing. Related files: kernel/process.c, kernel/syscall.c.
- TODO later: `elf_load()` allocates a fresh frame for every page of every
  PT_LOAD segment; two segments sharing a page would map it twice and leak
  the first frame. The current linker script page-aligns the segments, so
  no program in the tree hits it. Related files: kernel/elf.c.
- TODO later: after killing `selftest` with Ctrl+C during the "file create"
  test, the kernel heap (the "Heap" field of `fetch`) went from 229344 B to
  229300 B, 44 bytes less, and did not change after a full selftest. Not
  investigated, not blocking. Unconfirmed guesses: a kernel structure of a
  process killed in the middle of a file operation (an open-file or fd
  structure) is not released by the kill, or it is allocator fragmentation.
  Related files: kernel/syscall.c (`sys_kill()`, `close_all_fds()`),
  kernel/fs/fat16.c, kernel/memory/heap.c.

Phase 24-A (raw e1000 driver), left for later:

- WIP: `docs/network.md` is a work in progress until Phase 24 closes; link it
  from the README documentation list at that point (the README is only touched
  when a phase closes). Related files: docs/network.md, README.md.
- TODO later: when the e1000 driver becomes reachable from processes
  (24-B/C), wrap `e1000_send()`/`e1000_poll_rx()` in `irq_save()`/
  `irq_restore()` or a lock. Related files: kernel/drivers/e1000.c.
- TODO later: `e1000_send()` returns -1 on a transmit timeout but the
  descriptor stays the card's; a later send reuses the single TX buffer while
  the card may still read it. Harmless while frames are sent one at a time at
  boot. Related files: kernel/drivers/e1000.c.
- TODO later: on an init failure after the PCI command register was changed,
  Memory Space and Bus Master stay on (RX/TX are disabled). Related files:
  kernel/drivers/e1000.c.
- TODO later: kernel mappings above 8 MB only reach process directories
  created after them (`vmm_create_directory()` copies the kernel PDEs present
  at that moment). Fine for drivers mapped at boot; a mapping made later
  needs the PDE pushed into every live directory. Related files:
  kernel/memory/vmm.c.
