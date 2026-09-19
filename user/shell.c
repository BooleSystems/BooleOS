/* nullos/user/shell.c — interactive shell */

#include "version.h"
#include "lib/nullos.h"

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
    nos_write(1, s, sh_strlen(s));
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
    sh_puts(NULLOS_SHORT_BANNER " i686\n");
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
    nos_meminfo(&pmm_pages, &heap_bytes, &nprocs);

    uint32_t ticks  = nos_uptime();
    uint32_t uptime = ticks / 100;   /* 100 Hz */

    char nbuf[16];
    char *n;

    /* line 0: logo + OS */
    sh_puts(logo[0]); sh_puts("  OS: " NULLOS_SHORT_BANNER " i686\n");

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
    sh_puts(n); sh_puts(" KB free\n");

    /* line 4: logo + Heap */
    sh_puts(logo[4]);
    sh_puts("  Heap: ");
    n = sh_uitoa(heap_bytes, nbuf, sizeof(nbuf));
    sh_puts(n); sh_puts(" B free\n");

    /* line 5: padding + Procs */
    sh_puts("                                   Procs: ");
    n = sh_uitoa(nprocs, nbuf, sizeof(nbuf));
    sh_puts(n); sh_puts(" running\n");
}

static void cmd_ps(void) {
    nos_ps();
}

static void cmd_mem(void) {
    uint32_t pmm_pages = 0, heap_bytes = 0, nprocs = 0;
    nos_meminfo(&pmm_pages, &heap_bytes, &nprocs);

    char nbuf[16];
    char *n;

    sh_puts("PMM:  ");
    n = sh_uitoa(pmm_pages, nbuf, sizeof(nbuf));
    sh_puts(n);
    sh_puts(" free pages (");
    n = sh_uitoa(pmm_pages * 4, nbuf, sizeof(nbuf));
    sh_puts(n);
    sh_puts(" KB)\n");

    sh_puts("Heap: ");
    n = sh_uitoa(heap_bytes, nbuf, sizeof(nbuf));
    sh_puts(n);
    sh_puts(" B free\n");
}

static void cmd_echo(const char *line) {
    /* skip "echo " */
    if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' &&
        line[3] == 'o' && line[4] == ' ') {
        sh_puts(line + 5);
        sh_puts("\n");
    } else {
        sh_puts("\n");
    }
}

static void cmd_kill(const char *arg) {
    if (!arg || !*arg) { sh_puts("usage: kill <pid>\n"); return; }

    uint32_t pid = 0;
    while (*arg >= '0' && *arg <= '9')
        pid = pid * 10 + (uint32_t)(*arg++ - '0');

    if (pid == 0) { sh_puts("invalid pid\n"); return; }

    /* warn if it's the shell itself */
    if (pid == nos_getpid()) {
        sh_puts("shutting down shell...\n");
        nos_exit(0);
    }

    int r = nos_kill(pid);
    if (r == 0) {
        sh_puts("process ");
        char nbuf[16];
        sh_puts(sh_uitoa(pid, nbuf, sizeof(nbuf)));
        sh_puts(" terminated\n");
    } else {
        sh_puts("pid not found\n");
    }
}

static void cmd_touch(const char *arg) {
    if (!arg || !*arg) { sh_puts("usage: touch <file>\n"); return; }
    int fd = nos_create(arg);
    if (fd < 0) {
        sh_puts("error: could not create (no disk?)\n");
        return;
    }
    nos_close(fd);
}

static void cmd_mkdir(const char *arg) {
    if (!arg || !*arg) { sh_puts("usage: mkdir <dir>\n"); return; }
    if (nos_mkdir(arg) < 0) {
        sh_puts("error: could not create directory (no disk, path missing, or name taken by a file)\n");
    }
}

static void cmd_cd(const char *arg) {
    const char *path = (arg && *arg) ? arg : "/";
    if (nos_chdir(path) != 0) {
        sh_puts("cd: no such directory: ");
        sh_puts(path);
        sh_puts("\n");
    }
}

