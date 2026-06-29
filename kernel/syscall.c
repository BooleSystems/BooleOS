// nullos/kernel/syscall.c
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
#include <stdint.h>

static uint32_t sys_write(uint32_t fd, const char *buf, uint32_t len) {
    (void)fd;  // só stdout por enquanto
    if (!buf) return (uint32_t)-1;
    for (uint32_t i = 0; i < len; i++)
        vga_putchar(buf[i]);
    return len;
}

static uint32_t sys_exit(uint32_t code) {
    (void)code;
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

static uint32_t sys_read(uint32_t fd, char *buf, uint32_t len) {
    if (fd != 0 || !buf || len == 0)
        return (uint32_t)-1;

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

        vga_putchar((char)c);   /* eco */

        if (c == '\b') {
            if (n > 0) n--;     /* backspace: descarta último char */
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

#define USER_STR_MAX 64

/* resolve um byte no espaço virtual de cur para seu endereço físico identity-mapped */
static char *user_kptr(process_t *cur, uint32_t uaddr) {
    uint32_t phys = vmm_get_phys_from_dir(cur->cr3, uaddr);
    if (!phys) return (char *)0;
    return (char *)((phys & ~0xFFFu) | (uaddr & 0xFFFu));
}

static uint32_t sys_exec(const char *user_name) {
    if (!user_name) return (uint32_t)-1;
    process_t *cur = process_current();
    if (!cur) return (uint32_t)-1;
    char kname[USER_STR_MAX];
    uint32_t uaddr = (uint32_t)user_name;

    uint32_t i;
    for (i = 0; i < USER_STR_MAX - 1; i++) {
        char *kp = user_kptr(cur, uaddr + i);
        if (!kp) return (uint32_t)-1;
        char c = *kp;
        kname[i] = c;
        if (c == '\0') break;
    }
    kname[i] = '\0';

    process_t *p = exec(kname);
    if (!p) return (uint32_t)-1;
    return p->pid;
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
        case SYS_EXEC:    return sys_exec((const char *)arg1);
        default:
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("[SYSCALL] numero desconhecido: ");
            vga_putdec(num);
            vga_puts("\n");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            return (uint32_t)-1;
    }
}
