/* nullos/user/shell.c — shell interativo */

typedef unsigned int uint32_t;

/* ── syscall wrappers ───────────────────────────────────────────── */

static int sys_write(const char *buf, unsigned int len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(1), "b"(1), "c"(buf), "d"(len) : "memory");
    return ret;
}

static int sys_read(char *buf, unsigned int len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(5), "b"(0), "c"(buf), "d"(len) : "memory");
    return ret;
}

static void sys_exit(int code) {
    __asm__ volatile ("int $0x80" : : "a"(2), "b"(code) : "memory");
    for (;;);
}

static uint32_t sys_uptime(void) {
    uint32_t ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(6) : "memory");
    return ret;
}

static void sys_meminfo(uint32_t *pmm_pages, uint32_t *heap_bytes, uint32_t *nprocs) {
    __asm__ volatile ("int $0x80"
        : : "a"(7), "b"(pmm_pages), "c"(heap_bytes), "d"(nprocs) : "memory");
}

static void sys_ps(void) {
    __asm__ volatile ("int $0x80" : : "a"(8) : "memory");
}

static int sys_exec(const char *name) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(10), "b"(name) : "memory");
    return ret;
}

static int sys_kill(uint32_t pid) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(9), "b"(pid) : "memory");
    return ret;
}

static uint32_t sys_getpid(void) {
    uint32_t ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(4) : "memory");
    return ret;
}

/* ── string helpers ─────────────────────────────────────────────── */

