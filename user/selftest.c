/* nullos/user/selftest.c — automated regression test suite. Run with
   "run selftest" from the shell after any kernel change to get a quick
   PASS/FAIL readout instead of manually typing touch/edit/ls/fork by
   hand every time. Not a numbered phase — just a standalone diagnostic
   tool, and expected to grow as more subsystems get their own test. */

#include "lib/nullos.h"

/* ── tiny helpers (no libc) ───────────────────────────────────────── */

static void st_puts(const char *s) {
    nos_write(1, s, strlen(s));
}

/* ── PASS/FAIL bookkeeping ────────────────────────────────────────── */

static int g_tests_run = 0;
static int g_tests_passed = 0;

static void st_pass(const char *name) {
    st_puts("[PASS] "); st_puts(name); st_puts("\n");
    g_tests_run++;
    g_tests_passed++;
}

static void st_fail(const char *name, const char *reason) {
    st_puts("[FAIL] "); st_puts(name); st_puts(": "); st_puts(reason); st_puts("\n");
    g_tests_run++;
}

/* ── the test suite itself ────────────────────────────────────────── */

void _start(void) {
    st_puts("=== NullOS selftest ===\n");

    /* 1. Memory: there's no userland-facing syscall that allocates a
       raw heap block directly, so this exercises the closest available
       memory operation (SYS_MEMINFO, which reads pmm_free_pages() and
       heap_free_bytes() from the kernel side) and checks it doesn't
       error and reports at least this process in the table. */
    {
        uint32_t pmm = 0, heap = 0, nprocs = 0;
        nos_meminfo(&pmm, &heap, &nprocs);
        if (nprocs == 0)
            st_fail("memory: SYS_MEMINFO reports pmm/heap/process stats",
                     "nprocs == 0 (expected >= 1 for this process)");
        else
            st_pass("memory: SYS_MEMINFO reports pmm/heap/process stats");
    }

    /* 2. Process: fork(). The child exits immediately and silently so
       it doesn't re-run (and double-report) the rest of the suite. */
    {
        int ret = nos_fork();
        if (ret == 0) {
            nos_exit(0);
        } else if (ret > 0) {
            nos_wait(ret);   /* reap the child's process-table slot before continuing */
            st_pass("fork() returns a valid child PID (> 0) to the parent");
        } else {
            st_fail("fork() returns a valid child PID (> 0) to the parent",
                     "fork() returned a negative value");
        }
    }

    /* 3-5. File tests: create, write/read roundtrip, and the Phase 10
       duplicate-create regression — all on one disposable test file,
       named so it can't collide with anything a real user created. */
    {
        const char *fname   = "st_root.txt";
        const char *content = "NullOS selftest data 1234\n";
        unsigned int clen   = strlen(content);
        int file_ok = 1;

        /* 3. creation */
        int fd = nos_create(fname);
        if (fd < 0) {
            st_fail("file create (st_root.txt)", "nos_create() returned -1 (no disk?)");
            file_ok = 0;
        } else {
            st_pass("file create (st_root.txt)");
            nos_close(fd);
        }

        /* 4. write known content, close, reopen, read back, compare */
        if (!file_ok) {
            st_fail("file write/read roundtrip", "skipped: create already failed");
        } else {
            int ok = 1;
            int wfd = nos_open(fname);
            if (wfd < 0) { st_fail("file write/read roundtrip", "nos_open() for write failed"); ok = 0; }
            if (ok && nos_write_file(wfd, content, clen) != 0) {
                st_fail("file write/read roundtrip", "nos_write_file() failed");
                ok = 0;
            }
            if (wfd >= 0) nos_close(wfd);

            if (ok) {
                int rfd = nos_open(fname);
                char rbuf[64];
                memset(rbuf, 0, sizeof(rbuf));
                int n = (rfd >= 0) ? nos_read(rfd, rbuf, sizeof(rbuf) - 1) : -1;
                if (rfd >= 0) nos_close(rfd);

                if (rfd < 0 || n != (int)clen || memcmp(rbuf, content, clen) != 0) {
                    st_fail("file write/read roundtrip", "content read back does not match what was written");
                    ok = 0;
                } else {
                    st_pass("file write/read roundtrip");
                }
            }
            if (!ok) file_ok = 0;
        }

        /* 5. re-create the SAME file (already exists) — regression test
           for the Phase 10 bug where a second create() on an existing
           name added a duplicate directory entry instead of reusing it.
           There's no directory-listing syscall that returns parsed
           entries (SYS_READDIR only prints via VGA), so this checks the
           next best observable symptom: re-creating must not fail, and
           the original content must still read back unchanged
           afterward (a duplicate/reset entry would show up as the file
           losing its content or fat16_create() erroring out). */
        if (!file_ok) {
            st_fail("file duplicate-create regression (Phase 10)", "skipped: an earlier file test already failed");
        } else {
            int fd2 = nos_create(fname);
            if (fd2 < 0) {
                st_fail("file duplicate-create regression (Phase 10)", "nos_create() on an existing file returned -1");
            } else {
                nos_close(fd2);
                int rfd2 = nos_open(fname);
                char rbuf2[64];
                memset(rbuf2, 0, sizeof(rbuf2));
                int n2 = (rfd2 >= 0) ? nos_read(rfd2, rbuf2, sizeof(rbuf2) - 1) : -1;
                if (rfd2 >= 0) nos_close(rfd2);

                if (rfd2 < 0 || n2 != (int)clen || memcmp(rbuf2, content, clen) != 0)
                    st_fail("file duplicate-create regression (Phase 10)",
                             "content changed/lost after re-create (possible duplicate entry)");
                else
                    st_pass("file duplicate-create regression (Phase 10)");
            }
        }
    }

    /* 6. Security: an obviously-invalid pointer must be rejected by the
       syscall, not crash the kernel. 0x1000 falls inside the kernel's
       shared 0-8MB identity map (kernel/memory/vmm.c: vmm_init() maps
       0-8MB with VMM_KERNEL, never VMM_USER, and every process's own
       directory clones those same two PDEs) — present in this
       process's page directory, but not VMM_USER, so
       user_ptr_valid()/vmm_get_user_phys_from_dir() (kernel/syscall.c,
       kernel/memory/vmm.h) must reject it. If this test doesn't even
       print its result, the kernel crashed instead of returning -1. */
    {
        int ret = nos_write(1, (const char *)0x1000, 4);
        if (ret == -1)
            st_pass("invalid pointer into kernel-only region (0x1000) rejected by syscall");
        else
            st_fail("invalid pointer into kernel-only region (0x1000) rejected by syscall",
                     "syscall did not return -1");
    }

    /* 7. PCI: enumeration should have found at least one device (every
       real or emulated x86 machine has at least a host bridge on
       bus 0). */
    {
        int count = nos_pci_list();
        if (count >= 1)
            st_pass("PCI enumeration found at least 1 device");
        else
            st_fail("PCI enumeration found at least 1 device", "device count == 0");
    }

    /* 8-11. Phase 15: FAT16 subdirectories. Everything below runs on
       one disposable test directory, named so it can't collide with
       anything a real user created (same convention as st_root.txt
       above). dir_ok gates the later steps the same way file_ok does
       for the file tests: if mkdir/cd itself fails, there's no point
       attempting anything that depends on it.

       IMPORTANT — 8.3 collisions: FAT16 truncates a name to 8 base
       characters + a 3-character extension, case-insensitively. Every
       filename here (and st_root.txt above) MUST have a distinct 8.3
       encoding from every other one used anywhere in this test suite
       — "selftest_root.txt"/"selftest_sub.txt"/"selftest_mark.txt"
       would all collide on the identical packed name "SELFTESTTXT"
       (same first 8 chars "selftest", same "txt" extension), which
       previously caused test 11 to see an unrelated ROOT-level file
       as a false "leak" of a subdirectory file — not an actual
       directory-scoping bug, dir_lookup() correctly scopes by
       dir_cluster; the two files just happened to share one 8.3
       identity. Kept short and distinct here specifically to avoid
       repeating that mistake. */
    {
        const char *dname     = "selftest_dir";
        const char *subfname  = "st_sub.txt";
        const char *subcontent = "NullOS selftest subdir data 5678\n";
        unsigned int subclen  = strlen(subcontent);
        const char *markname  = "st_mark.txt";
        int dir_ok = 1;

        /* 8. mkdir */
        if (nos_mkdir(dname) != 0) {
            st_fail("mkdir (selftest_dir)", "nos_mkdir() returned nonzero (no disk?)");
            dir_ok = 0;
        } else {
            st_pass("mkdir (selftest_dir)");
        }

        /* cd into it — gates every step below, same as dir_ok itself */
        if (dir_ok && nos_chdir(dname) != 0) {
            st_fail("cd into selftest_dir", "nos_chdir() returned nonzero right after a successful mkdir");
            dir_ok = 0;
        }

        /* 9. create + write + read roundtrip INSIDE the subdirectory —
           the exact scenario that exposed the exec()-doesn't-inherit-
           cwd bug during this phase's own manual test (there it was
           edit's process losing the cwd across exec(); here, this
           process never execs, so this specifically checks the FAT16/
           vfs side: that a relative create/write/read all land inside
           selftest_dir, not silently at the root). */
        if (!dir_ok) {
            st_fail("file write/read roundtrip inside selftest_dir", "skipped: mkdir/cd already failed");
        } else {
            int ok = 1;
            int fd = nos_create(subfname);
            if (fd < 0) { st_fail("file write/read roundtrip inside selftest_dir", "nos_create() failed"); ok = 0; }
            if (ok) nos_close(fd);

            int wfd = ok ? nos_open(subfname) : -1;
            if (ok && wfd < 0) { st_fail("file write/read roundtrip inside selftest_dir", "nos_open() for write failed"); ok = 0; }
            if (ok && nos_write_file(wfd, subcontent, subclen) != 0) {
                st_fail("file write/read roundtrip inside selftest_dir", "nos_write_file() failed");
                ok = 0;
            }
            if (wfd >= 0) nos_close(wfd);

            if (ok) {
                int rfd = nos_open(subfname);
                char rbuf[64];
                memset(rbuf, 0, sizeof(rbuf));
                int n = (rfd >= 0) ? nos_read(rfd, rbuf, sizeof(rbuf) - 1) : -1;
                if (rfd >= 0) nos_close(rfd);

                if (rfd < 0 || n != (int)subclen || memcmp(rbuf, subcontent, subclen) != 0)
                    st_fail("file write/read roundtrip inside selftest_dir",
                             "content read back does not match what was written");
                else
                    st_pass("file write/read roundtrip inside selftest_dir");
            }
        }

        /* 10. fork() while cwd == selftest_dir: the child creates a
           marker file via a RELATIVE path with no cd of its own — it
           only ends up inside selftest_dir if process_fork() actually
           copied cwd_cluster from the parent. The parent (still in
           selftest_dir) then opens that same relative name: finding it
           proves the child wrote to the same directory, i.e. that the
           inherited cwd matched. This is deliberately done in-process
           here (a real fork(), not exec()) rather than duplicating
           forktest.c's exec()-based marker-file approach — fork()'s
           cwd inheritance was never the bug this phase found (only
           exec()'s was; see docs/scheduler.md), but it's cheap to keep
           covered by an actual regression test here rather than only
           by forktest.c's separate, exec-launched check. */
        if (!dir_ok) {
            st_fail("fork() child inherits cwd_cluster", "skipped: mkdir/cd already failed");
        } else {
            int fret = nos_fork();
            if (fret == 0) {
                int mfd = nos_create(markname);
                if (mfd >= 0) nos_close(mfd);
                nos_exit(0);
            } else if (fret > 0) {
                nos_wait(fret);
                int mfd2 = nos_open(markname);
                if (mfd2 >= 0) {
                    nos_close(mfd2);
                    st_pass("fork() child inherits cwd_cluster (marker created by child found in selftest_dir)");
                } else {
                    st_fail("fork() child inherits cwd_cluster",
                             "marker file not found in selftest_dir after the child exited");
                }
            } else {
                st_fail("fork() child inherits cwd_cluster", "fork() returned a negative value");
            }
        }

        /* 11. cd back to the root and confirm NEITHER file created
           above leaked out of selftest_dir — this is the exact
           observable symptom the manual test caught (a file written
           inside a subdirectory showing up at the root instead). Both
           nos_open() calls are expected to FAIL (-1) here: there's no
           parsed-directory-listing syscall to check against (SYS_READDIR
           only prints via VGA), so "can't be opened by this name at the
           root" is the next best observable proof of isolation. */
        if (!dir_ok) {
            st_fail("subdirectory files do not leak into the root", "skipped: mkdir/cd already failed");
        } else if (nos_chdir("..") != 0) {
            st_fail("subdirectory files do not leak into the root", "nos_chdir(\"..\") back to the root failed");
        } else {
            int leaked_sub  = (nos_open(subfname) >= 0);
            int leaked_mark = (nos_open(markname) >= 0);
            if (leaked_sub || leaked_mark)
                st_fail("subdirectory files do not leak into the root",
                         "a file created inside selftest_dir was openable by name at the root");
            else
                st_pass("subdirectory files do not leak into the root");
        }
    }

    /* 13-14. Pipes (Phase 16): nos_pipe() gives BOTH ends to this same
       process, so the basic mechanics — write/read roundtrip and the
       symmetric EOF-on-writer-close protocol — are fully testable
       here without fork()/SYS_EXEC_PIPE. A real two-process pipeline
       genuinely needs a second, independent process on the other end
       (that's the whole point of SYS_EXEC_PIPE), so it isn't
       something a single-process automated test can meaningfully
       substitute for — see docs/pipes.md's "forktest | cat" manual
       walkthrough for that coverage instead. */
    {
        int fds[2];
        int pipe_ok = (nos_pipe(fds) == 0);
        if (!pipe_ok) st_fail("pipe write/read roundtrip", "nos_pipe() failed");

        /* 13. write known content, read it back from the other end */
        if (pipe_ok) {
            const char *msg = "hello through the pipe";
            unsigned int mlen = strlen(msg);
            if (nos_write(fds[1], msg, mlen) != (int)mlen) {
                st_fail("pipe write/read roundtrip", "nos_write() to the write end failed");
                pipe_ok = 0;
            } else {
                char rbuf[64];
                memset(rbuf, 0, sizeof(rbuf));
                int n = nos_read(fds[0], rbuf, sizeof(rbuf) - 1);
                if (n != (int)mlen || memcmp(rbuf, msg, mlen) != 0)
                    st_fail("pipe write/read roundtrip", "content read back does not match what was written");
                else
                    st_pass("pipe write/read roundtrip");
            }
        }

        /* 14. EOF: once the write end is closed (the LAST write
           reference — this process never forked or dup'd it), a read
           on the now-permanently-empty read end must return 0
           immediately instead of blocking forever. This is the same
           symmetric-close protocol docs/pipes.md describes
           (pipe_release_write() waking a blocked reader), just
           observed here on an already-empty pipe rather than caught
           mid-block. */
        if (!pipe_ok) {
            st_fail("pipe read returns EOF after writer closes", "skipped: pipe setup already failed");
        } else {
            nos_close(fds[1]);
            char eofbuf[8];
            int n = nos_read(fds[0], eofbuf, sizeof(eofbuf));
            if (n != 0)
                st_fail("pipe read returns EOF after writer closes", "read() did not return 0 after the write end closed");
            else
                st_pass("pipe read returns EOF after writer closes");
            nos_close(fds[0]);
        }
    }

    /* 15. Stream writes accumulate (Phase 17-C regression). SYS_WRITE
       stages a write in 128-byte chunks, and each chunk used to replace
       the WHOLE file, so only the last chunk survived a write over 128
       bytes. This writes 2100 bytes as two nos_write() calls (600 + 1500:
       several chunks each, and the total crosses the first 2048-byte
       cluster so the chain has to grow), then reads everything back. */
    {
        static char wbuf[2100];
        static char rbuf[2100 + 16];   /* a little spare, to notice extra data */
        const unsigned int total = sizeof(wbuf);
        const unsigned int first = 600;
        const char *name = "st_big.txt";
        const char *why  = 0;

        for (unsigned int i = 0; i < total; i++)
            wbuf[i] = (char)('a' + (i * 7) % 26);

        int fd = nos_create(name);
        if (fd < 0)
            why = "nos_create() returned -1 (no disk?)";
        else if (nos_write_file(fd, "", 0) != 0)
            why = "could not truncate the file before writing";
        else if (nos_write(fd, wbuf, first) != (int)first)
            why = "first nos_write() (600 bytes) failed";
        else if (nos_write(fd, wbuf + first, total - first) != (int)(total - first))
            why = "second nos_write() (1500 bytes) failed";
        if (fd >= 0) nos_close(fd);

        if (why) {
            st_fail("SYS_WRITE >128 bytes accumulates in a FAT16 file", why);
        } else {
            unsigned int got = 0;
            int rfd = nos_open(name);
            if (rfd >= 0) {
                for (;;) {
                    int r = nos_read(rfd, rbuf + got, (unsigned int)sizeof(rbuf) - got);
                    if (r <= 0) break;
                    got += (unsigned int)r;
                    if (got >= sizeof(rbuf)) break;
                }
                nos_close(rfd);
            }

            if (rfd < 0) {
                st_fail("SYS_WRITE >128 bytes accumulates in a FAT16 file",
                         "could not reopen the file");
            } else if (got != total) {
                static char msg[64];
                char nbuf[16];
                const char *p1 = "read back ";
                const char *num = nos_uitoa(got, nbuf, sizeof(nbuf));
                const char *p2 = " of 2100 bytes";
                unsigned int at = 0;
                memcpy(msg + at, p1, strlen(p1));   at += strlen(p1);
                memcpy(msg + at, num, strlen(num)); at += strlen(num);
                memcpy(msg + at, p2, strlen(p2) + 1);
                st_fail("SYS_WRITE >128 bytes accumulates in a FAT16 file", msg);
            } else if (memcmp(rbuf, wbuf, total) != 0) {
                st_fail("SYS_WRITE >128 bytes accumulates in a FAT16 file",
                         "size is right but the content differs");
            } else {
                st_pass("SYS_WRITE >128 bytes accumulates in a FAT16 file");
            }
        }
    }

    /* 16. Cleanup — not counted as PASS/FAIL, just a note: there is no
       delete/unlink/rmdir syscall yet, so st_root.txt, st_big.txt,
       selftest_dir/ (and the two files inside it) are left on disk. Harmless: the
       next run just re-creates/overwrites everything by the same names. */
    st_puts("[INFO] cleanup: no delete/unlink/rmdir syscall exists yet -"
            " st_root.txt, st_big.txt and selftest_dir/ (with its files) left on disk (harmless)\n");

    st_puts("Selftest: ");
    char nbuf[16];
    st_puts(nos_uitoa((unsigned int)g_tests_passed, nbuf, sizeof(nbuf)));
    st_puts("/");
    st_puts(nos_uitoa((unsigned int)g_tests_run, nbuf, sizeof(nbuf)));
    st_puts(" passed\n");

    nos_exit(0);
}
