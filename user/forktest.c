/* nullos/user/forktest.c — exercises fork(): validates that both the
   parent and child execution paths actually happen, with correct PIDs,
   and (Phase 15) that the child inherits the parent's cwd_cluster —
   both processes create a relative-path marker file named after their
   own pid, so if both end up in the same directory when listed, the
   cwd was correctly shared at the moment of fork(). */
#include "lib/nullos.h"

static void ft_puts(const char *s) {
    nos_write(1, s, strlen(s));
}

/* Builds "fk<pid>.txt" into fname (must be at least 20 bytes: "fk" + up
   to 10 digits for a uint32 pid + ".txt" + '\0' = 17, rounded up) and
   creates it via a RELATIVE path — used by both the parent and the
   child below, right after fork(), to make cwd inheritance visible
   from the shell via a plain "ls" (see the comment at the top of this
   file). */
static void create_cwd_marker(unsigned int pid, char *fname) {
    char nbuf[16];
    char *n = nos_uitoa(pid, nbuf, sizeof(nbuf));

    fname[0] = 'f';
    fname[1] = 'k';
    int i = 2;
    while (*n) fname[i++] = *n++;
    fname[i++] = '.'; fname[i++] = 't'; fname[i++] = 'x'; fname[i++] = 't';
    fname[i] = '\0';

    int fd = nos_create(fname);
    if (fd >= 0) {
        ft_puts("forktest: created ");
        ft_puts(fname);
        ft_puts(" in cwd\n");
        nos_close(fd);
    } else {
        ft_puts("forktest: could not create ");
        ft_puts(fname);
        ft_puts(" (no disk?)\n");
    }
}

void _start(void) {
    char nbuf[16];

    ft_puts("forktest: calling fork()...\n");

    int ret = nos_fork();

    {
        char fname[20];
        create_cwd_marker(nos_getpid(), fname);
    }

    if (ret < 0) {
        ft_puts("forktest: fork() failed (no free process slot, or out of memory)\n");
        nos_exit(1);
    } else if (ret == 0) {
        ft_puts("forktest: I'm the child, pid=");
        ft_puts(nos_uitoa(nos_getpid(), nbuf, sizeof(nbuf)));
        ft_puts("\n");
    } else {
        ft_puts("forktest: I'm the parent, child=");
        ft_puts(nos_uitoa((unsigned int)ret, nbuf, sizeof(nbuf)));
        ft_puts("\n");
    }

    nos_exit(0);
}
