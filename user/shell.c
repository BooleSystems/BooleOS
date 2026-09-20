/* nullos/user/shell.c — interactive shell */

#include "version.h"
#include "lib/nullos.h"

/* ── string helpers ─────────────────────────────────────────────── */

static void sh_puts(const char *s) {
    nos_write(1, s, strlen(s));
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
    n = nos_uitoa(uptime, nbuf, sizeof(nbuf));
    sh_puts(n); sh_puts("s\n");

    /* line 3: logo + PMM */
    sh_puts(logo[3]);
    sh_puts("  Mem PMM: ");
    n = nos_uitoa(pmm_pages * 4, nbuf, sizeof(nbuf));
    sh_puts(n); sh_puts(" KB free\n");

    /* line 4: logo + Heap */
    sh_puts(logo[4]);
    sh_puts("  Heap: ");
    n = nos_uitoa(heap_bytes, nbuf, sizeof(nbuf));
    sh_puts(n); sh_puts(" B free\n");

    /* line 5: padding + Procs */
    sh_puts("                                   Procs: ");
    n = nos_uitoa(nprocs, nbuf, sizeof(nbuf));
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
    n = nos_uitoa(pmm_pages, nbuf, sizeof(nbuf));
    sh_puts(n);
    sh_puts(" free pages (");
    n = nos_uitoa(pmm_pages * 4, nbuf, sizeof(nbuf));
    sh_puts(n);
    sh_puts(" KB)\n");

    sh_puts("Heap: ");
    n = nos_uitoa(heap_bytes, nbuf, sizeof(nbuf));
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
        sh_puts(nos_uitoa(pid, nbuf, sizeof(nbuf)));
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

static void cmd_pwd(void) {
    char path[128];
    int n = nos_getcwd(path, sizeof(path));
    if (n < 0) {
        sh_puts("pwd: cannot determine the current directory\n");
        return;
    }
    sh_puts(path);
    sh_puts("\n");
}

/* Trims leading/trailing spaces (and a trailing '\n'/'\r', in case
   this is the tail end of the raw input line) in place, returning a
   pointer into the same buffer. */
static char *sh_trim(char *s) {
    while (*s == ' ') s++;
    unsigned n = strlen(s);
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

/* "cmd [< infile] [> outfile]": SYS_EXEC_PIPE with the redirect fds being
   plain FAT16/ramfs file fds instead of pipe ends — the kernel side never
   cared which kind of fd it copies (see docs/pipes.md). `>` creates the
   file if needed and truncates it (nos_write_file with length 0 replaces
   the content with nothing); the program's writes then accumulate at the
   fd's position. Only external programs can be redirected: builtins print
   from inside the shell process (or the kernel), and a process cannot
   redirect its own fd 1 — that would need a dup2-style syscall. Like the
   pipeline stages, the program is launched by name only, no arguments
   (SYS_EXEC_PIPE has no register left for one). `line` is modified in
   place. */
static void run_redirected(char *line) {
    char *in_name = 0, *out_name = 0;
    int dup = 0;

    char *q = line;
    while (*q && *q != '<' && *q != '>') q++;
    char c = *q;
    *q = '\0';
    char *cmd = sh_trim(line);

    while (c) {
        char *start = q + 1;
        q = start;
        while (*q && *q != '<' && *q != '>') q++;
        char next = *q;
        *q = '\0';
        char *target = sh_trim(start);
        if (c == '<') { if (in_name)  dup = 1; in_name  = target; }
        else          { if (out_name) dup = 1; out_name = target; }
        c = next;
    }

    if (!*cmd || dup || (in_name && !*in_name) || (out_name && !*out_name)) {
        sh_puts("usage: cmd [< infile] [> outfile]\n");
        return;
    }
    for (const char *p = cmd; *p; p++) {
        if (*p == ' ') {
            sh_puts("redirect: the program is launched by name only, no arguments\n");
            return;
        }
    }

    int in_fd = -1, out_fd = -1;

    if (in_name) {
        in_fd = nos_open(in_name);
        if (in_fd < 0) {
            sh_puts("redirect: cannot open: ");
            sh_puts(in_name);
            sh_puts("\n");
            return;
        }
    }
    if (out_name) {
        out_fd = nos_create(out_name);
        if (out_fd < 0 || nos_write_file(out_fd, "", 0) != 0) {
            sh_puts("redirect: cannot write to: ");
            sh_puts(out_name);
            sh_puts(" (no disk, or not a FAT16 file)\n");
            if (out_fd >= 0) nos_close(out_fd);
            if (in_fd >= 0)  nos_close(in_fd);
            return;
        }
    }

    int pid = nos_exec_pipe(cmd, in_fd, out_fd);
    if (pid < 0) {
        sh_puts("redirect: program not found: ");
        sh_puts(cmd);
        sh_puts(" (built-in commands can't be redirected)\n");
    }

    /* the shell needs neither file fd anymore: the child holds its own
       copies (SYS_EXEC_PIPE duplicated them) */
    if (in_fd >= 0)  nos_close(in_fd);
    if (out_fd >= 0) nos_close(out_fd);

    if (pid >= 0) nos_wait(pid);
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
    "  pwd            print the current directory\n"
    "  echo <text>    print text\n"
    "  kill <pid>     terminate a process\n"
    "  run <prog>     run a program in the background\n"
    "  edit <file>    open the text editor\n"
    "  cat <file>     print a file\n"
    "  cmd < in       run a program with stdin read from a file\n"
    "  cmd > out      run a program with stdout written to a file (truncates;\n"
    "                 program name only, no arguments, no builtins)\n"
    "  reboot         restart the machine\n"
    "  shutdown       power the machine off\n"
    "  cmd1 | cmd2    pipe cmd1's stdout into cmd2's stdin (both must\n"
    "                 be programs, not builtins — e.g. \"forktest | cat\")\n"
    "  clear          clear the screen\n"
    "  exit           exit the shell\n";

static const char *clear_text =
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

static void run_command(char *line, int len) {
    if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
    if (len == 0) return;

    if (strcmp(line, "help") == 0) {
        sh_puts(help_text);
    } else if (strcmp(line, "uname") == 0) {
        cmd_uname();
    } else if (strcmp(line, "fetch") == 0) {
        cmd_fetch();
    } else if (strcmp(line, "ps") == 0) {
        cmd_ps();
    } else if (strcmp(line, "mem") == 0) {
        cmd_mem();
    } else if (strcmp(line, "ls") == 0) {
        nos_readdir(0);
    } else if (strncmp(line, "ls", 2) == 0 && line[2] == ' ') {
        nos_readdir(line + 3);
    } else if (strcmp(line, "lspci") == 0) {
        nos_pci_list();
    } else if (strcmp(line, "pwd") == 0) {
        cmd_pwd();
    } else if (strcmp(line, "reboot") == 0) {
        nos_reboot();      /* only returns if the reset failed; the kernel says why */
    } else if (strcmp(line, "shutdown") == 0) {
        nos_shutdown();    /* only returns if unsupported/failed; the kernel says why */
    } else if (strncmp(line, "touch", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) {
        cmd_touch(line[5] == ' ' ? line + 6 : "");
    } else if (strncmp(line, "mkdir", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) {
        cmd_mkdir(line[5] == ' ' ? line + 6 : "");
    } else if (strncmp(line, "cd", 2) == 0 && (line[2] == ' ' || line[2] == '\0')) {
        cmd_cd(line[2] == ' ' ? line + 3 : "");
    } else if (strncmp(line, "echo", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
        cmd_echo(line);
    } else if (strncmp(line, "kill", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
        cmd_kill(line[4] == ' ' ? line + 5 : "");
    } else if (strncmp(line, "run", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
        cmd_run(line[3] == ' ' ? line + 4 : "");
    } else if (strcmp(line, "clear") == 0) {
        sh_puts(clear_text);
    } else if (strcmp(line, "exit") == 0) {
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

        /* nos_read() returns the line WITH its trailing '\n'. Drop it (and a
           stray '\r') before any dispatch below: the "edit"/"run"/"cat"
           branches test for "the command name followed by ' ' or the end of
           the string", so with the '\n' still attached a bare "edit" matched
           neither and fell through to "command not found: edit". */
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (n == 0) continue;

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

        int has_redirect = 0;
        for (char *p = line; *p; p++) { if (*p == '<' || *p == '>') { has_redirect = 1; break; } }

        if (bar && has_redirect) {
            sh_puts("redirection can't be combined with a pipe yet\n");
        } else if (bar) {
            *bar = '\0';
            char *cmd1 = sh_trim(line);
            char *cmd2 = sh_trim(bar + 1);
            if (!*cmd1 || !*cmd2) {
                sh_puts("usage: cmd1 | cmd2\n");
            } else {
                foreground_pid = 0;
                run_pipeline(cmd1, cmd2);
            }
        } else if (has_redirect) {
            foreground_pid = 0;
            run_redirected(line);
        } else if (strncmp(line, "cat", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
            char *arg = sh_trim(line + 3);
            if (!*arg) {
                sh_puts("usage: cat <file>\n");
            } else {
                int pid = nos_exec("cat", arg);
                if (pid < 0) {
                    sh_puts("error: cat not found\n");
                } else {
                    foreground_pid = pid;
                    nos_wait(pid);   /* the output appears before the next prompt */
                    foreground_pid = 0;
                }
            }
        } else if (strncmp(line, "edit", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
            char *arg = line[4] == ' ' ? line + 5 : "";
            unsigned int alen = strlen(arg);
            if (alen > 0 && arg[alen - 1] == '\n') arg[alen - 1] = '\0';
            int pid = nos_exec("edit", arg);
            if (pid < 0) {
                sh_puts("error: edit not found\n");
            } else {
                foreground_pid = pid;
                nos_wait(pid);   /* blocks until the editor exits */
                foreground_pid = 0;
            }
        } else if (strncmp(line, "run", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
            char *name = line[3] == ' ' ? line + 4 : "";
            unsigned int nlen = strlen(name);
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
