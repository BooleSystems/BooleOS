#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_WRITE   1  /* write(fd, buf, len)               → bytes         */
#define SYS_EXIT    2  /* exit(code)                        → does not return */
#define SYS_YIELD   3  /* yield()                           → 0             */
#define SYS_GETPID  4  /* getpid()                          → pid           */
#define SYS_READ    5  /* read(fd, buf, len)                → bytes read    */
#define SYS_UPTIME  6  /* uptime()                          → ticks         */
#define SYS_MEMINFO 7  /* meminfo(*pmm_pages, *heap_bytes, *nprocs) → 0     */
#define SYS_PS      8  /* ps()   — prints process table via VGA → 0        */
#define SYS_KILL    9  /* kill(pid)                         → 0 or -1       */
#define SYS_EXEC   10  /* exec(name)                        → pid or -1     */
#define SYS_OPEN     11  /* open(name)                      → fd or -1      */
#define SYS_CLOSE    12  /* close(fd)                       → 0 or -1       */
#define SYS_READ_RAW  13  /* read_raw() → scancode|ctrl<<8 (blocking)       */
#define SYS_GOTOXY    14  /* gotoxy(col, row)               → 0             */
#define SYS_CLEAR     15  /* clear()                        → 0             */
#define SYS_GETARG    16  /* getarg(buf, len)               → bytes or -1   */
#define SYS_KBD_FLUSH  17  /* kbd_flush() — flushes keyboard buffers → 0    */
#define SYS_SETCOLOR      18  /* set_color(fg, bg)              → 0             */
#define SYS_SET_RAW_MODE  19  /* set_raw_mode(1/0) — disables SYS_READ echo    */
#define SYS_WAIT          20  /* wait(pid) — blocks until pid terminates → 0   */
#define SYS_READDIR       21  /* readdir(path) — lists files (ramfs + FAT16) via VGA;
                                  path may be NULL/empty for the caller's cwd */
#define SYS_WRITE_FILE    22  /* write_file(fd, buf, len) — saves to FAT16 → 0/-1 */
#define SYS_CREATE        23  /* create(name) — opens or creates a FAT16 file → fd or -1 */
#define SYS_PCI_LIST      24  /* pci_list() — prints the PCI device table via VGA → device count */
#define SYS_FORK          25  /* fork() — duplicates the caller → child's pid (parent), 0 (child), -1 on failure */
#define SYS_CHDIR         26  /* chdir(path) — changes the caller's FAT16 cwd → 0 or -1 (cwd unchanged on failure) */
#define SYS_MKDIR         27  /* mkdir(path) — creates a directory on FAT16 → 0 or -1 */

uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif
