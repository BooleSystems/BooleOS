// nullos/kernel/syscall.c
#include "serial.h"
#include "syscall.h"
#include "process.h"
#include "scheduler.h"
#include "keyboard.h"
#include "timer.h"
#include "drivers/vga.h"
#include "memory/pmm.h"
#include "memory/heap.h"
#include "exec.h"
#include "memory/vmm.h"
#include "fs/vfs.h"
#include "fs/fat16.h"
#include "ramfs.h"
#include "drivers/pci.h"
#include <stdint.h>

/* ── file descriptor table ─────────────────────────────────── */

#define FD_PER_PROC  8
#define FD_BASE      3   /* 0=stdin,1=stdout,2=stderr reserved */

/* indexed by [process slot][local fd] */
static vfs_fd_t fd_table[PROCESS_MAX][FD_PER_PROC];

/* returns the current process's slot in the process table, or -1 */
static int proc_slot(void) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (process_at(i) == process_current())
            return (int)i;
    }
    return -1;
}

/* returns p's slot in the process table, or -1 */
static int slot_of(process_t *p) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (process_at(i) == p)
            return (int)i;
    }
    return -1;
}

/* ── SYS_FORK support ──────────────────────────────────────────
   Set by isr128 (see isr.asm) right after 'pusha', to the ESP value
   at that point — a pointer to a 13-word block: the 8 pusha
   registers followed by the CPU's ring3->ring0 trap frame (eip, cs,
   eflags, user_esp, user_ss). That's everything needed to resume the
   interrupted ring-3 execution via 'popa; iret', which is exactly
   what process_fork() fabricates for a new child's first scheduling.

   IMPORTANT: this global is only valid synchronously, for the
   syscall currently being dispatched. If sys_fork() below used it
   directly during the (potentially preempted, possibly slow) page
   copy in process_fork(), a different process's syscall entry could
   overwrite it in the meantime, corrupting the child being built.
   sys_fork() copies it into a LOCAL buffer as its very first action,
   before anything else, and nothing beyond that point — in this file
   or elsewhere — should read g_syscall_frame again for this call. */
uint32_t g_syscall_frame = 0;

#define SYSCALL_FRAME_WORDS 13
#define SYSCALL_FRAME_EAX   7   /* index of the saved EAX within the block above */

static uint32_t sys_fork(void) {
    /* snapshot FIRST — see the comment on g_syscall_frame above */
    uint32_t frame[SYSCALL_FRAME_WORDS];
    const uint32_t *src = (const uint32_t *)g_syscall_frame;
    for (int i = 0; i < SYSCALL_FRAME_WORDS; i++) frame[i] = src[i];
    frame[SYSCALL_FRAME_EAX] = 0;   /* the child sees fork() return 0 */

    process_t *parent = process_current();
    if (!parent) return (uint32_t)-1;
    int parent_slot = proc_slot();
    if (parent_slot < 0) return (uint32_t)-1;

    process_t *child = process_fork(parent, frame);
    if (!child) return (uint32_t)-1;

    /* duplicate the parent's open files into the child's own slot */
    int child_slot = slot_of(child);
    if (child_slot >= 0) {
        for (uint32_t j = 0; j < FD_PER_PROC; j++)
            fd_table[child_slot][j] = fd_table[parent_slot][j];
    }

    return child->pid;
}

static uint32_t sys_write(uint32_t fd, const char *buf, uint32_t len) {
    (void)fd;  // stdout only for now
    if (!buf) return (uint32_t)-1;
    for (uint32_t i = 0; i < len; i++)
        vga_putchar(buf[i]);
    return len;
}

static uint32_t sys_exit(uint32_t code) {
    (void)code;
    int slot = proc_slot();
    if (slot >= 0) {
        for (uint32_t j = 0; j < FD_PER_PROC; j++)
            fd_table[slot][j].used = 0;
    }
    process_t *p = process_current();
    if (p) process_exit(p);
    scheduler_yield();
    for (;;) __asm__ volatile ("hlt");
    return 0;
}

static uint32_t sys_yield(void) {
    scheduler_yield();
    return 0;
}