static unsigned int sh_strlen(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

static int sh_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int sh_strncmp(const char *a, const char *b, unsigned int n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    if (n == (unsigned int)-1) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

static void sh_puts(const char *s) {
    sys_write(s, sh_strlen(s));
}

/* converts uint32 to decimal string; returns pointer into buf (not start) */
static char *sh_uitoa(uint32_t v, char *buf, unsigned int bufsz) {
    buf[--bufsz] = '\0';
    if (v == 0) { buf[--bufsz] = '0'; return &buf[bufsz]; }
    while (v && bufsz > 0) {
        buf[--bufsz] = '0' + (v % 10);
        v /= 10;
    }
    return &buf[bufsz];
}

/* ── commands ───────────────────────────────────────────────────── */

static void cmd_uname(void) {
    sh_puts("NullOS v0.7.0 i686\n");
}

static void cmd_fetch(void) {
    static const char *logo[] = {
        "  _   _       _ _  ___  ____  ",
        " | \\ | |_   _| | |/ _ \\/ ___| ",
        " |  \\| | | | | | | | | \\___ \\ ",
        " | |\\  | |_| | | | |_| |___) |",
        " |_| \\_|\\__,_|_|_|\\___/|____/ ",
    };

    uint32_t pmm_pages = 0, heap_bytes = 0, nprocs = 0;
    sys_meminfo(&pmm_pages, &heap_bytes, &nprocs);

    uint32_t ticks  = sys_uptime();
    uint32_t uptime = ticks / 100;   /* 100 Hz */

    char nbuf[16];
    char *n;

    /* line 0: logo + OS */
    sh_puts(logo[0]); sh_puts("  OS: NullOS v0.7.0 i686\n");

    /* line 1: logo + Arch */
    sh_puts(logo[1]); sh_puts("  Arch: i686\n");

    /* line 2: logo + Uptime */
    sh_puts(logo[2]);
    sh_puts("  Uptime: ");
    n = sh_uitoa(uptime, nbuf, sizeof(nbuf));
    sh_puts(n); sh_puts("s\n");

    /* line 3: logo + PMM */
    sh_puts(logo[3]);
    sh_puts("  Mem PMM: ");
    n = sh_uitoa(pmm_pages * 4, nbuf, sizeof(nbuf));
    sh_puts(n); sh_puts(" KB livres\n");

    /* line 4: logo + Heap */
    sh_puts(logo[4]);
    sh_puts("  Heap: ");
    n = sh_uitoa(heap_bytes, nbuf, sizeof(nbuf));
    sh_puts(n); sh_puts(" B livres\n");

    /* line 5: padding + Procs */
    sh_puts("                                   Procs: ");
    n = sh_uitoa(nprocs, nbuf, sizeof(nbuf));
    sh_puts(n); sh_puts(" rodando\n");
}

static void cmd_ps(void) {
    sys_ps();
}

static void cmd_mem(void) {
    uint32_t pmm_pages = 0, heap_bytes = 0, nprocs = 0;
    sys_meminfo(&pmm_pages, &heap_bytes, &nprocs);

    char nbuf[16];
    char *n;

    sh_puts("PMM:  ");
    n = sh_uitoa(pmm_pages, nbuf, sizeof(nbuf));
    sh_puts(n);
    sh_puts(" paginas livres (");
    n = sh_uitoa(pmm_pages * 4, nbuf, sizeof(nbuf));
    sh_puts(n);
    sh_puts(" KB)\n");

    sh_puts("Heap: ");
    n = sh_uitoa(heap_bytes, nbuf, sizeof(nbuf));
    sh_puts(n);
    sh_puts(" B livres\n");
}

static void cmd_echo(const char *line) {
    /* pula "echo " */
    if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' &&
        line[3] == 'o' && line[4] == ' ') {
        sh_puts(line + 5);
        sh_puts("\n");
    } else {
        sh_puts("\n");
    }
}

static void cmd_kill(const char *arg) {
    if (!arg || !*arg) { sh_puts("uso: kill <pid>\n"); return; }

    uint32_t pid = 0;
    while (*arg >= '0' && *arg <= '9')
        pid = pid * 10 + (uint32_t)(*arg++ - '0');

    if (pid == 0) { sh_puts("pid invalido\n"); return; }

    /* avisa se for o proprio shell */
    if (pid == sys_getpid()) {
        sh_puts("encerrando shell...\n");
        sys_exit(0);
    }

    int r = sys_kill(pid);
    if (r == 0) {
        sh_puts("processo ");
        char nbuf[16];
        sh_puts(sh_uitoa(pid, nbuf, sizeof(nbuf)));
        sh_puts(" encerrado\n");
    } else {
        sh_puts("pid nao encontrado\n");
    }
}

static void cmd_run(const char *name) {
    if (!name || !*name) { sh_puts("uso: run <programa>\n"); return; }
    int pid = sys_exec(name);
    if (pid < 0) {
        sh_puts("erro: programa nao encontrado\n");
    } else {
        sh_puts("executando: ");
        sh_puts(name);
        sh_puts("\n");
    }
}

static const char *help_text =
    "comandos:\n"
    "  help           esta mensagem\n"
    "  uname          versao do sistema\n"
    "  fetch          info do sistema\n"
    "  ps             tabela de processos\n"
    "  mem            uso de memoria\n"
    "  echo <texto>   imprime texto\n"
    "  kill <pid>     encerra processo\n"
    "  run <prog>     executa programa em background\n"
    "  clear          limpa a tela\n"
    "  exit           encerra o shell\n";

static const char *clear_text =
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

static void run_command(char *line, int len) {
    if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
    if (len == 0) return;

    if (sh_strcmp(line, "help") == 0) {
        sh_puts(help_text);
    } else if (sh_strcmp(line, "uname") == 0) {
        cmd_uname();
    } else if (sh_strcmp(line, "fetch") == 0) {
        cmd_fetch();
    } else if (sh_strcmp(line, "ps") == 0) {
        cmd_ps();
    } else if (sh_strcmp(line, "mem") == 0) {
        cmd_mem();
    } else if (sh_strncmp(line, "echo", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
        cmd_echo(line);
    } else if (sh_strncmp(line, "kill", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
        cmd_kill(line[4] == ' ' ? line + 5 : "");
    } else if (sh_strncmp(line, "run", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
        cmd_run(line[3] == ' ' ? line + 4 : "");
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

/* ── entry point ────────────────────────────────────────────────── */

void _start(void) {
    static char line[128];
    static int foreground_pid = 0;

    sh_puts("NullOS shell — digite 'help'\n");

    for (;;) {
        sh_puts("> ");
        int n = sys_read(line, 127);
        if (n <= 0) continue;
        if (n == 1 && line[0] == 0x03) {
            if (foreground_pid > 0) {
                sys_kill((uint32_t)foreground_pid);
                foreground_pid = 0;
            }
            continue;
        }
        line[n] = '\0';

        /* extrai PID de run antes de despachar o comando */
        if (sh_strncmp(line, "run", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
            char *name = line[3] == ' ' ? line + 4 : "";
            unsigned int nlen = sh_strlen(name);
            if (nlen > 0 && name[nlen - 1] == '\n') name[nlen - 1] = '\0';
            int pid = sys_exec(name);
            if (pid < 0) {
                sh_puts("erro: programa nao encontrado\n");
            } else {
                foreground_pid = pid;
                sh_puts("executando: ");
                sh_puts(name);
                sh_puts("\n");
            }
        } else {
            foreground_pid = 0;
            run_command(line, n);
        }
    }
}
