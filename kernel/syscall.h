#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_WRITE   1  /* write(fd, buf, len)               → bytes        */
#define SYS_EXIT    2  /* exit(code)                        → não retorna  */
#define SYS_YIELD   3  /* yield()                           → 0            */
#define SYS_GETPID  4  /* getpid()                          → pid          */
#define SYS_READ    5  /* read(fd, buf, len)                → bytes lidos  */
#define SYS_UPTIME  6  /* uptime()                          → ticks        */
#define SYS_MEMINFO 7  /* meminfo(*pmm_pages, *heap_bytes, *nprocs) → 0   */
#define SYS_PS      8  /* ps()   — imprime tabela de procs via VGA → 0    */
#define SYS_KILL    9  /* kill(pid)                         → 0 ou -1      */

uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif
