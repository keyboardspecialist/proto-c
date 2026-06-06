/*
 * runtime.c - I/O shims and helpers (last1120 got these from a library).
 * last1120 lexes integers inline in symbol() and has no floats, so there is
 * no getnum/fcval here.
 */
#include "cc.h"

FILE *fin;                      /* lexer input  (set by main) */
FILE *fout;                     /* code output  (set by main) */

extern int line, nerror;

/* ---- input-source stack ----
 * The lexer reads characters through c_getchar(). Beyond the base file (fin),
 * sources can be pushed: a string (manifest-constant substitution) or another
 * file (%filename inclusion). Reads come from the top of the stack; an
 * exhausted source pops and reading resumes below. */
struct src { FILE *f; char *s; int pos; int ch; };
static struct src srcstk[64];
static int srctop = -1;

void src_push_str(char *s)      /* substitute manifest text */
{
        if (srctop < 63) { srctop++; srcstk[srctop].f = 0; srcstk[srctop].s = s; srcstk[srctop].pos = 0; srcstk[srctop].ch = 0; }
}
void src_push_char(int c)       /* unget a single char (read after the source above it) */
{
        if (c && srctop < 63) { srctop++; srcstk[srctop].f = 0; srcstk[srctop].s = 0; srcstk[srctop].pos = 0; srcstk[srctop].ch = c; }
}
void src_push_file(char *name)  /* %filename inclusion */
{
        FILE *f = fopen(name, "r");
        if (f == NULL) { error("cannot include %s", name); return; }
        if (srctop < 63) { srctop++; srcstk[srctop].f = f; srcstk[srctop].s = 0; srcstk[srctop].pos = 0; srcstk[srctop].ch = 0; }
}

/* read one raw char from the current source, popping exhausted ones; 0 at end */
static int rawget(void)
{
        int c;
        while (srctop >= 0) {
                struct src *t = &srcstk[srctop];
                if (t->ch) { c = t->ch; srctop--; return c; }
                if (t->s) {
                        c = (unsigned char) t->s[t->pos];
                        if (c == 0) { srctop--; continue; }
                        t->pos++;
                        return c;
                }
                c = getc(t->f);
                if (c == EOF) { fclose(t->f); srctop--; continue; }
                return c;
        }
        if (fin == NULL)
                return 0;
        c = getc(fin);
        return (c == EOF) ? 0 : c;
}

int c_getchar(void)
{
        static int atcol0 = 1;          /* next char begins a line */
        int c;
        for (;;) {
                c = rawget();
                if (c == 0)
                        return 0;
                if (atcol0 && c == '%') {       /* %filename inclusion directive */
                        char nm[256];
                        int n = 0, ch;
                        while ((ch = rawget()) != '\n' && ch != 0)
                                if (n < 255) nm[n++] = ch;
                        while (n > 0 && (nm[n - 1] == ' ' || nm[n - 1] == '\t' || nm[n - 1] == '\r'))
                                n--;
                        nm[n] = '\0';
                        src_push_file(nm);
                        atcol0 = 1;
                        continue;
                }
                atcol0 = (c == '\n');
                return c;
        }
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
