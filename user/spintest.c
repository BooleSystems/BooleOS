/* nullos/user/spintest.c — testa preempção: nunca chama yield */

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

static const char msg[] = "spintest: still spinning\n";

void _start(void) {
    volatile unsigned int counter = 0;
    for (;;) {
        counter++;
        if (counter % 5000000 == 0)
            sys_write(msg, sizeof(msg) - 1);
    }
}