static uint32_t sys_getpid(void) {
    process_t *p = process_current();
    return p ? p->pid : 0;
}

/* PID of the process in raw mode (no keyboard echo); -1 = none */
static int raw_mode_pid = -1;

static uint32_t sys_read(uint32_t fd, char *buf, uint32_t len) {
    if (!buf || len == 0) return (uint32_t)-1;

    /* fd >= FD_BASE: file read via VFS */
    if (fd >= FD_BASE) {
        int slot = proc_slot();
        if (slot < 0) return (uint32_t)-1;
        uint32_t idx = fd - FD_BASE;
        if (idx >= FD_PER_PROC) return (uint32_t)-1;
        vfs_fd_t *f = &fd_table[slot][idx];
        if (!f->used) return (uint32_t)-1;
        int r = vfs_read(f, buf, len);
        return (r < 0) ? (uint32_t)-1 : (uint32_t)r;
    }

    /* fd == 0: keyboard */
    if (fd != 0) return (uint32_t)-1;

    uint32_t n = 0;
    while (n < len) {
        int c;
        while ((c = keyboard_getchar_nowait()) == -1)
            scheduler_sleep_current(1);

        if (c == 0x03) {
            vga_puts("^C\n");
            buf[0] = 0x03;
            return 1;
        }

        int in_raw = (raw_mode_pid >= 0 &&
                      process_current() &&
                      (int)process_current()->pid == raw_mode_pid);
        if (!in_raw)
            vga_putchar((char)c);   /* echo */

        if (c == '\b') {
            if (n > 0) n--;     /* backspace: discards the last char */
            continue;
        }

        buf[n++] = (char)c;

        if (c == '\n')
            break;
    }
    return n;
}

static uint32_t sys_uptime(void) {
    return timer_get_ticks();
}

static uint32_t sys_meminfo(uint32_t *pmm_out, uint32_t *heap_out, uint32_t *procs_out) {
    if (pmm_out)   *pmm_out   = pmm_free_pages();
    if (heap_out)  *heap_out  = heap_free_bytes();
    if (procs_out) {
        uint32_t n = 0;
        for (uint32_t i = 0; i < PROCESS_MAX; i++) {
            process_t *p = process_at(i);
            if (p && p->state != PROCESS_UNUSED) n++;
        }
        *procs_out = n;
    }
    return 0;
}

static uint32_t sys_ps(void) {
    process_dump();
    return 0;
}

/* argument passed by the last SYS_EXEC (e.g. filename for the editor) */
static char exec_arg[64];

#define USER_STR_MAX 64

/* resolves a byte in cur's virtual address space to its identity-mapped physical address */
static char *user_kptr(process_t *cur, uint32_t uaddr) {
    uint32_t phys = vmm_get_phys_from_dir(cur->cr3, uaddr);
    if (!phys) return (char *)0;
    return (char *)((phys & ~0xFFFu) | (uaddr & 0xFFFu));
}

/* copies a string from the user's virtual address into a kernel buf */
static int copy_user_str(process_t *cur, uint32_t uaddr, char *buf, uint32_t maxlen) {
    uint32_t i;
    for (i = 0; i < maxlen - 1; i++) {
        char *kp = user_kptr(cur, uaddr + i);
        if (!kp) return -1;
        char c = *kp;
        buf[i] = c;
        if (c == '\0') break;
    }
    buf[i] = '\0';
    return 0;
}

static uint32_t sys_open(const char *user_name) {
    serial_putchar('O'); serial_putchar('P'); serial_putchar('N'); serial_putchar('\n');
    if (!user_name) { serial_putchar('A'); serial_putchar('\n'); return (uint32_t)-1; }
    process_t *cur = process_current();
    if (!cur) { serial_putchar('B'); serial_putchar('\n'); return (uint32_t)-1; }
    int slot = proc_slot();
    if (slot < 0) { serial_putchar('C'); serial_putchar('\n'); return (uint32_t)-1; }

    char kname[USER_STR_MAX];
    if (copy_user_str(cur, (uint32_t)user_name, kname, USER_STR_MAX) < 0) {
        serial_putchar('D'); serial_putchar('\n'); return (uint32_t)-1;
    }

    /* find a free slot */
    for (uint32_t j = 0; j < FD_PER_PROC; j++) {
        if (!fd_table[slot][j].used) {
            if (vfs_open(kname, &fd_table[slot][j]) < 0) {
                serial_putchar('E'); serial_putchar('\n'); return (uint32_t)-1;
            }
            return FD_BASE + j;
        }
    }
    serial_putchar('F'); serial_putchar('\n');
    return (uint32_t)-1;  /* no free slots */
}

