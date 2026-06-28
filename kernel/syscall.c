// nullos/kernel/syscall.c
#include "syscall.h"
#include "process.h"
#include "scheduler.h"
#include "drivers/vga.h"
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

uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    switch (num) {
        case SYS_WRITE:  return sys_write(arg1, (const char *)arg2, arg3);
        case SYS_EXIT:   return sys_exit(arg1);
        case SYS_YIELD:  return sys_yield();
        case SYS_GETPID: return sys_getpid();
        default:
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("[SYSCALL] numero desconhecido: ");
            vga_putdec(num);
            vga_puts("\n");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            return (uint32_t)-1;
    }
}
