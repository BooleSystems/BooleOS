// nullos/kernel/safeshell.c — Safe Mode's restricted shell (see safeshell.h).
//
// Fixed-size static buffers only; no allocation of its own (the FAT16 driver
// uses the heap internally). Text goes through msg().

#include "safeshell.h"
#include "hal.h"
#include "messages.h"
#include "fs/fat16.h"

#define LINE_MAX 128
#define KEY_ESC  27
#define KEY_CTRL_C 3

static char     g_line[LINE_MAX];
static char     g_path[128];          // scratch for pwd
static char     g_chunk[512];         // scratch for cat
static uint32_t g_cwd = 0;            // current directory cluster (0 = root)

// ── helpers ──────────────────────────────────────────────────────────

static int read_key(void) {
    for (;;) {
        int c = input_poll_key();
        if (c != -1) return c;
        __asm__ volatile ("hlt");     // interrupts are on: the keyboard IRQ wakes us
    }
}

// Reads one line into g_line with simple backspace editing. Only printable
// ASCII is accepted; the buffer never overflows.
static void read_line(void) {
    int len = 0;
    for (;;) {
        int c = read_key();
        if (c == '\n') { console_putc('\n'); break; }
        if (c == '\b') {
            if (len > 0) { len--; console_putc('\b'); }
            continue;
        }
        if (c >= 32 && c < 127 && len < LINE_MAX - 1) {
            g_line[len++] = (char)c;
            console_putc((char)c);
        }
        // anything else (ESC, arrows, ...): ignored
    }
    g_line[len] = '\0';
}

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

// Splits g_line in place into a command word and the rest (leading/trailing
// spaces removed). *arg points to "" when there is no argument.
static void split_line(char **cmd, char **arg) {
    char *p = g_line;
    while (*p == ' ') p++;
    *cmd = p;
    while (*p && *p != ' ') p++;
    if (*p) { *p++ = '\0'; }
    while (*p == ' ') p++;
    *arg = p;
    int n = 0;
    while ((*arg)[n]) n++;
    while (n > 0 && (*arg)[n - 1] == ' ') (*arg)[--n] = '\0';
}

static void put_padded(const char *s, int width) {
    int n = 0;
    while (s[n]) { console_putc(s[n]); n++; }
    while (n < width) { console_putc(' '); n++; }
}

// ── builtins ─────────────────────────────────────────────────────────

static void cmd_ls(const char *arg) {
    uint32_t dir = g_cwd;
    if (*arg) {
        int r = fat16_resolve_dir(g_cwd, arg, &dir);
        if (r == 0)  { console_puts(msg(MSG_SAFESH_LS_NO_DIR));  console_puts(arg); console_puts("\n"); return; }
        if (r == -2) { console_puts(msg(MSG_SAFESH_LS_NOT_DIR)); console_puts(arg); console_puts("\n"); return; }
        if (r < 0)   { console_puts(msg(MSG_SAFESH_LS_IO)); return; }
    }

    char     name[13];
    uint32_t size;
    uint8_t  is_dir;
    int any = 0;
    for (uint32_t i = 0; fat16_readdir(dir, i, name, &size, &is_dir); i++) {
        any = 1;
        console_puts("  ");
        put_padded(name, 14);
        if (is_dir) {
            console_puts(msg(MSG_SAFESH_DIR_ENTRY));
        } else {
            console_put_dec(size);
            console_puts(msg(MSG_SAFESH_BYTES_NL));
        }
    }
    if (!any) console_puts(msg(MSG_SAFESH_LS_EMPTY));
}

static void cmd_cat(const char *arg) {
    if (!*arg) { console_puts(msg(MSG_SAFESH_CAT_USAGE)); return; }

    uint32_t first, size;
    int r = fat16_find(g_cwd, arg, &first, &size, 0);
    if (r == 0)  { console_puts(msg(MSG_SAFESH_CAT_NOT_FOUND)); console_puts(arg); console_puts("\n"); return; }
    if (r < 0)   { console_puts(msg(MSG_SAFESH_CAT_IO));        console_puts(arg); console_puts("\n"); return; }

    uint32_t pos = 0;
    while (pos < size) {
        int k = input_poll_key();                 // ESC or Ctrl+C stops a long file
        if (k == KEY_ESC || k == KEY_CTRL_C) { console_puts(msg(MSG_SAFESH_STOPPED)); return; }

        uint32_t want = size - pos;
        if (want > sizeof(g_chunk)) want = sizeof(g_chunk);
        int got = fat16_read_at(first, pos, g_chunk, want, size);
        if (got <= 0) { console_puts(msg(MSG_SAFESH_CAT_READ_ERROR)); return; }

        for (int i = 0; i < got; i++) {
            char c = g_chunk[i];
            if (c == '\n')        console_putc('\n');
            else if (c == '\r')   { /* skip */ }
            else if (c == '\t')   console_putc(' ');
            else if (c >= 32 && c < 127) console_putc(c);
            else                  console_putc('.');
        }
        pos += (uint32_t)got;
    }
    console_putc('\n');
}

static void cmd_pwd(void) {
    if (fat16_get_path(g_cwd, g_path, sizeof(g_path)) < 0) {
        console_puts(msg(MSG_SAFESH_PWD_ERR));
        return;
    }
    console_puts(g_path);
    console_putc('\n');
}

static void cmd_cd(const char *arg) {
    if (!*arg) { g_cwd = 0; return; }          // no argument: the root
    uint32_t dir;
    int r = fat16_resolve_dir(g_cwd, arg, &dir);
    if (r == 0)  { console_puts(msg(MSG_SAFESH_CD_NO_DIR));  console_puts(arg); console_puts("\n"); return; }
    if (r == -2) { console_puts(msg(MSG_SAFESH_CD_NOT_DIR)); console_puts(arg); console_puts("\n"); return; }
    if (r < 0)   { console_puts(msg(MSG_SAFESH_CD_IO)); return; }
    g_cwd = dir;
}

// ── main loop ────────────────────────────────────────────────────────

void safeshell_run(void) {
    for (;;) {
        console_puts(msg(MSG_SAFESH_PROMPT));
        read_line();

        char *cmd, *arg;
        split_line(&cmd, &arg);
        if (!*cmd) continue;

        // These command names are compared, not printed: they stay literals
        // (they are identifiers, not messages).
        if      (streq(cmd, "help")) console_puts(msg(MSG_SAFESH_HELP));
        else if (streq(cmd, "ls"))   cmd_ls(arg);
        else if (streq(cmd, "cat"))  cmd_cat(arg);
        else if (streq(cmd, "pwd"))  cmd_pwd();
        else if (streq(cmd, "cd"))   cmd_cd(arg);
        else if (streq(cmd, "back")) return;
        else {
            console_puts(msg(MSG_SAFESH_UNKNOWN));
            console_puts(cmd);
            console_putc('\n');
        }
    }
}
