#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_WRITE  1   /* write(fd, buf, len)  → bytes escritos */
#define SYS_EXIT   2   /* exit(code)           → não retorna    */
#define SYS_YIELD  3   /* yield()              → 0              */
#define SYS_GETPID 4   /* getpid()             → pid            */

uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif
