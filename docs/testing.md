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
[PASS] PCI: Intel 440FX host bridge (8086:1237) present [QEMU -machine pc only]
[PASS] mkdir (selftest_dir)
[PASS] file write/read roundtrip inside selftest_dir
[PASS] fork() child inherits cwd_cluster (marker created by child found in selftest_dir)
[PASS] subdirectory files do not leak into the root
[PASS] pipe write/read roundtrip
[PASS] pipe read returns EOF after writer closes
[PASS] SYS_WRITE >128 bytes accumulates in a FAT16 file
[PASS] two-process pipe (fork writer -> exec cat -> parent)
[PASS] waitpid with 3 children: each pid collected with its own result
[PASS] mkdir/cd 3 levels deep, file at the bottom, cd .. back to /
[PASS] exec() runs a program that exists only on FAT16
[PASS] exec() rejects a FAT16 file that is not a valid program
[PASS] printf family: %d %u %x %s %c %% and snprintf truncation
[INFO] cleanup: no delete/unlink/rmdir syscall exists yet - st_root.txt, st_big.txt, st_cat.elf, st_bad.bin, st_trunc.elf, selftest_dir/ and st_d1/ (with their files) left on disk (harmless)
Selftest: 21/21 passed
```

A `[FAIL] <name>: <reason>` line pinpoints which subsystem broke without
needing to reproduce the bug by hand first.

## What each test checks

**21 tests in total** (numbering below: the cleanup note is item 22).

1. **Memory** — calls `SYS_MEMINFO` and checks it returns a plausible
   process count. There is no userland-facing syscall that allocates a
   raw heap block directly, so this exercises the closest available
   memory operation instead of a true `kmalloc()` call.
2. **Process** — calls `fork()` and checks the parent gets a PID > 0
   (the child exits immediately and silently so it doesn't re-run the
   rest of the suite); the parent then `wait()`s on the child to reap
   its process-table slot before continuing.
3. **File create** — `nos_create("st_root.txt")` (`SYS_CREATE`) succeeds.
4. **File write/read roundtrip** — writes known content, closes,
   reopens, reads it back, and compares byte-for-byte.
5. **File duplicate-create regression** — calls `nos_create()` again on
   the *same* existing filename (the Phase 10 bug: a second create used
   to add a duplicate directory entry instead of reusing it) and checks
   the original content still reads back unchanged. There's no
   directory-listing syscall that returns parsed entries (`SYS_READDIR`
   only prints via VGA), so this is the best observable symptom
   available rather than a literal duplicate-entry count.
6. **Security — invalid pointer** — calls `nos_write()` with a pointer
   into the kernel's shared 0–8MB identity map (`0x1000`, present in
   every process's page directory per `vmm_init()` but never
   `VMM_USER`) and checks the syscall returns `-1` instead of crashing.
   See [security.md](security.md) for why that region is rejected.
7. **PCI** — checks `SYS_PCI_LIST` reports at least 1 device. This
   syscall previously always returned `0`; it was changed (see
   [pci.md](pci.md) → `pci_device_count()`) specifically so this test
   could check the count without parsing VGA text output.
8. **`mkdir`** — `nos_mkdir("selftest_dir")` (`SYS_MKDIR`, Phase 15) succeeds.
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
12. **Pipe write/read roundtrip** — `nos_pipe()` gives both ends to
    this same process, so the roundtrip is fully testable without
    `fork()`/`SYS_EXEC_PIPE`: writes known content to the write end,
    reads it back from the read end, compares byte-for-byte. See
    [pipes.md](pipes.md).
13. **Pipe EOF after writer closes** — closes the write end (the last
    reference to it, since this process never forked or `dup`'d it),
    then reads from the (now permanently empty) read end and checks
    it returns `0` immediately instead of blocking. This is the same
    symmetric-close protocol [pipes.md](pipes.md) describes
    (`pipe_release_write()` waking a blocked reader), just observed
    here on an already-empty pipe rather than caught mid-block. A
    real two-process pipeline needs a second, independent process on
    the other end — that's the whole point of `SYS_EXEC_PIPE` — so
    it isn't something this single-process automated test can
    substitute for; see "Manual test: a real pipeline" below instead.
14. **`SYS_WRITE` > 128 bytes accumulates** — writes 2100 bytes as two
    `nos_write()` calls (600 + 1500, crossing the first 2048-byte
    cluster) to `st_big.txt` and reads them all back; the regression
    test for the chunked-write data loss (`fat16_write_at`).
15. **PCI: Intel 440FX host bridge** — `nos_pci_find(0x8086, 0x1237)`
    (`SYS_PCI_FIND`). **Expected to fail once Phase 25 switches QEMU to
    `-machine q35`** (different host bridge IDs): update the IDs then;
    the generic "≥ 1 device" test (7) is unaffected.
16. **Two-process pipeline** — a `fork()`ed child writes a known string
    into pipe 1, `cat` is launched with `SYS_EXEC_PIPE` (stdin ← pipe 1,
    stdout → pipe 2), and the parent reads pipe 2 to EOF and compares.
    Each process closes the pipe ends it doesn't use, otherwise EOF never
    arrives (see [pipes.md](pipes.md)). Automates what `forktest | cat`
    only covered by hand.
17. **`waitpid` with 3 children** — three `fork()`s; child *i* yields a
    different number of times (child 0 longest) and reports
    `C<i>:<its pid>` through its own pipe. The parent waits for the
    slowest child first, then the others; for each pid it checks the
    process is really gone (`SYS_KILL` on it fails) and that the result
    read from that child's pipe matches the pid `fork()` returned. A pipe
    carries the result because there is no exit-code syscall.
18. **`mkdir`/`cd` 3 levels deep** — `/ST_D1/ST_D2/ST_D3`: `pwd`
    (`SYS_GETCWD`) checked after every step, a file created/read at the
    bottom, the file opened by its 3-component path from the root, and
    `cd ..` back to `/` one level at a time (cwd always restored to the
    root, even on failure).
19. **`exec()` runs a program that exists only on FAT16** (Phase 19) —
    copies the ramfs program `cat` to a FAT16 file (`st_cat.elf`) and
    launches it from there with `SYS_EXEC_PIPE` (stdin ← pipe 1, stdout →
    pipe 2); the parent feeds a known string in and checks the same string
    comes back. Proves the whole path: `exec()` finds a program that is on
    the disk only, reads it by its directory-entry size, loads it and runs
    it. Runs from the root directory.
20. **`exec()` rejects a FAT16 file that is not a valid program** (Phase 19)
    — a file of garbage bytes, the first 100 bytes of a real ELF (valid
    header, program headers pointing past the end of the file) and a name
    that does not exist anywhere must all make `nos_exec()` return -1,
    without crashing the kernel. (Each failed `exec()` after the page
    directory was created leaks that page — see "Known limitations".)
21. **The printf family** (Phase 19) — `snprintf`/`sprintf` with `%d %u %x
    %X %s %c %%`, width, zero-padding, left-justify, precision, negative
    numbers and `INT_MIN`, a NULL `%s`, bounded truncation with the C99
    return value. During development the same code was also compared with a
    host libc over ~9000 formats (a one-off harness that is not part of the
    repository); this test runs it on the real i386 target.
22. **Cleanup** — not a PASS/FAIL check: there's no delete/unlink/rmdir
    syscall yet, so `st_root.txt`, `st_big.txt`, `st_cat.elf`, `st_bad.bin`,
    `st_trunc.elf`, `selftest_dir/` and the files inside it, and
    `st_d1/st_d2/st_d3/` with `st_deep.txt`, are left on disk. Noted in the
    output as a known limitation, not a failure.

## Manual test: a real pipeline (`cmd1 | cmd2`)

The automated two-process pipeline (test 16 above) covers a `fork()`
writer feeding an exec'd `cat`; the shell's `cmd1 | cmd2` additionally
exercises two processes both launched via `SYS_EXEC_PIPE`. None of the shell's builtins (`ps`, `echo`,
...) can sit on either side of a real pipe (they write straight to VGA
via syscalls that never touch fd 1), so `user/cat.c` was added
specifically as a minimal pipe sink, and `forktest` — which already
writes several lines via `nos_write(1, ...)` — works as an incidental
source. From the shell:

```
forktest | cat
```

Expected: the same lines `run forktest` alone would print (`forktest:
calling fork()...`, two `forktest: created fk<pid>.txt in cwd` lines,
one parent line, one child line — order may interleave, that's normal
scheduler behavior, not a bug), this time arriving via the pipe and
re-printed by `cat`. The shell's prompt only returns after **both**
processes have exited (`run_pipeline()` waits on both pids) — see
[pipes.md](pipes.md) for the full design, including why the shell
itself must close its own copies of both pipe fds for `cat` to ever
see EOF and exit.

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
- Tests 12-13 are single-process; test 16 covers the two-process case.
- There is no exit-code syscall, so test 17 passes each child's result
  through a pipe.
- A failed `exec()` does not free the page directory it already created
  (the same accepted leak as `process_exit()`, Phase 23): test 20 leaks two
  pages per run, out of ~1000 free at boot.

## Manual test: the crash handler (Phase 20)

The crash pipeline (an exception saves a record, the machine resets, Safe Mode shows it) cannot run inside `selftest` — it takes the machine down — so it is tested by hand with the shell's `crash <de|pf|gpf>` debug command, which faults on purpose (`#DE`, a read of `0xDEADBEEF`, `#GP`). **Use `make run-reboot-test`, not `make run`:** the normal `run` passes `-no-reboot` on purpose, which makes QEMU exit when the guest resets. The full procedure and what each step must show are in `docs/safemode.md` ("How to test"). It was run for all three exceptions in one QEMU session, including the reboot between them. Not covered: a crash inside the saving code (the re-entry guard), a crash before the disk is up (the halt path) and a fault in kernel mode.

## Host-side test of the ELF loader (`make test-elf`)

The kernel's ELF loader (`kernel/elf.c`) is also tested on the host, without
QEMU: `make test-elf` (in `tools/`) compiles the **real** `kernel/elf.c`
against stand-in page-allocator/page-mapper functions
(`tools/test_elf_load.c`) and runs it on every built user program. It checks
that each program loads and that every loadable byte lands where it should;
that a copy truncated at any length is rejected or still complete and is
**never read past its end** (the image sits flush against an unmapped guard
page, so an out-of-bounds read faults); that randomly corrupted headers never
crash; that a dozen crafted hostile headers are rejected (segment bigger than
its file data, offsets or addresses that wrap 32 bits, a segment reaching the
user stack, a program header table past the end, ...); and a hand-built
regression case: a pure `.bss` segment whose file offset is at or past the end
of the file (what the linker produces) must load. Run it after changing
`elf.c`, the linker script or the size of a user program. Linux only (it uses
`MAP_32BIT`).

## Relevant files

```
user/selftest.c    the test suite itself (see docs/shell.md for the file list)
tools/test_elf_load.c  host-side test of kernel/elf.c (make test-elf)
user/cat.c          minimal pipe sink, used for the manual pipeline test above
```
