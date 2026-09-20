/* nullos/user/edit.c — minimal nano-style text editor */
#include "lib/nullos.h"

/* ── VGA colors (subset) ──────────────────────────────────────────── */
#define VGA_BLACK      0
#define VGA_LIGHT_GREY 7
#define VGA_LIGHT_CYAN 11
#define VGA_WHITE      15
#define VGA_BLUE       1
#define VGA_CYAN       3

/* ── scancode → ASCII map ───────────────────────────────────── */
static const char sc_map[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',
    '\b', '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']',
    '\n', 0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*',
    0,    ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,
};

/* Same layout as sc_map, index-for-index, with Shift held (US QWERTY).
   Duplicates kernel/keyboard.c's scancode_map_shift on purpose: the
   kernel swallows the Shift make/break scancodes itself, so edit.c can
   never track Shift on its own — it reads the Shift state from bit 9 of
   the raw value (SYS_READ_RAW) and picks the table here. */
static const char sc_map_shift[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_',  '+',
    '\b', '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{',  '}',
    '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"',  '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,    '*',
    0,    ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,
};

/* ── string helpers ──────────────────────────────────────────── */
static unsigned ed_strlen(const char *s) {
    unsigned n = 0; while (s[n]) n++; return n;
}

static char *ed_uitoa(unsigned v, char *buf, unsigned sz) {
    buf[--sz] = '\0';
    if (v == 0) { buf[--sz] = '0'; return &buf[sz]; }
    while (v && sz > 0) { buf[--sz] = '0' + (v % 10); v /= 10; }
    return &buf[sz];
}

/* ── layout ─────────────────────────────────────────────────────── */
#define COLS      80
#define TEXT_ROWS 23
#define HELP_ROW  23
#define STAT_ROW  24
#define BUF_SIZE  4096

/* ── state ──────────────────────────────────────────────────────── */
static char     buf[BUF_SIZE];
static unsigned buf_len  = 0;
static unsigned cur      = 0;
static unsigned top_line = 0;
static char     filename[64];
static char     status[64];
static int      file_fd = -1;   /* fd kept open for writing */

/* ── navigation ───────────────────────────────────────────────────── */
static unsigned line_of(unsigned p) {
    unsigned l = 0;
    for (unsigned i = 0; i < p; i++) if (buf[i] == '\n') l++;
    return l;
}
static unsigned col_of(unsigned p) {
    unsigned c = 0;
    while (p > 0 && buf[p-1] != '\n') { p--; c++; }
    return c;
}
static unsigned line_start(unsigned n) {
    unsigned pos = 0, l = 0;
    while (pos < buf_len && l < n) { if (buf[pos] == '\n') l++; pos++; }
    return pos;
}
static unsigned line_end_of(unsigned p) {
    while (p < buf_len && buf[p] != '\n') p++;
    return p;
}
static unsigned total_lines(void) {
    unsigned l = 1;
    for (unsigned i = 0; i < buf_len; i++) if (buf[i] == '\n') l++;
    return l;
}

/* ── rendering ────────────────────────────────────────────────── */

/* writes exactly 'n' blank spaces */
static void write_spaces(unsigned n) {
    static const char sp32[32] = "                                ";
    while (n >= 32) { nos_write(1, sp32, 32); n -= 32; }
    if (n) nos_write(1, sp32, n);
}

static void render(void) {
    unsigned cur_line = line_of(cur);
    unsigned cur_col  = col_of(cur);

    /* adjust the viewport */
    if (cur_line < top_line) top_line = cur_line;
    if (cur_line >= top_line + TEXT_ROWS) top_line = cur_line - TEXT_ROWS + 1;

    nos_clear();  /* clears the screen and resets the cursor to (0,0) */

    /* ── content lines ── */
    nos_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    unsigned pos = line_start(top_line);
    for (unsigned row = 0; row < TEXT_ROWS; row++) {
        nos_gotoxy(0, row);
        unsigned col = 0;
        while (pos < buf_len && buf[pos] != '\n' && col < COLS - 1) {
            nos_write(1, &buf[pos], 1);
            pos++; col++;
        }
        if (pos < buf_len && buf[pos] == '\n') pos++;
        /* pads up to COLS-1: never advance to the next VGA line */
        write_spaces(COLS - 1 - col);
    }

    /* ── help bar (line 23) ── */
    nos_gotoxy(0, HELP_ROW);
    nos_setcolor(VGA_BLACK, VGA_LIGHT_GREY);
    const char *help = "^S save  ^Q quit  Arrows: navigate";
    unsigned hlen = ed_strlen(help);
    if (hlen > COLS - 1) hlen = COLS - 1;
    nos_write(1, help, hlen);
    write_spaces(COLS - 1 - hlen);

    /* ── status bar (line 24) ──────────────────────────────────
       We write at most COLS-1 chars: the last cell (col=79) is
       left untouched to keep vga_putchar from triggering vga_scroll(). */
    nos_gotoxy(0, STAT_ROW);
    nos_setcolor(VGA_WHITE, VGA_BLUE);
    char nbuf[12];
    /* builds the status string in a local buffer and writes it in one go */
    static char sbar[80];
    unsigned si = 0;
    sbar[si++] = ' ';
    const char *fname = filename[0] ? filename : "[no name]";
    for (unsigned i = 0; fname[i] && si < COLS - 2; i++) sbar[si++] = fname[i];
    sbar[si++] = ' '; sbar[si++] = ' ';
    if (si < COLS - 2) { sbar[si++] = 'L'; sbar[si++] = ':'; }
    const char *ln = ed_uitoa(cur_line + 1, nbuf, sizeof(nbuf));
    for (unsigned i = 0; ln[i] && si < COLS - 2; i++) sbar[si++] = ln[i];
    if (si < COLS - 2) { sbar[si++] = ' '; sbar[si++] = 'C'; sbar[si++] = ':'; }
    const char *cn = ed_uitoa(cur_col + 1, nbuf, sizeof(nbuf));
    for (unsigned i = 0; cn[i] && si < COLS - 2; i++) sbar[si++] = cn[i];
    if (status[0] && si < COLS - 2) {
        sbar[si++] = ' '; sbar[si++] = ' ';
        for (unsigned i = 0; status[i] && si < COLS - 2; i++) sbar[si++] = status[i];
    }
    /* pad with spaces up to COLS-1 (not COLS!) */
    while (si < COLS - 1) sbar[si++] = ' ';
    nos_write(1, sbar, COLS - 1);  /* exactly 79 chars — no wrap, no scroll */

    /* restores the color and positions the hardware cursor at the edit point */
    nos_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    unsigned vcol = cur_col < (unsigned)(COLS - 1) ? cur_col : (unsigned)(COLS - 2);
    nos_gotoxy(vcol, cur_line - top_line);
}

