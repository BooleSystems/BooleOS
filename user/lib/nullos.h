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

#endif
