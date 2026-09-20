/* nullos/user/lib/nosstdio.c — a minimal printf family for user programs.
 *
 * printf / vprintf / sprintf / snprintf / vsnprintf, with the standard libc
 * names and signatures on purpose (like memcpy/strlen in nullos.c): code written
 * for a normal libc, such as a port of an existing program, compiles and links
 * unchanged. There is no libc here — this is the whole implementation, C99,
 * using only <stdarg.h>/<stddef.h>/<stdint.h> (compiler headers, not libc).
 *
 * Supported: %d %i %u %x %X %c %s %p %%, the flags '-' '0' '+' ' ', a field
 * width (digits or '*'), a precision (".N" or ".*": at least N digits for the
 * integer conversions, at most N characters for %s), and the length modifiers
 * 'h', 'hh' (the value is converted to short / char first, as C specifies)
 * and 'l' (accepted: long is 32 bits here, the same as int).
 * NOT supported: floating point (%f %e %g), 64-bit integers (%lld — an
 * unsupported conversion is printed literally, e.g. "%lld" -> "%lld"), %n, and
 * positional arguments. Every sink is bounded by the caller's buffer, never by
 * this code, except sprintf() (see its comment).
 *
 * Linked only into the programs that use it (see user/Makefile, LIBSTDIO): every
 * program that links this object carries it.
 */
#include "nullos.h"
#include <stdarg.h>

/* ── output sinks ─────────────────────────────────────────────────── */

typedef struct sink {
    void  (*put)(struct sink *s, char c);
    char   *buf;        /* snprintf: destination; printf: staging buffer */
    size_t  cap;        /* snprintf: size of buf; printf: staging size */
    size_t  len;        /* characters produced (snprintf: may exceed cap) */
} sink_t;

/* snprintf sink: stores while there is room for the character AND the final
   NUL, always counts. */
static void put_mem(sink_t *s, char c) {
    if (s->cap && s->len + 1 < s->cap) s->buf[s->len] = c;
    s->len++;
}

/* printf sink: stages up to cap characters, flushed to fd 1 when full. */
static void put_out(sink_t *s, char c) {
    /* s->len counts everything produced; the staged count is len % cap */
    s->buf[s->len % s->cap] = c;
    s->len++;
    if (s->len % s->cap == 0) nos_write(1, s->buf, (unsigned)s->cap);
}

static void flush_out(sink_t *s) {
    size_t staged = s->len % s->cap;
    if (staged) nos_write(1, s->buf, (unsigned)staged);
}

/* ── the formatter ────────────────────────────────────────────────── */

typedef struct {
    int left;       /* '-' */
    int zero;       /* '0' */
    int plus;       /* '+' */
    int space;      /* ' ' */
    int width;      /* -1 = none */
    int prec;       /* -1 = none */
} spec_t;

static void pad(sink_t *s, char c, int n) {
    while (n-- > 0) s->put(s, c);
}

/* Prints an unsigned magnitude in `base` with the sign/prefix handling done by
   the caller in `sign` (0, '-', '+' or ' ') and `prefix` ("0x" or ""). */
static void emit_number(sink_t *s, const spec_t *sp, unsigned int v, unsigned base,
                        int upper, char sign, const char *prefix) {
    char digits[12];                       /* 32 bits in base 10 = 10 digits */
    int  nd = 0;
    const char *set = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (v == 0) { if (sp->prec != 0) digits[nd++] = '0'; }   /* "%.0d" of 0 is empty */
    while (v) { digits[nd++] = set[v % base]; v /= base; }

    int zeros = (sp->prec > nd) ? sp->prec - nd : 0;         /* precision: min digits */
    int plen = 0;
    while (prefix[plen]) plen++;
    int total = nd + zeros + plen + (sign ? 1 : 0);
    int fill  = (sp->width > total) ? sp->width - total : 0;

    /* '0' flag pads with zeros after the sign/prefix; ignored with '-' or a precision */
    int zero_pad = sp->zero && !sp->left && sp->prec < 0;

    if (!sp->left && !zero_pad) pad(s, ' ', fill);
    if (sign) s->put(s, sign);
    for (int i = 0; i < plen; i++) s->put(s, prefix[i]);
    if (zero_pad) pad(s, '0', fill);
    pad(s, '0', zeros);
    while (nd) s->put(s, digits[--nd]);
    if (sp->left) pad(s, ' ', fill);
}

