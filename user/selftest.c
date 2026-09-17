/* nullos/user/selftest.c — automated regression test suite. Run with
   "run selftest" from the shell after any kernel change to get a quick
   PASS/FAIL readout instead of manually typing touch/edit/ls/fork by
   hand every time. Not a numbered phase — just a standalone diagnostic
   tool, and expected to grow as more subsystems get their own test. */

typedef unsigned int uint32_t;

/* ── syscall wrappers (see kernel/syscall.h for numbers/convention) ── */

static int sys_write(const char *buf, unsigned int len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(1), "b"(1), "c"(buf), "d"(len) : "memory");
    return ret;
}

static void sys_exit(int code) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(2), "b"(code) : "memory");
    (void)ret;
    for (;;);
}

static void sys_meminfo(uint32_t *pmm_pages, uint32_t *heap_bytes, uint32_t *nprocs) {
    __asm__ volatile ("int $0x80"
        : : "a"(7), "b"(pmm_pages), "c"(heap_bytes), "d"(nprocs) : "memory");
}

static int sys_fork(void) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(25) : "memory");
    return ret;
}

static void sys_wait(int pid) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(20), "b"(pid) : "memory");
    (void)ret;
}

static int sys_open(const char *name) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(11), "b"(name) : "memory");
    return ret;
}

static int sys_create(const char *name) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(23), "b"(name) : "memory");
    return ret;
}

static int sys_close(int fd) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(12), "b"(fd) : "memory");
    return ret;
}

static int sys_read_fd(int fd, char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(5), "b"(fd), "c"(buf), "d"(len) : "memory");
    return ret;
}

static int sys_write_file(int fd, const char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(22), "b"(fd), "c"(buf), "d"(len) : "memory");
    return ret;
}

/* Returns the PCI device count (see kernel/drivers/pci.c ->
   pci_device_count(), plumbed through SYS_PCI_LIST for this test —
   previously the syscall always returned 0). Also reprints the table
   via VGA, same as the shell's "lspci". */
static int sys_pci_list(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(24) : "memory");
    return ret;
}

/* ── tiny helpers (no libc) ───────────────────────────────────────── */

static unsigned int st_strlen(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

static void st_puts(const char *s) {
    sys_write(s, st_strlen(s));
}

static int st_bufeq(const char *a, const char *b, unsigned int n) {
    for (unsigned int i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

/* converts uint32 to decimal string; returns pointer into buf (not start) */
static char *st_uitoa(unsigned int v, char *buf, unsigned int bufsz) {
    buf[--bufsz] = '\0';
    if (v == 0) { buf[--bufsz] = '0'; return &buf[bufsz]; }
    while (v && bufsz > 0) {
        buf[--bufsz] = '0' + (v % 10);
        v /= 10;
    }
    return &buf[bufsz];
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
        sys_meminfo(&pmm, &heap, &nprocs);
        if (nprocs == 0)
            st_fail("memory: SYS_MEMINFO reports pmm/heap/process stats",
                     "nprocs == 0 (expected >= 1 for this process)");
        else
            st_pass("memory: SYS_MEMINFO reports pmm/heap/process stats");
    }

    /* 2. Process: fork(). The child exits immediately and silently so
       it doesn't re-run (and double-report) the rest of the suite. */
    {
        int ret = sys_fork();
        if (ret == 0) {
            sys_exit(0);
        } else if (ret > 0) {
            sys_wait(ret);   /* reap the child's process-table slot before continuing */
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
        const char *fname   = "selftest_tmp.txt";
        const char *content = "NullOS selftest data 1234\n";
        unsigned int clen   = st_strlen(content);
        int file_ok = 1;

        /* 3. creation */
        int fd = sys_create(fname);
        if (fd < 0) {
            st_fail("file create (selftest_tmp.txt)", "sys_create() returned -1 (no disk?)");
            file_ok = 0;
        } else {
            st_pass("file create (selftest_tmp.txt)");
            sys_close(fd);
        }

        /* 4. write known content, close, reopen, read back, compare */
        if (!file_ok) {
            st_fail("file write/read roundtrip", "skipped: create already failed");
        } else {
            int ok = 1;
            int wfd = sys_open(fname);
            if (wfd < 0) { st_fail("file write/read roundtrip", "sys_open() for write failed"); ok = 0; }
            if (ok && sys_write_file(wfd, content, clen) != 0) {
                st_fail("file write/read roundtrip", "sys_write_file() failed");
                ok = 0;
            }
            if (wfd >= 0) sys_close(wfd);

            if (ok) {
                int rfd = sys_open(fname);
                char rbuf[64];
                for (unsigned int i = 0; i < sizeof(rbuf); i++) rbuf[i] = 0;
                int n = (rfd >= 0) ? sys_read_fd(rfd, rbuf, sizeof(rbuf) - 1) : -1;
                if (rfd >= 0) sys_close(rfd);

                if (rfd < 0 || n != (int)clen || !st_bufeq(rbuf, content, clen)) {
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
            int fd2 = sys_create(fname);
            if (fd2 < 0) {
                st_fail("file duplicate-create regression (Phase 10)", "sys_create() on an existing file returned -1");
            } else {
                sys_close(fd2);
                int rfd2 = sys_open(fname);
                char rbuf2[64];
                for (unsigned int i = 0; i < sizeof(rbuf2); i++) rbuf2[i] = 0;
                int n2 = (rfd2 >= 0) ? sys_read_fd(rfd2, rbuf2, sizeof(rbuf2) - 1) : -1;
                if (rfd2 >= 0) sys_close(rfd2);

                if (rfd2 < 0 || n2 != (int)clen || !st_bufeq(rbuf2, content, clen))
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
        int ret = sys_write((const char *)0x1000, 4);
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
        int count = sys_pci_list();
        if (count >= 1)
            st_pass("PCI enumeration found at least 1 device");
        else
            st_fail("PCI enumeration found at least 1 device", "device count == 0");
    }

    /* 8. Cleanup — not counted as PASS/FAIL, just a note: there is no
       delete/unlink syscall yet, so selftest_tmp.txt is left on disk.
       Harmless: the next run just re-creates/overwrites it. */
    st_puts("[INFO] cleanup: no delete/unlink syscall exists yet -"
            " selftest_tmp.txt left on disk (harmless)\n");

    st_puts("Selftest: ");
    char nbuf[16];
    st_puts(st_uitoa((unsigned int)g_tests_passed, nbuf, sizeof(nbuf)));
    st_puts("/");
    st_puts(st_uitoa((unsigned int)g_tests_run, nbuf, sizeof(nbuf)));
    st_puts(" passed\n");

    sys_exit(0);
}
