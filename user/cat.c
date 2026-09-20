/* nullos/user/cat.c — copies a file (or stdin) to stdout.
   With an argument ("cat notes.txt"): opens that file and prints it.
   Without one: reads all of stdin and writes it to stdout — which is what
   makes it a pipe sink (Phase 16): none of the other existing programs are
   meaningful pipe sinks, since the shell's builtins ("ps", "echo", ...)
   write straight to VGA and never through fd 1, so they can't be
   redirected into a pipe's write end. "forktest | cat" is the manual test
   in docs/pipes.md; "cat < file" reads stdin from a file the same way. */
#include "lib/nullos.h"
#include "lib/messages.h"

static void cat_puts(const char *s) {
    nos_write(1, s, (unsigned)strlen(s));
}

void _start(void) {
    char arg[64];
    int fd = 0;   /* stdin, unless a file name was given */

    int n = nos_getarg(arg, sizeof(arg));
    if (n > 0 && arg[0]) {
        fd = nos_open(arg);
        if (fd < 0) {
            cat_puts(msg(UMSG_CAT_CANNOT_OPEN));
            cat_puts(arg);
            cat_puts("\n");
            nos_exit(1);
        }
    }

    char buf[64];
    for (;;) {
        int r = nos_read(fd, buf, sizeof(buf));
        if (r <= 0) break;   /* EOF (0) or error (-1) */
        nos_write(1, buf, (unsigned)r);
    }
    if (fd != 0) nos_close(fd);
    nos_exit(0);
}
