/* nullos/user/lib/nullos.c — thin syscall wrapper library ("libnos").
 * See nullos.h for the rationale. Every function below does exactly
 * one "int $0x80" with the matching syscall number from
 * kernel/syscall.h and returns whatever the kernel put in eax — no
 * added logic, no retries, no argument massaging.
 *
 * Every function, including void ones, captures the kernel's return
 * value into a local via "=a"(ret) even when it's discarded — this
 * keeps the compiler from assuming eax survives the "int $0x80"
 * unclobbered and reusing it as an argument register for a
 * subsequent syscall (the same convention user/edit.c's wrappers
 * already documented and relied on before this file existed).
 */
#include "nullos.h"
#include "syscall.h"

void nos_exit(int code) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_EXIT), "b"(code) : "memory");
    (void)ret;
    for (;;);
}

int nos_write(int fd, const char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len) : "memory");
    return ret;
}

int nos_read(int fd, char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_READ), "b"(fd), "c"(buf), "d"(len) : "memory");
    return ret;
}

int nos_yield(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_YIELD) : "memory");
    return ret;
}

uint32_t nos_getpid(void) {
    uint32_t ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_GETPID) : "memory");
    return ret;
}

uint32_t nos_uptime(void) {
    uint32_t ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_UPTIME) : "memory");
    return ret;
}

void nos_meminfo(uint32_t *pmm_pages, uint32_t *heap_bytes, uint32_t *nprocs) {
    uint32_t ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_MEMINFO), "b"(pmm_pages), "c"(heap_bytes), "d"(nprocs) : "memory");
    (void)ret;
}

void nos_ps(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_PS) : "memory");
    (void)ret;
}

int nos_kill(uint32_t pid) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_KILL), "b"(pid) : "memory");
    return ret;
}

int nos_exec(const char *name, const char *arg) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_EXEC), "b"(name), "c"(arg) : "memory");
    return ret;
}

int nos_open(const char *name) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_OPEN), "b"(name) : "memory");
    return ret;
}

int nos_close(int fd) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_CLOSE), "b"(fd) : "memory");
    return ret;
}

unsigned nos_read_raw(void) {
    unsigned ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_READ_RAW) : "memory");
    return ret;
}

void nos_gotoxy(unsigned col, unsigned row) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_GOTOXY), "b"(col), "c"(row) : "memory");
    (void)ret;
}

void nos_clear(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_CLEAR) : "memory");
    (void)ret;
}

int nos_getarg(char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_GETARG), "b"(buf), "c"(len) : "memory");
    return ret;
}

void nos_kbd_flush(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_KBD_FLUSH) : "memory");
    (void)ret;
}

void nos_setcolor(unsigned fg, unsigned bg) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_SETCOLOR), "b"(fg), "c"(bg) : "memory");
    (void)ret;
}

void nos_set_raw_mode(unsigned enable) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_SET_RAW_MODE), "b"(enable) : "memory");
    (void)ret;
}

void nos_wait(int pid) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_WAIT), "b"(pid) : "memory");
    (void)ret;
}

void nos_readdir(const char *path) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_READDIR), "b"(path) : "memory");
    (void)ret;
}

int nos_write_file(int fd, const char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_WRITE_FILE), "b"(fd), "c"(buf), "d"(len) : "memory");
    return ret;
}

int nos_create(const char *name) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_CREATE), "b"(name) : "memory");
    return ret;
}

int nos_pci_list(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_PCI_LIST) : "memory");
    return ret;
}

int nos_fork(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_FORK) : "memory");
    return ret;
}

int nos_chdir(const char *path) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_CHDIR), "b"(path) : "memory");
    return ret;
}

int nos_mkdir(const char *path) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_MKDIR), "b"(path) : "memory");
    return ret;
}

int nos_pipe(int fds[2]) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_PIPE), "b"(fds) : "memory");
    return ret;
}

int nos_exec_pipe(const char *name, int stdin_fd, int stdout_fd) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_EXEC_PIPE), "b"(name), "c"(stdin_fd), "d"(stdout_fd) : "memory");
    return ret;
}

int nos_getcwd(char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_GETCWD), "b"(buf), "c"(len) : "memory");
    return ret;
}

int nos_pci_find(unsigned vendor, unsigned device) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_PCI_FIND), "b"(vendor), "c"(device) : "memory");
    return ret;
}

int nos_reboot(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_REBOOT) : "memory");
    return ret;
}

int nos_shutdown(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(SYS_SHUTDOWN) : "memory");
    return ret;
}

/* ── string / memory helpers (see nullos.h) ───────────────────────
   memcpy/memset/memmove use rep movsb / rep stosb instead of C loops:
   a plain byte loop can be recognized by GCC's loop-idiom pass and
   turned back into a call to the very function being defined
   (infinite recursion). The direction flag is clear on entry per the
   i386 ABI; memmove sets it for the backward copy and clears it again. */

void *memcpy(void *dst, const void *src, size_t n) {
    void *d = dst;
    __asm__ volatile ("rep movsb"
        : "+D"(d), "+S"(src), "+c"(n) : : "memory");
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    void *d = dst;
    __asm__ volatile ("rep stosb"
        : "+D"(d), "+c"(n) : "a"(c) : "memory");
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || n == 0) return dst;
    if (d < s || d >= s + n) {
        return memcpy(dst, src, n);          /* no harmful overlap: forward copy */
    }
    /* overlapping with dst above src: copy backward */
    d += n - 1;
    s += n - 1;
    __asm__ volatile ("std; rep movsb; cld"
        : "+D"(d), "+S"(s), "+c"(n) : : "memory", "cc");
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n > 0 && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

char *nos_uitoa(uint32_t v, char *buf, unsigned bufsz) {
    buf[--bufsz] = '\0';
    if (v == 0) { buf[--bufsz] = '0'; return &buf[bufsz]; }
    while (v && bufsz > 0) {
        buf[--bufsz] = (char)('0' + (v % 10));
        v /= 10;
    }
    return &buf[bufsz];
}
