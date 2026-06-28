/* nullos/user/shell.c — shell básico de userland */

/* ── syscall wrappers ───────────────────────────────────────────── */

static int sys_write(const char *buf, unsigned int len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret)
        : "0"(1), "b"(1), "c"(buf), "d"(len)
        : "memory");
    return ret;
}

static int sys_read(char *buf, unsigned int len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret)
        : "0"(5), "b"(0), "c"(buf), "d"(len)
        : "memory");
    return ret;
}

static void sys_exit(int code) {
    __asm__ volatile ("int $0x80"
        : : "a"(2), "b"(code) : "memory");
}

/* ── helpers ────────────────────────────────────────────────────── */

static unsigned int sh_strlen(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

static int sh_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void sh_puts(const char *s) {
    sys_write(s, sh_strlen(s));
}

/* ── shell ──────────────────────────────────────────────────────── */

#define LINE_MAX 128

static const char *help_text =
    "comandos disponíveis:\n"
    "  help   mostra esta mensagem\n"
    "  clear  limpa a tela\n"
    "  exit   encerra o shell\n";

static const char *clear_text =
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

static void run_command(char *line, int len) {
    /* strip trailing newline */
    if (len > 0 && line[len - 1] == '\n')
        line[--len] = '\0';

    if (len == 0)
        return;

    if (sh_strcmp(line, "help") == 0) {
        sh_puts(help_text);
    } else if (sh_strcmp(line, "clear") == 0) {
        sh_puts(clear_text);
    } else if (sh_strcmp(line, "exit") == 0) {
        sh_puts("tchau!\n");
        sys_exit(0);
    } else {
        sh_puts("comando nao encontrado: ");
        sh_puts(line);
        sh_puts("\n");
    }
}

void _start(void) {
    static char line[LINE_MAX];

    sh_puts("NullOS shell — digite 'help'\n");

    for (;;) {
        sh_puts("> ");
        int n = sys_read(line, LINE_MAX - 1);
        if (n <= 0)
            continue;
        line[n] = '\0';
        run_command(line, n);
    }
}
