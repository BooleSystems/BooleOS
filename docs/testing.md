# Selftest — automated regression suite

`user/selftest.c` is a standalone diagnostic tool, not a numbered phase.
It runs a small battery of checks across the kernel subsystems and prints
`[PASS]`/`[FAIL]` per check plus a final summary, so a change can be
sanity-checked with `run selftest` instead of manually typing
`touch`/`edit`/`ls`/`fork` by hand every time.

## Running it

From the shell:

```
run selftest
```

Expected output shape (exact wording may evolve as tests are added):

```
=== NullOS selftest ===
[PASS] memory: SYS_MEMINFO reports pmm/heap/process stats
[PASS] fork() returns a valid child PID (> 0) to the parent
[PASS] file create (selftest_tmp.txt)
[PASS] file write/read roundtrip
[PASS] file duplicate-create regression (Phase 10)
[PASS] invalid pointer into kernel-only region (0x1000) rejected by syscall
[PASS] PCI enumeration found at least 1 device
[INFO] cleanup: no delete/unlink syscall exists yet - selftest_tmp.txt left on disk (harmless)
Selftest: 7/7 passed
```

A `[FAIL] <name>: <reason>` line pinpoints which subsystem broke without
needing to reproduce the bug by hand first.

## What each test checks

1. **Memory** — calls `SYS_MEMINFO` and checks it returns a plausible
   process count. There is no userland-facing syscall that allocates a
   raw heap block directly, so this exercises the closest available
   memory operation instead of a true `kmalloc()` call.
2. **Process** — calls `fork()` and checks the parent gets a PID > 0
   (the child exits immediately and silently so it doesn't re-run the
   rest of the suite); the parent then `wait()`s on the child to reap
   its process-table slot before continuing.
3. **File create** — `sys_create("selftest_tmp.txt")` succeeds.
4. **File write/read roundtrip** — writes known content, closes,
   reopens, reads it back, and compares byte-for-byte.
5. **File duplicate-create regression** — calls `sys_create()` again on
   the *same* existing filename (the Phase 10 bug: a second create used
   to add a duplicate directory entry instead of reusing it) and checks
   the original content still reads back unchanged. There's no
   directory-listing syscall that returns parsed entries (`SYS_READDIR`
   only prints via VGA), so this is the best observable symptom
   available rather than a literal duplicate-entry count.
6. **Security — invalid pointer** — calls `sys_write()` with a pointer
   into the kernel's shared 0–8MB identity map (`0x1000`, present in
   every process's page directory per `vmm_init()` but never
   `VMM_USER`) and checks the syscall returns `-1` instead of crashing.
   See [security.md](security.md) for why that region is rejected.
7. **PCI** — checks `SYS_PCI_LIST` reports at least 1 device. This
   syscall previously always returned `0`; it was changed (see
   [pci.md](pci.md) → `pci_device_count()`) specifically so this test
   could check the count without parsing VGA text output.
8. **Cleanup** — not a PASS/FAIL check: there's no delete/unlink
   syscall yet, so `selftest_tmp.txt` is left on disk. Noted in the
   output as a known limitation, not a failure.

## Known limitations

- No delete/unlink syscall exists yet, so the test file persists across
  runs (harmless — each run just re-creates/overwrites it).
- Test 5 can't directly verify "no duplicate directory entry" since
  there's no syscall that returns parsed directory entries — it checks
  the closest observable symptom instead (see above).
- Test 1 doesn't perform a real heap allocation, since no syscall
  exposes `kmalloc()` to userland.

## Relevant files

```
user/selftest.c    the test suite itself (see docs/shell.md for the file list)
```