static uint32_t sys_create(const char *user_name) {
    if (!user_name) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    int slot = proc_slot();
    if (slot < 0) return (uint32_t)-1;

    char kname[USER_STR_MAX];
    if (copy_user_str(cur, (uint32_t)user_name, kname, USER_STR_MAX) < 0)
        return (uint32_t)-1;

    for (uint32_t j = 0; j < FD_PER_PROC; j++) {
        if (!fd_table[slot][j].used) {
            if (vfs_create(kname, &fd_table[slot][j]) < 0)
                return (uint32_t)-1;
            return FD_BASE + j;
        }
    }
    return (uint32_t)-1;  /* no free slots */
}

static uint32_t sys_close(uint32_t fd) {
    if (fd < FD_BASE) return (uint32_t)-1;
    int slot = proc_slot();
    if (slot < 0) return (uint32_t)-1;
    uint32_t idx = fd - FD_BASE;
    if (idx >= FD_PER_PROC) return (uint32_t)-1;
    if (!fd_table[slot][idx].used) return (uint32_t)-1;
    vfs_close(&fd_table[slot][idx]);
    return 0;
}

static uint32_t sys_exec(const char *user_name, uint32_t user_arg) {
    if (!user_name) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;

    char kname[USER_STR_MAX];
    if (copy_user_str(cur, (uint32_t)user_name, kname, USER_STR_MAX) < 0)
        return (uint32_t)-1;

    /* copies the optional argument (e.g. filename for the editor) */
    exec_arg[0] = '\0';
    if (user_arg)
        copy_user_str(cur, user_arg, exec_arg, sizeof(exec_arg));

    process_t *p = exec(kname);
    if (!p) return (uint32_t)-1;
    return p->pid;
}

static uint32_t sys_getarg(char *user_buf, uint32_t len) {
    if (!user_buf || len == 0) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    uint32_t n = 0;
    while (n < len - 1 && exec_arg[n]) {
        char *kp = user_kptr(cur, (uint32_t)user_buf + n);
        if (!kp) return (uint32_t)-1;
        *kp = exec_arg[n];
        n++;
    }
    char *kp = user_kptr(cur, (uint32_t)user_buf + n);
    if (kp) *kp = '\0';
    return n;
}

static void puts_padded(const char *s, int width) {
    int n = 0;
    while (s[n]) { vga_putchar(s[n++]); }
    while (n++ < width) vga_putchar(' ');
}

static uint32_t sys_readdir(void) {
    int any = 0;

    /* ramfs */
    if (ramfs_base) {
        vga_puts("ramfs:\n");
        /* accesses n_entries and entries directly via ramfs_h */
        uint32_t n = *(uint32_t *)ramfs_base;
        ramfs_entry_t *entries = (ramfs_entry_t *)(ramfs_base + sizeof(uint32_t));
        for (uint32_t i = 0; i < n; i++) {
            vga_puts("  ");
            puts_padded(entries[i].name, 20);
            vga_putdec(entries[i].size);
            vga_puts(" B\n");
        }
        any = 1;
    }

    /* FAT16 */
    if (fat16_available()) {
        vga_puts("fat16:\n");
        char name[13];
        uint32_t size;
        for (uint32_t idx = 0; fat16_readdir(idx, name, &size); idx++) {
            vga_puts("  ");
            puts_padded(name, 20);
            vga_putdec(size);
            vga_puts(" B\n");
            any = 1;
        }
    }

    if (!any) vga_puts("(no files)\n");
    return 0;
}

