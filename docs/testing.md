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
[PASS] file create (st_root.txt)
[PASS] file write/read roundtrip
[PASS] file duplicate-create regression (Phase 10)
[PASS] invalid pointer into kernel-only region (0x1000) rejected by syscall
[PASS] PCI enumeration found at least 1 device
[PASS] mkdir (selftest_dir)
[PASS] file write/read roundtrip inside selftest_dir
[PASS] fork() child inherits cwd_cluster (marker created by child found in selftest_dir)
[PASS] subdirectory files do not leak into the root
[INFO] cleanup: no delete/unlink/rmdir syscall exists yet - st_root.txt and selftest_dir/ (with its files) left on disk (harmless)
Selftest: 11/11 passed
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
3. **File create** — `sys_create("st_root.txt")` succeeds.
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
8. **`mkdir`** — `sys_mkdir("selftest_dir")` (Phase 15) succeeds.
9. **File write/read roundtrip inside a subdirectory** — same as test 4,
   but `cd`'d into `selftest_dir` first. This is the exact scenario
   that exposed the `exec()`-doesn't-inherit-cwd bug found during Phase
   15's own manual test (there it was `edit`'s exec'd process losing
   the cwd; here, `selftest` never `exec()`s, so this instead checks
   the FAT16/VFS side directly: that a relative create/write/read all
   land inside `selftest_dir`, not silently at the root).
10. **`fork()` inherits `cwd_cluster`** — while still `cd`'d into
    `selftest_dir`, `fork()`s; the child creates a marker file via a
    relative path with no `cd` of its own, and the parent (still in
    `selftest_dir`) opens that same relative name afterward — finding
    it proves the child's `cwd_cluster` matched the parent's at
    `fork()` time. Deliberately done in-process here (a real `fork()`,
    not `exec()`) rather than duplicating `forktest.c`'s exec-based
    marker-file check — `fork()`'s cwd inheritance was never the bug
    this phase found (only `exec()`'s was; see
    [scheduler.md](scheduler.md)), but it's cheap to keep covered by an
    actual regression test here too.
11. **Subdirectory files don't leak into the root** — `cd ..` back to
    the root, then checks that neither file created inside
    `selftest_dir` above can be opened by name at the root. This is the
    exact observable symptom the manual test caught (a file written
    inside a subdirectory showing up at the root instead) — see
    [filesystem.md](filesystem.md) → "Subdirectories" and
    CHANGELOG.md `[0.15.0]` for the bug this guards against. There's no
    parsed-directory-listing syscall to check against directly
    (`SYS_READDIR` only prints via VGA), so "can't be opened by this
    name at the root" is the next best observable proof of isolation.
    **All test filenames (`st_root.txt`, `st_sub.txt`, `st_mark.txt`)
    are deliberately given distinct FAT 8.3 encodings** (first 8 chars
    + 3-char extension) — an earlier revision used
    `selftest_root.txt`/`selftest_sub.txt`/`selftest_fork_marker.txt`,
    which all truncate to the identical packed name `SELFTESTTXT` and
    made this exact test falsely fail (test 3's *root* file, sharing
    that same 8.3 identity, was mistaken for a leaked subdirectory
    file). That was a test-naming bug, not a `dir_lookup()` bug — the
    kernel correctly scopes lookups by directory throughout.
12. **Cleanup** — not a PASS/FAIL check: there's no delete/unlink/rmdir
    syscall yet, so `st_root.txt`, `selftest_dir/`, and the two
    files inside it are left on disk. Noted in the output as a known
    limitation, not a failure.

## Known limitations

- No delete/unlink/rmdir syscall exists yet, so test files/directories
  persist across runs (harmless — each run just re-creates/overwrites
  the same names).
- Test 5 can't directly verify "no duplicate directory entry" since
  there's no syscall that returns parsed directory entries — it checks
  the closest observable symptom instead (see above). Test 11 has the
  same limitation for "does this file exist at the root" — it checks
  "can this name be opened at the root" instead.
- Test 1 doesn't perform a real heap allocation, since no syscall
  exposes `kmalloc()` to userland.

## Relevant files

```
user/selftest.c    the test suite itself (see docs/shell.md for the file list)
```
