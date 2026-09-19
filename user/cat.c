/* nullos/user/cat.c — reads all of stdin and writes it to stdout.
   Exists specifically to exercise real pipes end to end (Phase 16):
   none of the other existing programs are meaningful pipe sinks —
   the shell's own builtins ("ps", "echo", ...) write straight to VGA
   via syscalls that never go through fd 1 at all, so they can't be
   redirected into a pipe's write end. "forktest | cat" is the manual
   test in docs/pipes.md. */
#include "lib/nullos.h"

void _start(void) {
    char buf[64];
    for (;;) {
        int n = nos_read(0, buf, sizeof(buf));
        if (n <= 0) break;   /* EOF (0) or error (-1) */
        nos_write(1, buf, (unsigned)n);
    }
    nos_exit(0);
}
