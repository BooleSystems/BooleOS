/* nullos/user/edit.c — editor de texto mínimo estilo nano */

typedef unsigned int  uint32_t;
typedef unsigned char uint8_t;

/* ── syscall wrappers ───────────────────────────────────────────── */
/* Todas as funções que retornam void usam "=a"(ret) para descartar
   o valor de retorno do kernel e evitar que o compilador reutilize
   eax como argumento da próxima instrução int $0x80.              */

static void sys_exit(int code) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(2), "b"(code) : "memory");
    (void)ret; for (;;);
}

static int sys_open(const char *name) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(11), "b"(name) : "memory");
    return ret;
}

static int sys_close(int fd) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(12), "b"(fd) : "memory");
    return ret;
}

static int sys_read_fd(int fd, char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(5), "b"(fd), "c"(buf), "d"(len) : "memory");
    return ret;
}

/* retorna scancode|(ctrl?0x100:0), bloqueante */
static unsigned sys_read_raw(void) {
    unsigned ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(13) : "memory");
    return ret;
}

static void sys_gotoxy(unsigned col, unsigned row) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(14), "b"(col), "c"(row) : "memory");
    (void)ret;
}

static void sys_clear(void) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(15) : "memory");
    (void)ret;
}

static int sys_getarg(char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(16), "b"(buf), "c"(len) : "memory");
    return ret;
}

static void sys_kbd_flush(void) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(17) : "memory");
    (void)ret;
}

static void sys_set_raw_mode(unsigned enable) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(19), "b"(enable) : "memory");
    (void)ret;
}

/* fg/bg: valores da enum vga_color_t do kernel (0-15) */
static void sys_set_color(unsigned fg, unsigned bg) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(18), "b"(fg), "c"(bg) : "memory");
    (void)ret;
}

static void sys_write(const char *buf, unsigned len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(1), "b"(1), "c"(buf), "d"(len) : "memory");
    (void)ret;
}

/* ── cores VGA (subset) ──────────────────────────────────────────── */
#define VGA_BLACK      0
#define VGA_LIGHT_GREY 7
#define VGA_LIGHT_CYAN 11
#define VGA_WHITE      15
#define VGA_BLUE       1
#define VGA_CYAN       3

/* ── mapa de scancodes → ASCII ───────────────────────────────────── */
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

/* ── helpers de string ──────────────────────────────────────────── */
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

/* ── estado ──────────────────────────────────────────────────────── */
static char     buf[BUF_SIZE];
static unsigned buf_len  = 0;
static unsigned cur      = 0;
static unsigned top_line = 0;
static char     filename[64];
static char     status[64];

/* ── navegação ───────────────────────────────────────────────────── */
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

/* ── renderização ────────────────────────────────────────────────── */

/* escreve exatamente 'n' espaços em branco */
static void write_spaces(unsigned n) {
    static const char sp32[32] = "                                ";
    while (n >= 32) { sys_write(sp32, 32); n -= 32; }
    if (n) sys_write(sp32, n);
}

