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

/* True if the caller's current directory is exactly `want`
   ("/" or e.g. "/ST_D1/ST_D2" — 8.3 uppercase, see SYS_GETCWD). */
static int st_cwd_is(const char *want) {
    char b[64];
    int n = nos_getcwd(b, sizeof(b));
    return n >= 0 && strcmp(b, want) == 0;
}

/* Builds "C<idx>:<pid>\n" into out (>= 32 bytes) and returns its length.
   Used by the multi-child waitpid test: each child reports this string
   about itself, and the parent rebuilds the expected one from the pid
   fork() returned. */
static unsigned int st_child_result(char *out, unsigned int idx, unsigned int pid) {
    char nbuf[16];
    char *num = nos_uitoa(pid, nbuf, sizeof(nbuf));
    unsigned int at = 0;
    out[at++] = 'C';
    out[at++] = (char)('0' + idx);
    out[at++] = ':';
    while (*num) out[at++] = *num++;
    out[at++] = '\n';
    out[at] = '\0';
    return at;
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

    /* 7b. PCI: a SPECIFIC, stable device — the Intel 440FX host bridge
       (vendor 0x8086, device 0x1237) that QEMU's default "pc" machine
       always has, and that already shows up in the boot log.

       ###################################################################
       # WARNING — THIS TEST WILL (EXPECTEDLY) BREAK IN PHASE 24 (AHCI). #
       # Phase 24 switches QEMU to `-machine q35`, whose host bridge is  #
       # a different chip (Intel 82G33, 8086:29c0), so 8086:1237 is no   #
       # longer enumerated. That is NOT a kernel bug: whoever does       #
       # Phase 24 must update the vendor/device IDs below (see also      #
       # docs/TODO.md). The generic ">= 1 device" test above stays valid.#
       ################################################################### */
    {
        if (nos_pci_find(0x8086, 0x1237))
            st_pass("PCI: Intel 440FX host bridge (8086:1237) present [QEMU -machine pc only]");
        else
            st_fail("PCI: Intel 440FX host bridge (8086:1237) present [QEMU -machine pc only]",
                     "not found (expected if QEMU no longer runs -machine pc, e.g. Phase 24's q35)");
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

    /* 16. Two-process pipeline, automated (Phase 17-D). Until now
       "forktest | cat" was only ever run by hand in the shell; the pipe
       tests above use both ends inside ONE process. Here the two ends
       really belong to different processes:
           writer  = a fork()ed child (inherits the pipe fds via vfs_dup)
           reader  = "cat", launched with SYS_EXEC_PIPE, stdin <- pipe 1,
                     stdout -> pipe 2
           parent  = reads pipe 2 to EOF and compares with what the writer sent.
       EOF only arrives once EVERY copy of a write end is closed, so each
       process closes what it doesn't use (this is the same discipline
       docs/pipes.md describes for the shell). */
    {
        const char *tname = "two-process pipe (fork writer -> exec cat -> parent)";
        const char *msg   = "NullOS two-process pipe test 4242\n";
        unsigned int mlen = strlen(msg);
        int p1[2] = { -1, -1 }, p2[2] = { -1, -1 };
        int wpid = -1, cpid = -1;
        const char *why = 0;

        if (nos_pipe(p1) != 0) {
            why = "nos_pipe() #1 failed";
        } else if (nos_pipe(p2) != 0) {
            why = "nos_pipe() #2 failed";
            nos_close(p1[0]); nos_close(p1[1]);
        } else {
            wpid = nos_fork();
            if (wpid == 0) {
                /* writer child: keep only p1's write end */
                nos_close(p1[0]); nos_close(p2[0]); nos_close(p2[1]);
                nos_write(p1[1], msg, mlen);
                nos_close(p1[1]);
                nos_exit(0);
            }
            if (wpid < 0) {
                why = "fork() for the writer failed";
            } else {
                cpid = nos_exec_pipe("cat", p1[0], p2[1]);
                if (cpid < 0) why = "nos_exec_pipe(\"cat\") failed";
            }
            /* the parent's own copies must go before reading to EOF */
            nos_close(p1[0]); nos_close(p1[1]); nos_close(p2[1]);

            if (!why) {
                static char rbuf[128];
                unsigned int got = 0;
                memset(rbuf, 0, sizeof(rbuf));
                for (;;) {
                    int r = nos_read(p2[0], rbuf + got, (unsigned)sizeof(rbuf) - 1 - got);
                    if (r <= 0) break;   /* 0 = EOF: writer and cat are both done */
                    got += (unsigned int)r;
                    if (got >= sizeof(rbuf) - 1) break;
                }
                if (got != mlen || memcmp(rbuf, msg, mlen) != 0)
                    why = "data read from the pipeline output differs from what the writer sent";
            }
            nos_close(p2[0]);
            if (wpid > 0) nos_wait(wpid);
            if (cpid > 0) nos_wait(cpid);
        }

        if (why) st_fail(tname, why);
        else     st_pass(tname);
    }

    /* 17. waitpid with several children (Phase 17-D). fork() three
       children; child i yields a DIFFERENT number of times (child 0 the
       longest) so they finish in the opposite order to how they were
       forked, then reports "C<i>:<its own pid>" through its own pipe.
       The parent waits for the SLOWEST child first, then the others, and
       for each specific pid checks that (a) the pid is really gone
       afterwards (SYS_KILL on it fails), and (b) the result read from
       THAT child's pipe is the one built from THAT pid — not just "all
       three ended". (There is no exit-code syscall yet, hence a pipe.) */
    {
        enum { NCH = 3 };
        const char *tname = "waitpid with 3 children: each pid collected with its own result";
        int pfd[NCH][2];
        int cpid[NCH];
        const char *why = 0;
        int i, j, forked = 0;

        for (i = 0; i < NCH; i++) { pfd[i][0] = pfd[i][1] = -1; cpid[i] = -1; }

        for (i = 0; i < NCH && !why; i++)
            if (nos_pipe(pfd[i]) != 0) why = "nos_pipe() failed";

        for (i = 0; i < NCH && !why; i++) {
            int r = nos_fork();
            if (r == 0) {
                char m[32];
                unsigned int ml = st_child_result(m, (unsigned)i, nos_getpid());
                for (j = 0; j < NCH; j++) {
                    nos_close(pfd[j][0]);
                    if (j != i) nos_close(pfd[j][1]);
                }
                for (j = 0; j < (NCH - i) * 3; j++) nos_yield();
                nos_write(pfd[i][1], m, ml);
                nos_close(pfd[i][1]);
                nos_exit(0);
            }
            if (r < 0) { why = "fork() failed"; break; }
            cpid[i] = r;
            forked++;
        }

        /* the parent's write ends are never used */
        for (i = 0; i < NCH; i++) if (pfd[i][1] >= 0) nos_close(pfd[i][1]);

        if (!why) {
            static const int order[NCH] = { 0, 2, 1 };   /* slowest child first */
            for (j = 0; j < NCH && !why; j++) {
                int k = order[j];
                char want[32], got[32];
                unsigned int wl = st_child_result(want, (unsigned)k, (unsigned)cpid[k]);
                nos_wait(cpid[k]);
                if (nos_kill((uint32_t)cpid[k]) != -1) {
                    why = "a pid is still alive after nos_wait() returned for it";
                    break;
                }
                memset(got, 0, sizeof(got));
                int n = nos_read(pfd[k][0], got, sizeof(got) - 1);
                if (n != (int)wl || memcmp(got, want, wl) != 0)
                    why = "a child's result does not match its own pid";
            }
        }

        for (i = 0; i < NCH; i++) if (pfd[i][0] >= 0) nos_close(pfd[i][0]);
        if (why) {   /* don't leave children behind on failure */
            for (i = 0; i < forked; i++) nos_wait(cpid[i]);
            st_fail(tname, why);
        } else {
            st_pass(tname);
        }
    }

    /* 18. mkdir/cd three levels deep (Phase 17-D): /ST_D1/ST_D2/ST_D3.
       Checks pwd after every step, a file created and read at the
       deepest level, that a multi-component path from the root resolves
       to it, and that "cd .." walks back up to "/" one level at a time.
       Directory names are 8.3-distinct from every other name here.
       Assumes the earlier tests left cwd at the root. */
    {
        const char *tname   = "mkdir/cd 3 levels deep, file at the bottom, cd .. back to /";
        const char *content = "NullOS selftest deep data 9012\n";
        unsigned int clen   = strlen(content);
        const char *why     = 0;
        int depth = 0;   /* successful cd's below the root, for cleanup */

        static const char *dirs[3]  = { "st_d1", "st_d2", "st_d3" };
        static const char *paths[4] = { "/", "/ST_D1", "/ST_D1/ST_D2", "/ST_D1/ST_D2/ST_D3" };

        if (!st_cwd_is("/")) why = "cwd is not the root before the test starts";

        for (int i = 0; i < 3 && !why; i++) {
            if (nos_mkdir(dirs[i]) != 0)      why = "nos_mkdir() failed";
            else if (nos_chdir(dirs[i]) != 0) why = "nos_chdir() into the new directory failed";
            else {
                depth++;
                if (!st_cwd_is(paths[i + 1])) why = "pwd is wrong after cd into a level";
            }
        }

        if (!why) {
            int fd = nos_create("st_deep.txt");
            if (fd < 0) why = "nos_create() at the deepest level failed";
            else {
                nos_close(fd);
                int wfd = nos_open("st_deep.txt");
                if (wfd < 0 || nos_write_file(wfd, content, clen) != 0)
                    why = "writing the file at the deepest level failed";
                if (wfd >= 0) nos_close(wfd);
            }
        }
        if (!why) {
            char rbuf[64];
            memset(rbuf, 0, sizeof(rbuf));
            int rfd = nos_open("st_deep.txt");
            int n = (rfd >= 0) ? nos_read(rfd, rbuf, sizeof(rbuf) - 1) : -1;
            if (rfd >= 0) nos_close(rfd);
            if (n != (int)clen || memcmp(rbuf, content, clen) != 0)
                why = "content read back at the deepest level does not match";
        }

        while (depth > 0) {   /* also the failure-path cleanup: always end at the root */
            if (nos_chdir("..") != 0) { if (!why) why = "nos_chdir(\"..\") failed"; break; }
            depth--;
            if (!why && !st_cwd_is(paths[depth])) why = "pwd is wrong after cd ..";
        }

        if (!why) {   /* from the root, by a multi-component relative path */
            char rbuf[64];
            memset(rbuf, 0, sizeof(rbuf));
            int rfd = nos_open("st_d1/st_d2/st_d3/st_deep.txt");
            int n = (rfd >= 0) ? nos_read(rfd, rbuf, sizeof(rbuf) - 1) : -1;
            if (rfd >= 0) nos_close(rfd);
            if (n != (int)clen || memcmp(rbuf, content, clen) != 0)
                why = "the file is not reachable by its 3-component path from the root";
            else if (nos_open("st_deep.txt") >= 0)
                why = "the deep file leaked into the root";
        }

        if (why) st_fail(tname, why);
        else     st_pass(tname);
    }

    /* 19. Cleanup — not counted as PASS/FAIL, just a note: there is no
       delete/unlink/rmdir syscall yet, so st_root.txt, st_big.txt,
       selftest_dir/ (and the two files inside it) and st_d1/st_d2/st_d3/ (with st_deep.txt) are left on disk. Harmless: the
       next run just re-creates/overwrites everything by the same names. */
    st_puts("[INFO] cleanup: no delete/unlink/rmdir syscall exists yet -"
            " st_root.txt, st_big.txt, selftest_dir/ and st_d1/ (with their files) left on disk (harmless)\n");

    st_puts("Selftest: ");
    char nbuf[16];
    st_puts(nos_uitoa((unsigned int)g_tests_passed, nbuf, sizeof(nbuf)));
    st_puts("/");
    st_puts(nos_uitoa((unsigned int)g_tests_run, nbuf, sizeof(nbuf)));
    st_puts(" passed\n");

    nos_exit(0);
}
