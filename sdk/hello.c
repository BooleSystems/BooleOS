/* booleos/sdk/hello.c — the smallest useful BooleOS program: a template.
 *
 * Build it with `make` in this directory, put it on the disk with
 * `make inject PROG=hello`, then in the BooleOS shell type:  run hello.elf
 *
 * The rules for a BooleOS program:
 *   - the entry point is `_start` (there is no main() and no C runtime);
 *   - there is no libc: use libnos (booleos.h) for system calls and for the
 *     small printf family;
 *   - the program MUST end with nos_exit(): _start() has nowhere to return to.
 * See docs/sdk.md.
 */
#include "booleos.h"

void _start(void) {
    printf("Hello from BooleOS!\n");
    printf("  my pid is %u\n", nos_getpid());
    printf("  the system has been up for %u ticks (100 per second)\n", nos_uptime());

    uint32_t free_pages = 0, heap_free = 0, procs = 0;
    nos_meminfo(&free_pages, &heap_free, &procs);
    printf("  %u free pages (%u KB), %u process(es) running\n",
           free_pages, free_pages * 4, procs);

    nos_exit(0);
}