static uint32_t sys_set_raw_mode(uint32_t enable) {
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    if (enable)
        raw_mode_pid = (int)cur->pid;
    else if (raw_mode_pid == (int)cur->pid)
        raw_mode_pid = -1;
    return 0;
}

static uint32_t sys_wait(uint32_t pid) {
    for (;;) {
        int found = 0;
        for (uint32_t i = 0; i < PROCESS_MAX; i++) {
            process_t *p = process_at(i);
            if (p && p->pid == pid && p->state != PROCESS_UNUSED) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
        scheduler_sleep_current(10);
    }
}

static uint32_t sys_kill(uint32_t pid) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        process_t *p = process_at(i);
        if (p && p->pid == pid && p->state != PROCESS_UNUSED) {
            process_exit(p);
            return 0;
        }
    }
    return (uint32_t)-1;
}

static uint32_t sys_write_file(uint32_t fd, uint32_t user_buf, uint32_t len) {
    if (fd < FD_BASE) return (uint32_t)-1;
    int slot = proc_slot();
    if (slot < 0) return (uint32_t)-1;
    uint32_t idx = fd - FD_BASE;
    if (idx >= FD_PER_PROC) return (uint32_t)-1;
    vfs_fd_t *f = &fd_table[slot][idx];
    if (!f->used || f->backend != VFS_FAT16) return (uint32_t)-1;

    if (len == 0) return (uint32_t)vfs_write(f, (const char *)0, 0);

    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;

    /* copies buf from userland into the kernel (heap) */
    char *kbuf = (char *)kmalloc(len);
    if (!kbuf) return (uint32_t)-1;

    for (uint32_t i = 0; i < len; i++) {
        char *kp = user_kptr(cur, user_buf + i);
        if (!kp) { kfree(kbuf); return (uint32_t)-1; }
        kbuf[i] = *kp;
    }

    int r = vfs_write(f, kbuf, len);
    kfree(kbuf);
    return (r < 0) ? (uint32_t)-1 : 0;
}

uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    switch (num) {
        case SYS_WRITE:   return sys_write(arg1, (const char *)arg2, arg3);
        case SYS_EXIT:    return sys_exit(arg1);
        case SYS_YIELD:   return sys_yield();
        case SYS_GETPID:  return sys_getpid();
        case SYS_READ:    return sys_read(arg1, (char *)arg2, arg3);
        case SYS_UPTIME:  return sys_uptime();
        case SYS_MEMINFO: return sys_meminfo((uint32_t *)arg1, (uint32_t *)arg2, (uint32_t *)arg3);
        case SYS_PS:      return sys_ps();
        case SYS_KILL:    return sys_kill(arg1);
        case SYS_EXEC:    return sys_exec((const char *)arg1, arg2);
        case SYS_OPEN:     return sys_open((const char *)arg1);
        case SYS_CLOSE:    return sys_close(arg1);
        case SYS_READ_RAW: {
            int r;
            while ((r = keyboard_raw_nowait()) == -1)
                scheduler_sleep_current(1);
            return (uint32_t)r;
        }
        case SYS_GOTOXY:    vga_set_cursor((uint8_t)arg1, (uint8_t)arg2); return 0;
        case SYS_CLEAR:     vga_clear(); return 0;
        case SYS_GETARG:    return sys_getarg((char *)arg1, arg2);
        case SYS_KBD_FLUSH: keyboard_flush(); return 0;
        case SYS_SETCOLOR:     vga_set_color((vga_color_t)arg1, (vga_color_t)arg2); return 0;
        case SYS_SET_RAW_MODE: return sys_set_raw_mode(arg1);
        case SYS_WAIT:         return sys_wait(arg1);
        case SYS_READDIR:      return sys_readdir();
        case SYS_WRITE_FILE:   return sys_write_file(arg1, arg2, arg3);
        case SYS_CREATE:       return sys_create((const char *)arg1);
        case SYS_PCI_LIST:     pci_print_list(); return 0;
        case SYS_FORK:         return sys_fork();
        default:
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("[SYSCALL] unknown number: ");
            vga_putdec(num);
            vga_puts("\n");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            return (uint32_t)-1;
    }
}
