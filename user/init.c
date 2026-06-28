/* nullos/user/init.c — primeiro processo de usuário */

static int sys_write(const char *buf, unsigned int len) {
    int ret;
    /* "=a" output + "0" input: eax=1 na entrada, retorno do kernel na saída.
       Garante que o compilador não reutiliza eax após o int $0x80. */
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

static const char msg[] = "init: hello from userland!\n";

void _start(void) {
    sys_write(msg, sizeof(msg) - 1);
    sys_exit(0);
}