static void emit_string(sink_t *s, const spec_t *sp, const char *str) {
    if (!str) str = "(null)";
    int n = 0;
    while (str[n] && (sp->prec < 0 || n < sp->prec)) n++;
    int fill = (sp->width > n) ? sp->width - n : 0;

    if (!sp->left) pad(s, ' ', fill);
    for (int i = 0; i < n; i++) s->put(s, str[i]);
    if (sp->left) pad(s, ' ', fill);
}

static void format(sink_t *s, const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') { s->put(s, *fmt); continue; }

        const char *start = fmt;           /* for printing an unsupported spec literally */
        fmt++;

        spec_t sp = { 0, 0, 0, 0, -1, -1 };

        /* flags */
        for (;; fmt++) {
            if      (*fmt == '-') sp.left = 1;
            else if (*fmt == '0') sp.zero = 1;
            else if (*fmt == '+') sp.plus = 1;
            else if (*fmt == ' ') sp.space = 1;
            else break;
        }

        /* width */
        if (*fmt == '*') {
            int w = va_arg(ap, int);
            if (w < 0) { sp.left = 1; w = -w; }
            sp.width = w;
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                if (sp.width < 0) sp.width = 0;
                if (sp.width < 100000) sp.width = sp.width * 10 + (*fmt - '0');
                fmt++;
            }
        }

        /* precision */
        if (*fmt == '.') {
            fmt++;
            sp.prec = 0;
            if (*fmt == '*') {
                int p = va_arg(ap, int);
                sp.prec = (p < 0) ? -1 : p;
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    if (sp.prec < 100000) sp.prec = sp.prec * 10 + (*fmt - '0');
                    fmt++;
                }
            }
        }

        /* length: 'h' and 'hh' narrow the value, 'l' is a no-op (32-bit long),
           'll' is not supported */
        int ll = 0, lm = 0;                 /* lm: 0 none, 1 h, 2 hh */
        if (*fmt == 'h') { fmt++; lm = 1; if (*fmt == 'h') { fmt++; lm = 2; } }
        else if (*fmt == 'l') { fmt++; if (*fmt == 'l') { ll = 1; fmt++; } }

        char c = *fmt;
        if (c == '\0') {                    /* a lone '%' (or an incomplete spec) at the end */
            for (const char *p = start; p < fmt; p++) s->put(s, *p);
            break;
        }

        if (ll && (c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X')) c = '?';

        switch (c) {
        case 'd': case 'i': {
            int v = va_arg(ap, int);
            if (lm == 1) v = (short)v; else if (lm == 2) v = (signed char)v;
            unsigned int mag = (v < 0) ? (0u - (unsigned int)v) : (unsigned int)v;
            char sign = (v < 0) ? '-' : sp.plus ? '+' : sp.space ? ' ' : 0;
            emit_number(s, &sp, mag, 10, 0, sign, "");
            break;
        }
        case 'u': case 'x': case 'X': {
            unsigned int v = va_arg(ap, unsigned int);
            if (lm == 1) v = (unsigned short)v; else if (lm == 2) v = (unsigned char)v;
            emit_number(s, &sp, v, c == 'u' ? 10 : 16, c == 'X', 0, "");
            break;
        }
        case 'p': {
            spec_t ps = sp;
            ps.prec = -1;
            emit_number(s, &ps, (unsigned int)(uintptr_t)va_arg(ap, void *), 16, 0, 0, "0x");
            break;
        }
        case 'c': {
            char ch = (char)va_arg(ap, int);
            int fill = (sp.width > 1) ? sp.width - 1 : 0;
            if (!sp.left) pad(s, ' ', fill);
            s->put(s, ch);
            if (sp.left) pad(s, ' ', fill);
            break;
        }
        case 's':
            emit_string(s, &sp, va_arg(ap, const char *));
            break;
        case '%':
            s->put(s, '%');
            break;
        default:                            /* unsupported (floats, %lld, %n, ...): print it literally */
            for (const char *p = start; p <= fmt; p++) s->put(s, *p);
            break;
        }
    }
}

/* ── the public functions ─────────────────────────────────────────── */

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    sink_t s = { put_mem, buf, size, 0 };
    format(&s, fmt, ap);
    if (size) buf[(s.len < size) ? s.len : size - 1] = '\0';   /* always terminated */
    return (int)s.len;                     /* what WOULD have been written (C99) */
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

/* Unbounded: the caller must guarantee buf is large enough. Prefer snprintf(). */
int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, (size_t)-1 >> 1, fmt, ap);
    va_end(ap);
    return n;
}

int vprintf(const char *fmt, va_list ap) {
    char stage[128];
    sink_t s = { put_out, stage, sizeof(stage), 0 };
    format(&s, fmt, ap);
    flush_out(&s);
    return (int)s.len;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}
