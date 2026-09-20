/* nullos/user/lib/nullos.h — thin syscall wrapper library ("libnos").
 *
 * One function per syscall in kernel/syscall.h, doing nothing but the
 * "int $0x80" and returning the kernel's result — no added behavior,
 * no invented conventions. This exists so every user program stops
 * encoding "int 0x80" + a raw syscall number directly: before v1.0.0
 * the syscall interface is still free to change, and after it,
 * changing how a syscall works underneath only means recompiling this
 * one file, not every program that happens to call it. See
 * docs/kernel.md → "User-space syscall library (libnos)" for the full
 * rationale, and docs/syscalls.md for what each syscall actually does
 * (this header only gives the C signature, not the semantics).
 *
 * Naming: "nos_" (NullOS), chosen over "sys_" specifically to avoid
 * reading like the kernel's own internal sys_* handlers in
 * kernel/syscall.c (a different binary entirely — no actual linker
 * collision risk, just naming clarity).
 */
#ifndef NULLOS_LIB_H
#define NULLOS_LIB_H

#include <stdint.h>
#include <stddef.h>

/* SYS_EXIT — never returns. */
void nos_exit(int code);

/* SYS_WRITE / SYS_READ — real signature includes fd, unlike the
   fd-hardcoded-to-1/0 local wrappers every program used to have. */
int nos_write(int fd, const char *buf, unsigned len);
int nos_read(int fd, char *buf, unsigned len);

int      nos_yield(void);
uint32_t nos_getpid(void);
uint32_t nos_uptime(void);
void     nos_meminfo(uint32_t *pmm_pages, uint32_t *heap_bytes, uint32_t *nprocs);
void     nos_ps(void);
int      nos_kill(uint32_t pid);

/* SYS_EXEC — arg may be NULL ("no argument"), replacing the old
   sys_exec(name)/sys_exec_arg(name,arg) split; this is the syscall's
   real 2-argument signature. */
int nos_exec(const char *name, const char *arg);

int      nos_open(const char *name);
int      nos_close(int fd);
unsigned nos_read_raw(void);
void     nos_gotoxy(unsigned col, unsigned row);
void     nos_clear(void);
int      nos_getarg(char *buf, unsigned len);
void     nos_kbd_flush(void);
void     nos_setcolor(unsigned fg, unsigned bg);
void     nos_set_raw_mode(unsigned enable);
void     nos_wait(int pid);

/* SYS_READDIR — path may be NULL/empty for the caller's cwd. */
void nos_readdir(const char *path);

int nos_write_file(int fd, const char *buf, unsigned len);
int nos_create(const char *name);
int nos_pci_list(void);
int nos_fork(void);
int nos_chdir(const char *path);
int nos_mkdir(const char *path);

/* SYS_PIPE (Phase 16) — creates a pipe; fds[0] becomes the read end,
   fds[1] the write end, both plain fds usable with nos_read()/
   nos_write() like any other. Returns 0 on success, -1 on failure. */
int nos_pipe(int fds[2]);

/* SYS_EXEC_PIPE (Phase 16) — like nos_exec(name, 0), except the new
   process's fd 0/1 are redirected to the caller's already-open
   stdin_fd/stdout_fd. Pass -1 for either to leave that stream
   un-redirected (default keyboard/VGA). Used by the shell's
   "cmd1 | cmd2" — see docs/pipes.md. */
int nos_exec_pipe(const char *name, int stdin_fd, int stdout_fd);

/* SYS_GETCWD — writes the caller's current directory as an absolute path
   ("/", "/FOO/BAR", 8.3 uppercase names) into buf. Returns the path length,
   or -1 (buf too small, or the path couldn't be reconstructed). */
int nos_getcwd(char *buf, unsigned len);

/* SYS_PCI_FIND — 1 if a PCI device with this vendor/device ID was found
   during enumeration, 0 if not (used by selftest to check a specific device). */
int nos_pci_find(unsigned vendor, unsigned device);

/* SYS_REBOOT / SYS_SHUTDOWN — do not return on success; -1 if the request
   had no effect (the kernel prints why). */
int nos_reboot(void);
int nos_shutdown(void);

/* ── string / memory helpers ──────────────────────────────────────
   Standard libc names and signatures on purpose (not "nos_"-prefixed):
   GCC may itself emit calls to memcpy/memset/memmove (struct copies,
   zeroing loops) even with -ffreestanding, and only the standard names
   resolve those. Not syscall wrappers — plain user-space code, replacing
   the per-program copies that shell/forktest/selftest/edit each carried. */
void  *memcpy(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);

/* Converts v to decimal, filling buf from the END; returns a pointer to
   the first digit (inside buf, NOT buf itself). bufsz includes the
   terminating '\0'. Prefixed because nothing implicit ever calls it. */
char *nos_uitoa(uint32_t v, char *buf, unsigned bufsz);

#endif
