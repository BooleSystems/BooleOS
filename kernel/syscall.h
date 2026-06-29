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
#define SYS_EXEC   10  /* exec(name)                        → pid ou -1    */
#define SYS_OPEN     11  /* open(name)                      → fd ou -1     */
#define SYS_CLOSE    12  /* close(fd)                       → 0 ou -1      */
#define SYS_READ_RAW  13  /* read_raw() → scancode|ctrl<<8 (bloqueante)    */
#define SYS_GOTOXY    14  /* gotoxy(col, row)               → 0            */
#define SYS_CLEAR     15  /* clear()                        → 0            */
#define SYS_GETARG    16  /* getarg(buf, len)               → bytes ou -1  */
#define SYS_KBD_FLUSH  17  /* kbd_flush() — esvazia buffers de teclado → 0  */
#define SYS_SETCOLOR      18  /* set_color(fg, bg)              → 0            */
#define SYS_SET_RAW_MODE  19  /* set_raw_mode(1/0) — desativa eco no SYS_READ */
#define SYS_WAIT          20  /* wait(pid) — bloqueia até pid terminar → 0    */

uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif
