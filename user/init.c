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

static void sys_yield(void) {
    int ret;
    /* "=a"/"0": eax=3 na entrada, retorno na saída.
       Sem isso, com -O2 o compilador assume eax=3 após iret e não recarrega,
       causando o próximo int $0x80 com eax=retorno_anterior em vez de 3. */
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "0"(3)
        : "memory"
    );
    (void)ret;
}

static const char msg[] = "init: hello from userland!\n";

void _start(void) {
    sys_write(msg, sizeof(msg) - 1);
    for (;;)
        sys_yield();
}