/* Trims leading/trailing spaces (and a trailing '\n'/'\r', in case
   this is the tail end of the raw input line) in place, returning a
   pointer into the same buffer. */
static char *sh_trim(char *s) {
    while (*s == ' ') s++;
    unsigned n = sh_strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
    return s;
}

/* "cmd1 | cmd2": creates a pipe, launches both sides via
   SYS_EXEC_PIPE (cmd1's stdout redirected to the pipe's write end,
   cmd2's stdin to its read end), then — critically — closes the
   shell's OWN copies of both raw pipe fds before waiting on either
   child. See docs/pipes.md for why that close is not optional: fork()
   never happens here (SYS_EXEC_PIPE spawns each stage directly, see
   its own doc comment in kernel/syscall.c), but the shell's own
   fd_table entries from nos_pipe() are a real, separate reference to
   each end — if left open, cmd1 exiting would drop the write end's
   refcount to 1 (the shell's own lingering copy), never reaching 0,
   and cmd2 would block forever waiting for an EOF that can no longer
   arrive. Neither pipeline stage can take an exec-style argument in
   this first cut — SYS_EXEC_PIPE's 3 registers are fully spent on
   name+stdin_fd+stdout_fd, with no room left for one (a real, stated
   scope limit, not an oversight). */
static void run_pipeline(const char *cmd1, const char *cmd2) {
    int fds[2];
    if (nos_pipe(fds) != 0) {
        sh_puts("pipe: could not create pipe\n");
        return;
    }
    int read_fd = fds[0], write_fd = fds[1];

    int pid1 = nos_exec_pipe(cmd1, -1, write_fd);
    if (pid1 < 0) {
        sh_puts("pipe: program not found: ");
        sh_puts(cmd1);
        sh_puts("\n");
        nos_close(read_fd);
        nos_close(write_fd);
        return;
    }

    int pid2 = nos_exec_pipe(cmd2, read_fd, -1);
    if (pid2 < 0) {
        sh_puts("pipe: program not found: ");
        sh_puts(cmd2);
        sh_puts("\n");
        nos_close(read_fd);
        nos_close(write_fd);
        nos_kill((uint32_t)pid1);
        return;
    }

    /* the shell needs neither raw end anymore — see the comment above */
    nos_close(read_fd);
    nos_close(write_fd);

    nos_wait(pid1);
    nos_wait(pid2);
}

static void cmd_run(const char *name) {
    if (!name || !*name) { sh_puts("usage: run <program>\n"); return; }
    int pid = nos_exec(name, 0);
    if (pid < 0) {
        sh_puts("error: program not found\n");
    } else {
        sh_puts("running: ");
        sh_puts(name);
        sh_puts("\n");
    }
}