static void render(void) {
    unsigned cur_line = line_of(cur);
    unsigned cur_col  = col_of(cur);

    /* ajusta viewport */
    if (cur_line < top_line) top_line = cur_line;
    if (cur_line >= top_line + TEXT_ROWS) top_line = cur_line - TEXT_ROWS + 1;

    sys_clear();  /* limpa tela e reseta cursor para (0,0) */

    /* ── linhas de conteúdo ── */
    sys_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    unsigned pos = line_start(top_line);
    for (unsigned row = 0; row < TEXT_ROWS; row++) {
        sys_gotoxy(0, row);
        unsigned col = 0;
        while (pos < buf_len && buf[pos] != '\n' && col < COLS - 1) {
            sys_write(&buf[pos], 1);
            pos++; col++;
        }
        if (pos < buf_len && buf[pos] == '\n') pos++;
        /* preenche até COLS-1: nunca avança para a próxima linha no VGA */
        write_spaces(COLS - 1 - col);
    }

    /* ── barra de ajuda (linha 23) ── */
    sys_gotoxy(0, HELP_ROW);
    sys_set_color(VGA_BLACK, VGA_LIGHT_GREY);
    const char *help = "^S salvar  ^Q sair  Setas: navegar";
    unsigned hlen = ed_strlen(help);
    if (hlen > COLS - 1) hlen = COLS - 1;
    sys_write(help, hlen);
    write_spaces(COLS - 1 - hlen);

    /* ── barra de status (linha 24) ──────────────────────────────────
       Escrevemos no máximo COLS-1 chars: o último cell (col=79) fica
       intocado para evitar que vga_putchar dispare vga_scroll().     */
    sys_gotoxy(0, STAT_ROW);
    sys_set_color(VGA_WHITE, VGA_BLUE);
    char nbuf[12];
    /* monta string de status num buffer local e escreve de uma vez */
    static char sbar[80];
    unsigned si = 0;
    sbar[si++] = ' ';
    const char *fname = filename[0] ? filename : "[sem nome]";
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
    /* pad com espaços até COLS-1 (não COLS!) */
    while (si < COLS - 1) sbar[si++] = ' ';
    sys_write(sbar, COLS - 1);  /* exatamente 79 chars — sem wrap, sem scroll */

    /* restaura cor e posiciona o cursor de hardware no ponto de edição */
    sys_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    unsigned vcol = cur_col < (unsigned)(COLS - 1) ? cur_col : (unsigned)(COLS - 2);
    sys_gotoxy(vcol, cur_line - top_line);
}

/* ── edição ──────────────────────────────────────────────────────── */
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

/* ── movimentação ────────────────────────────────────────────────── */
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

/* ── carrega arquivo ─────────────────────────────────────────────── */
static void load_file(void) {
    int fd = sys_open(filename);
    if (fd < 0) { buf[0] = '\0'; buf_len = 0; return; }
    char tmp;
    while (buf_len < BUF_SIZE - 1) {
        int r = sys_read_fd(fd, &tmp, 1);
        if (r <= 0) break;
        buf[buf_len++] = tmp;
    }
    buf[buf_len] = '\0';
    sys_close(fd);
}

/* ── entry point ─────────────────────────────────────────────────── */
void _start(void) {
    int n = sys_getarg(filename, sizeof(filename));
    if (n <= 0) filename[0] = '\0';

    buf_len = 0; cur = 0; top_line = 0; status[0] = '\0';
    if (filename[0]) load_file();

    sys_kbd_flush();
    sys_set_raw_mode(1);

    for (;;) {
        render();

        unsigned raw  = sys_read_raw();
        unsigned sc   = raw & 0xFF;
        int      ctrl = (raw & 0x100) != 0;

        status[0] = '\0';

        if (ctrl) {
            if (sc == 0x1F) {      /* Ctrl+S: S = scancode 0x1F */
                const char *m = "salvo (sem disco)";
                unsigned i = 0;
                while (m[i] && i < 63) { status[i] = m[i]; i++; }
                status[i] = '\0';
            } else if (sc == 0x10) { /* Ctrl+Q: Q = scancode 0x10 */
                sys_set_raw_mode(0);
                sys_clear();
                sys_gotoxy(0, 0);
                sys_kbd_flush();
                sys_exit(0);
            }
            continue;
        }

        switch (sc) {
            case 0x48: move_up();       break;  /* seta cima  */
            case 0x50: move_down();     break;  /* seta baixo */
            case 0x4B: move_left();     break;  /* seta esq   */
            case 0x4D: move_right();    break;  /* seta dir   */
            case 0x0E: delete_before(); break;  /* backspace  */
            case 0x1C: insert_char('\n'); break; /* enter     */
            default: {
                char c = (sc < 128) ? sc_map[sc] : 0;
                if (c >= 0x20 && (unsigned char)c < 0x7F)
                    insert_char(c);
                break;
            }
        }
    }
}
