/*
 * runtime.c - I/O shims and helpers (last1120 got these from a library).
 * last1120 lexes integers inline in symbol() and has no floats, so there is
 * no getnum/fcval here.
 */
#include "cc.h"

FILE *fin;                      /* lexer input  (set by main) */
FILE *fout;                     /* code output  (set by main) */

extern int line, nerror;

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