static const char *help_text =
    "commands:\n"
    "  help           this message\n"
    "  uname          system version\n"
    "  fetch          system info\n"
    "  ps             process table\n"
    "  mem            memory usage\n"
    "  ls [dir]       list files (cwd, or a given path)\n"
    "  lspci          list PCI devices\n"
    "  touch <name>   create an empty file (path allowed, e.g. docs/a.txt)\n"
    "  mkdir <dir>    create a directory (path allowed)\n"
    "  cd [dir]       change the current directory (no arg = root)\n"
    "  echo <text>    print text\n"
    "  kill <pid>     terminate a process\n"
    "  run <prog>     run a program in the background\n"
    "  edit <file>    open the text editor\n"
    "  cmd1 | cmd2    pipe cmd1's stdout into cmd2's stdin (both must\n"
    "                 be programs, not builtins — e.g. \"forktest | cat\")\n"
    "  clear          clear the screen\n"
    "  exit           exit the shell\n";

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
    } else if (sh_strcmp(line, "ls") == 0) {
        nos_readdir(0);
    } else if (sh_strncmp(line, "ls", 2) == 0 && line[2] == ' ') {
        nos_readdir(line + 3);
    } else if (sh_strcmp(line, "lspci") == 0) {
        nos_pci_list();
    } else if (sh_strncmp(line, "touch", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) {
        cmd_touch(line[5] == ' ' ? line + 6 : "");
    } else if (sh_strncmp(line, "mkdir", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) {
        cmd_mkdir(line[5] == ' ' ? line + 6 : "");
    } else if (sh_strncmp(line, "cd", 2) == 0 && (line[2] == ' ' || line[2] == '\0')) {
        cmd_cd(line[2] == ' ' ? line + 3 : "");
    } else if (sh_strncmp(line, "echo", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
        cmd_echo(line);
    } else if (sh_strncmp(line, "kill", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
        cmd_kill(line[4] == ' ' ? line + 5 : "");
    } else if (sh_strncmp(line, "run", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
        cmd_run(line[3] == ' ' ? line + 4 : "");
    } else if (sh_strcmp(line, "clear") == 0) {
        sh_puts(clear_text);
    } else if (sh_strcmp(line, "exit") == 0) {
        sh_puts("bye!\n");
        nos_exit(0);
    } else {
        sh_puts("command not found: ");
        sh_puts(line);
        sh_puts("\n");
    }
}

/* ── entry point ────────────────────────────────────────────────── */

void _start(void) {
    static char line[128];
    static int foreground_pid = 0;

    sh_puts("NullOS shell — type 'help'\n");

    for (;;) {
        sh_puts("> ");
        int n = nos_read(0, line, 127);
        if (n <= 0) continue;
        if (n == 1 && line[0] == 0x03) {
            if (foreground_pid > 0) {
                nos_kill((uint32_t)foreground_pid);
                foreground_pid = 0;
            }
            continue;
        }
        line[n] = '\0';

        /* "cmd1 | cmd2" — checked first, before any other dispatch,
           so it can never be shadowed by "edit"/"run" special-casing
           below (a pipeline's LHS could itself be named "run..." as
           plain text, e.g. "run1 | cat", and must still split on '|'
           correctly). Splitting on '|' commits to pipeline handling
           for the rest of this line — including the invalid-usage
           case below — since '|' is never a valid character in any
           other command form here. */
        char *bar = 0;
        for (char *p = line; *p; p++) { if (*p == '|') { bar = p; break; } }

        if (bar) {
            *bar = '\0';
            char *cmd1 = sh_trim(line);
            char *cmd2 = sh_trim(bar + 1);
            if (!*cmd1 || !*cmd2) {
                sh_puts("usage: cmd1 | cmd2\n");
            } else {
                foreground_pid = 0;
                run_pipeline(cmd1, cmd2);
            }
        } else if (sh_strncmp(line, "edit", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
            char *arg = line[4] == ' ' ? line + 5 : "";
            unsigned int alen = sh_strlen(arg);
            if (alen > 0 && arg[alen - 1] == '\n') arg[alen - 1] = '\0';
            int pid = nos_exec("edit", arg);
            if (pid < 0) {
                sh_puts("error: edit not found\n");
            } else {
                foreground_pid = pid;
                nos_wait(pid);   /* blocks until the editor exits */
                foreground_pid = 0;
            }
        } else if (sh_strncmp(line, "run", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
            char *name = line[3] == ' ' ? line + 4 : "";
            unsigned int nlen = sh_strlen(name);
            if (nlen > 0 && name[nlen - 1] == '\n') name[nlen - 1] = '\0';
            int pid = nos_exec(name, 0);
            if (pid < 0) {
                sh_puts("error: program not found\n");
            } else {
                foreground_pid = pid;
                sh_puts("running: ");
                sh_puts(name);
                sh_puts("\n");
            }
        } else {
            foreground_pid = 0;
            run_command(line, n);
        }
    }
}
