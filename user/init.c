/* nullos/user/init.c — primeiro processo de usuário */

static void sys_write(const char *buf, unsigned int len) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(1), "b"(1), "c"(buf), "d"(len)
        : "memory"
    );
}

static void sys_yield(void) {
    __asm__ volatile ("int $0x80" : : "a"(3) : "memory");
}

static const char msg[] = "init: hello from userland!\n";

void _start(void) {
    sys_write(msg, sizeof(msg) - 1);
    for (;;)
        sys_yield();
}
