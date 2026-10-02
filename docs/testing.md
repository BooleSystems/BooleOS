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
=== BooleOS selftest ===
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
[PASS] copy-on-write fork: writes stay private (child first, then parent first)
[PASS] copy-on-write fork: 3 generations, refcount follows the live holders
[PASS] copy-on-write fork under preemption: 4 children rewrite 4 shared pages
[PASS] unlink() deletes a file; a second unlink of it fails
[PASS] unlink() refuses an open file, then succeeds after close
[PASS] rmdir() deletes an empty directory; a second rmdir of it fails
[PASS] rmdir() refuses a non-empty directory, then succeeds once emptied
[PASS] unlink()/rmdir() refuse the wrong kind, the root, "." and ".."
[PASS] rmdir() refuses a directory that is a live process's cwd
[PASS] cleanup: every test file and directory deleted
[PASS] concurrent console writers don't corrupt kernel state (vga race, 0.22.1)
[PASS] fork()+exit x30 returns every page to the PMM (23-B)
[PASS] exec()+exit x20 returns every page to the PMM (23-B)
Selftest: 34/34 passed
```

A `[FAIL] <name>: <reason>` line pinpoints which subsystem broke without
needing to reproduce the bug by hand first.

## What each test checks

**34 tests in total**; the cleanup (31) is a counted test too.

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
22. **Copy-on-write `fork()`, writes stay private** (Phase 21). A
    page-aligned global buffer, two rounds. After `fork()` the buffer's
    reference count (`nos_pageref()`) must be 2 in the parent. In round one
    the child writes first and the parent must still see its old byte, with
    the count back to 1; then the parent writes and the child must still see
    its own. Round two swaps the order. Pipes force the order between the
    two processes. Neither write may show up in the other process.
23. **Copy-on-write `fork()`, three generations** (Phase 21). This
    process forks a middle process, which forks a youngest one, all holding
    the same page. The count must be 3 in each while all three are alive,
    2 after the youngest exits without writing, and 1 after the middle one
    exits too, with the page content unchanged the whole time. The last
    holder must then be able to write its page.
24. **Copy-on-write `fork()` under preemption** (Phase 21). Four children,
    forked one after another while the earlier ones are already running,
    each rewriting every 64th byte of four shared pages for about 300 ms
    (many timer ticks) while the parent keeps rewriting its own copy between
    forks. Each child must see only its own bytes plus the untouched ones it
    inherited; the parent must end with its own content and a count of 1 on
    every page, so no reference is lost or left behind.
25. **`unlink()` of a file** (Phase 22) — creates `st_unl.txt`, deletes it,
    and checks it no longer opens and that a second `unlink()` of the name
    fails. Creating the name again must give an empty file, not the old
    data (the new entry reuses the `0xE5` slot).
26. **`unlink()` refuses an open file** (Phase 22) — with `st_open.txt` open
    in this process, `unlink()` must fail and leave the file in place; after
    `close()` the same call succeeds. The kernel checks the fds of every
    live process, this one included.
27. **`rmdir()` of an empty directory** (Phase 22) — `st_rmd` is created and
    removed; `cd` into it must fail afterwards, and so must a second
    `rmdir()`.
28. **`rmdir()` refuses a non-empty directory** (Phase 22) — `st_full`
    holds `st_in.txt`, so `rmdir()` must fail and keep the file. After
    `unlink("st_full/st_in.txt")` (a path through the directory) the
    `rmdir()` succeeds.
29. **Wrong kind and special names** (Phase 22) — `unlink()` of a directory,
    `rmdir()` of a file, and `rmdir()` of `/`, `.` and `..` must all fail
    without deleting anything.
30. **`rmdir()` refuses a live process's cwd** (Phase 22) — a forked child
    `cd`s into `st_cwd` and waits on a pipe; while it is alive `rmdir()`
    must fail. The parent also `cd`s into the directory and tries
    `rmdir("../st_cwd")` on its own cwd, which must fail too. Once the child
    has exited and been reaped, and the parent is back at `/`, the same
    `rmdir()` succeeds.
31. **Cleanup** (Phase 22) — deletes every file and directory the suite
    creates, deepest first: `st_root.txt`, `st_big.txt`, `st_cat.elf`,
    `st_bad.bin`, `st_trunc.elf`, `selftest_dir/` with its two files,
    `st_d1/st_d2/st_d3/` with `st_deep.txt`, and the names of tests 25–30
    in case one of them stopped halfway. A name that is already gone is
    fine; the test fails only if something is still there afterwards. The
    first run on a disk used by an older selftest also removes what that
    one left behind.
32. **Concurrent console writers don't corrupt kernel state** (0.22.1) —
    four children and this process all `SYS_WRITE` ~4000 bytes each at the
    same time (50+ screen-fuls apiece, forcing many scrolls), the exact
    race that used to corrupt `kernel/drivers/vga.c`'s shared cursor state
    (see "Known limitations" below for why this can't check the screen
    itself, and the manual test right after this list for the check that
    does). What it does check through syscalls alone: every child reaps
    cleanly (`SYS_KILL` on it fails afterward, the same pattern test 17
    uses), and an unrelated syscall (`SYS_PCI_LIST`) still behaves normally
    right after the storm. That checks the more serious failure mode of the
    original bug: `term_row` running past `VGA_ROWS` before being clamped, a
    write past the mapped VGA buffer into unrelated physical memory.
33. **`fork()` + exit returns every page** (Phase 23-B) — reads the free
    PMM page count through `SYS_MEMINFO`, then 30 times forks a child that
    writes a page shared copy-on-write with the parent (forcing a private
    copy) and exits, each one reaped with `nos_wait()`. After a few
    `nos_yield()`s the count must be exactly the same as before. Before
    23-B every exit leaked the child's page directory and page tables. The
    count is stable because nothing else allocates pages while the test
    runs: the heap stopped taking PMM pages at boot, pipes and fd tables
    are static, and the shell allocates nothing while it waits for a key.
    Typing a command into the shell during this test changes the count and
    fails it.
34. **`exec()` + exit returns every page** (Phase 23-B) — the same check
    over 20 runs of `cat` from the ramfs, started with `nos_exec_pipe()`
    with a pipe as stdin whose write end the parent closes at once, so
    `cat` reads EOF and exits. Each run has its own directory, page tables,
    ELF pages and stack pages.

## Manual test: typing while a background process prints (0.22.1)

The automated test above forces the race that used to corrupt shared
console state, but nothing reads the screen back through a syscall, so it
can't confirm the screen itself looks right. That needs a human looking
at it. Run this by hand after any change to the console output path:

```
run selftest
```

Then, while it is still printing (roughly the first second or two), type
a short command and press Enter, for example:

```
help
```

Expected: the typed characters echo correctly and `help`'s output prints
once, right after `selftest` finishes its own output (both may still
visually interleave line-by-line while both processes are printing at
once; that is normal shared-terminal behavior, not a bug, see the note
on `vga_putchar()` in `kernel/drivers/vga.c`). What must NOT happen: a
whole line duplicating or vanishing, or `help` (or any command) appearing
to run a second time without being typed again. Repeat two or three times,
typing at a different moment in `selftest`'s output each time, since the
original bug depended on the exact timing of a preemption landing mid
console write.

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
- No syscall reads back screen content, so test 32 can only check that
  concurrent console writers don't corrupt kernel state, not that the
  screen displays correctly; that needs the manual test right after the
  test list.

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
