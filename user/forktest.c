/* nullos/user/forktest.c — exercises fork(): validates that both the
   parent and child execution paths actually happen, with correct PIDs. */

static int sys_write(const char *buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "0"(1), "b"(1), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static void sys_exit(int code) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(2), "b"(code)
        : "memory"
    );
}

static unsigned int sys_getpid(void) {
    unsigned int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(4) : "memory");
    return ret;
}

static int sys_fork(void) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(25) : "memory");
    return ret;
}

static unsigned int ft_strlen(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

static void ft_puts(const char *s) {
    sys_write(s, ft_strlen(s));
}

/* converts uint32 to decimal string; returns pointer into buf (not start) */
static char *ft_uitoa(unsigned int v, char *buf, unsigned int bufsz) {
    buf[--bufsz] = '\0';
    if (v == 0) { buf[--bufsz] = '0'; return &buf[bufsz]; }
    while (v && bufsz > 0) {
        buf[--bufsz] = '0' + (v % 10);
        v /= 10;
    }
    return &buf[bufsz];
}

void _start(void) {
    char nbuf[16];

    ft_puts("forktest: calling fork()...\n");

    int ret = sys_fork();

    if (ret < 0) {
        ft_puts("forktest: fork() failed (no free process slot, or out of memory)\n");
        sys_exit(1);
    } else if (ret == 0) {
        ft_puts("forktest: I'm the child, pid=");
        ft_puts(ft_uitoa(sys_getpid(), nbuf, sizeof(nbuf)));
        ft_puts("\n");
    } else {
        ft_puts("forktest: I'm the parent, child=");
        ft_puts(ft_uitoa((unsigned int)ret, nbuf, sizeof(nbuf)));
        ft_puts("\n");
    }

    sys_exit(0);
}
