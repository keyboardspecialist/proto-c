/*
 * runtime.c - I/O shims and lexer helpers that the original compiler got from
 * a separate hand-written library (also absent from the prestruct-c tape).
 * Reimplemented against stdio for native execution.
 */
#include "cc.h"

FILE *fin;                      /* lexer input  (set by main) */
FILE *fout;                     /* code output  (set by main) */

/* globals defined in c00.c that we touch here */
extern int peekc, cval, ctyp, line, nerror;
extern double fcval;

/* one input character, 0 at end of file (matches the old library) */
int c_getchar(void)
{
        int c;
        if (fin == NULL)
                return 0;
        c = getc(fin);
        return (c == EOF) ? 0 : c;
}

int c_putchar(int c)
{
        putc(c, fout ? fout : stdout);
        return c;
}

void flush(void)
{
        if (fout)
                fflush(fout);
}

/*
 * getnum - lex a numeric constant in the given base. Sets cval/ctyp for an
 * integer or fcval for a float, and pushes the terminating character back via
 * peekc. Returns the token the lexer expects:
 *   21 integer constant
 *   23 non-zero float, 25 zero float
 *   39 the structure member operator '.' (a lone '.' is not a number)
 */
int getnum(int base)
{
        int c, n, isflt;
        double scale;

        c = peekc;
        peekc = 0;
        if (c == 0)
                c = getchar();
        n = 0;
        isflt = 0;
        fcval = 0.0;
        scale = 1.0;

        if (c == '.') {
                int d = getchar();
                if (d < '0' || d > '9') {       /* member operator, not a number */
                        peekc = d;
                        return 39;
                }
                isflt = 1;
                for (c = d; c >= '0' && c <= '9'; c = getchar()) {
                        scale /= 10.0;
                        fcval += (c - '0') * scale;
                }
        } else {
                int top = (base == 8) ? '7' : '9';
                for (; c >= '0' && c <= top; c = getchar())
                        n = n * base + (c - '0');
                fcval = (double) n;
                if (c == '.') {
                        isflt = 1;
                        for (c = getchar(); c >= '0' && c <= '9'; c = getchar()) {
                                scale /= 10.0;
                                fcval += (c - '0') * scale;
                        }
                }
        }
        if (c == 'e' || c == 'E') {
                int eneg = 0, ev = 0;
                isflt = 1;
                c = getchar();
                if (c == '+')
                        c = getchar();
                else if (c == '-') {
                        eneg = 1;
                        c = getchar();
                }
                for (; c >= '0' && c <= '9'; c = getchar())
                        ev = ev * 10 + (c - '0');
                while (ev-- > 0)
                        fcval = eneg ? fcval / 10.0 : fcval * 10.0;
        }
        peekc = c;
        if (isflt) {
                ctyp = 3;                       /* double */
                return (fcval == 0.0) ? 25 : 23;
        }
        cval = n;
        ctyp = 0;                               /* int */
        return 21;
}

/* unpack a packed symbol name (NWPS words, NCPW chars/word) into buf */
char *namestr(word *name, char *buf)
{
        int i, ch;
        for (i = 0; i < NAMSIZ; i++) {
                ch = (int) ((name[i / NCPW] >> (8 * (i % NCPW))) & 0177);
                if (ch == 0)
                        break;
                buf[i] = (char) ch;
        }
        buf[i] = '\0';
        return buf;
}

void pname(word *name)
{
        char buf[NAMSIZ + 1];
        fputs(namestr(name, buf), fout ? fout : stdout);
}

/* diagnostic, printf-style. Formats with the standard library now, so the few
 * original "%p" (name) call sites are rewritten to pass a string instead. */
void error(char *s, ...)
{
        va_list ap;
        nerror++;
        flush();
        fprintf(stderr, "%d: ", line);
        va_start(ap, s);
        vfprintf(stderr, s, ap);
        va_end(ap);
        fputc('\n', stderr);
}