/* ── editing ──────────────────────────────────────────────────────── */
static void insert_char(char c) {
    if (buf_len >= BUF_SIZE - 1) return;
    for (unsigned i = buf_len; i > cur; i--) buf[i] = buf[i-1];
    buf[cur++] = c; buf_len++;
}
static void delete_before(void) {
    if (cur == 0) return;
    cur--; buf_len--;
    for (unsigned i = cur; i < buf_len; i++) buf[i] = buf[i+1];
}

/* ── movement ────────────────────────────────────────────────── */
static void move_up(void) {
    unsigned cl = line_of(cur), cc = col_of(cur);
    if (cl == 0) return;
    unsigned ps = line_start(cl-1), pe = line_end_of(ps), pl = pe - ps;
    cur = ps + (cc < pl ? cc : pl);
}
static void move_down(void) {
    unsigned cl = line_of(cur), cc = col_of(cur);
    if (cl + 1 >= total_lines()) return;
    unsigned ns = line_start(cl+1), ne = line_end_of(ns), nl = ne - ns;
    cur = ns + (cc < nl ? cc : nl);
}
static void move_left(void)  { if (cur > 0) cur--; }
static void move_right(void) { if (cur < buf_len) cur++; }

/* ── loads the file ─────────────────────────────────────────────── */
static void load_file(void) {
    file_fd = nos_open(filename);
    if (file_fd < 0) {
        /* new file: create it already open so it can be saved later */
        file_fd = nos_create(filename);
        buf[0] = '\0'; buf_len = 0;
        return;
    }
    char tmp;
    while (buf_len < BUF_SIZE - 1) {
        int r = nos_read(file_fd, &tmp, 1);
        if (r <= 0) break;
        buf[buf_len++] = tmp;
    }
    buf[buf_len] = '\0';
    /* keeps file_fd open for later writes via Ctrl+S */
}

/* ── entry point ─────────────────────────────────────────────────── */
void _start(void) {
    int n = nos_getarg(filename, sizeof(filename));
    if (n <= 0) filename[0] = '\0';

    buf_len = 0; cur = 0; top_line = 0; status[0] = '\0';
    if (filename[0]) load_file();

    nos_kbd_flush();
    nos_set_raw_mode(1);

    for (;;) {
        render();

        unsigned raw  = nos_read_raw();
        unsigned sc   = raw & 0xFF;
        int      ctrl = (raw & 0x100) != 0;
        int      shift = (raw & 0x200) != 0;

        status[0] = '\0';

        if (ctrl) {
            if (sc == 0x1F) {      /* Ctrl+S: S = scancode 0x1F */
                const char *m;
                if (file_fd >= 0 && nos_write_file(file_fd, buf, buf_len) == 0)
                    m = "saved";
                else
                    m = "saved (no disk)";
                unsigned i = 0;
                while (m[i] && i < 63) { status[i] = m[i]; i++; }
                status[i] = '\0';
            } else if (sc == 0x10) { /* Ctrl+Q: Q = scancode 0x10 */
                nos_set_raw_mode(0);
                if (file_fd >= 0) nos_close(file_fd);
                nos_clear();
                nos_gotoxy(0, 0);
                nos_kbd_flush();
                nos_exit(0);
            }
            continue;
        }

        switch (sc) {
            case 0x48: move_up();       break;  /* up arrow    */
            case 0x50: move_down();     break;  /* down arrow  */
            case 0x4B: move_left();     break;  /* left arrow  */
            case 0x4D: move_right();    break;  /* right arrow */
            case 0x0E: delete_before(); break;  /* backspace   */
            case 0x1C: insert_char('\n'); break; /* enter      */
            default: {
                char c = (sc < 128) ? (shift ? sc_map_shift[sc] : sc_map[sc]) : 0;
                if (c >= 0x20 && (unsigned char)c < 0x7F)
                    insert_char(c);
                break;
            }
        }
    }
}
