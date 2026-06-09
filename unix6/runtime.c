/*
 * runtime.c - I/O shims and lexer helpers that the original compiler got from
 * a separate hand-written library (also absent from the prestruct-c tape).
 * Reimplemented against stdio for native execution.
 */
#include "cc.h"
#include <string.h>

FILE *fin;                      /* lexer input  (set by main) */
FILE *fout;                     /* code output  (set by main) */

/* globals defined in c00.c that we touch here */
extern int peekc, cval, ctyp, line, nerror;
extern double fcval;

/* ------------------------------------------------------------------------
 * Preprocessor (1975 C Reference Manual, §12): #define token replacement and
 * #include file inclusion. The lexer reads characters through a source stack:
 * the base file sits at the bottom, while #include splices a file and a macro
 * use splices its replacement text on top. A macro is not expanded while its
 * own replacement is being scanned, which bounds recursion (§12.1: "no
 * rescanning of the replacement string").
 * ---------------------------------------------------------------------- */

#define MAXSRC 64
struct src { FILE *f; char *s; int pos; int mac; char cbuf[2]; };  /* file XOR string; mac id or -1 */
static struct src srcstk[MAXSRC];
static int nsrc;                /* 0 => read the base file `fin` */
static int ungot = -1;          /* one-char pushback */

#define MAXMAC 512
static struct { char name[NAMSIZ + 1]; char *text; int active; } macro[MAXMAC];
static int nmac;

static void pp_pop(void)
{
        struct src *s = &srcstk[nsrc - 1];
        if (s->f)
                fclose(s->f);
        else if (s->mac >= 0)
                macro[s->mac].active = 0;       /* its body is fully scanned now */
        nsrc--;
}

/* one raw character from the current source (stack top, else base file) */
static int rawget(void)
{
        int c;
        if (ungot >= 0) { c = ungot; ungot = -1; return c; }
        for (;;) {
                if (nsrc == 0) {
                        if (fin == NULL)
                                return 0;
                        c = getc(fin);
                        return (c == EOF) ? 0 : c;
                }
                {
                        struct src *s = &srcstk[nsrc - 1];
                        if (s->f) {
                                c = getc(s->f);
                                if (c != EOF)
                                        return c;
                        } else if (s->s[s->pos]) {
                                return (unsigned char) s->s[s->pos++];
                        }
                        pp_pop();
                }
        }
}

static void pp_push_str(char *text, int mac)
{
        if (nsrc >= MAXSRC) { error("macro expansion too deep"); return; }
        srcstk[nsrc].f = 0;
        srcstk[nsrc].s = text;
        srcstk[nsrc].pos = 0;
        srcstk[nsrc].mac = mac;
        nsrc++;
}

/* push a single character as a source (its own per-slot buffer, no aliasing) */
static void pp_push_char(int c)
{
        if (nsrc >= MAXSRC) return;
        srcstk[nsrc].f = 0;
        srcstk[nsrc].cbuf[0] = (char) c;
        srcstk[nsrc].cbuf[1] = '\0';
        srcstk[nsrc].s = srcstk[nsrc].cbuf;
        srcstk[nsrc].pos = 0;
        srcstk[nsrc].mac = -1;
        nsrc++;
}

static void pp_addmacro(char *name, char *text)
{
        int i;
        char *t;
        for (i = 0; i < nmac; i++)
                if (strcmp(macro[i].name, name) == 0)
                        break;
        if (i == nmac) {
                if (nmac >= MAXMAC) { error("too many #defines"); return; }
                nmac++;
        } else
                free(macro[i].text);
        strncpy(macro[i].name, name, NAMSIZ);
        macro[i].name[NAMSIZ] = '\0';
        t = malloc(strlen(text) + 1);
        strcpy(t, text);
        macro[i].text = t;
        macro[i].active = 0;
}

/* called by the lexer with a freshly read identifier (NAMSIZ chars, null
 * padded). If it names an inactive macro, splice its text and return 1. */
int pp_expand(char *id)
{
        char nm[NAMSIZ + 1];
        int i, k;
        for (k = 0; k < NAMSIZ && id[k]; k++)
                nm[k] = id[k];
        nm[k] = '\0';
        for (i = 0; i < nmac; i++)
                if (!macro[i].active && strcmp(macro[i].name, nm) == 0) {
                        int after = peekc;             /* char read just past the name */
                        peekc = 0;
                        if (after > 0)
                                pp_push_char(after);   /* lower: re-streamed after the body */
                        macro[i].active = 1;
                        pp_push_str(macro[i].text, i); /* top: scanned first */
                        return 1;
                }
        return 0;
}

static int isidc(int c)
{
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
}

static void pp_define(int c)
{
        char name[NAMSIZ + 1], text[1024];
        int n;
        while (c == ' ' || c == '\t') c = rawget();
        n = 0;
        while (isidc(c)) { if (n < NAMSIZ) name[n++] = c; c = rawget(); }
        name[n] = '\0';
        while (c == ' ' || c == '\t') c = rawget();
        n = 0;
        while (c && c != '\n' && n < (int) sizeof text - 1) { text[n++] = c; c = rawget(); }
        while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t')) n--;
        text[n] = '\0';
        if (name[0])
                pp_addmacro(name, text);
        ungot = c;              /* keep the terminating newline for line counting */
}

static void pp_include(int c)
{
        char fn[256];
        int n, close;
        FILE *f;
        while (c == ' ' || c == '\t') c = rawget();
        if (c == '"' || c == '<') {
                close = (c == '<') ? '>' : '"';
                c = rawget();
                n = 0;
                while (c && c != close && c != '\n' && n < (int) sizeof fn - 1) { fn[n++] = c; c = rawget(); }
                fn[n] = '\0';
                while (c && c != '\n') c = rawget();
                f = fopen(fn, "r");
                if (f == NULL) { error("#include: cannot open %s", fn); ungot = c; return; }
                if (nsrc >= MAXSRC) { fclose(f); error("#include nested too deep"); ungot = c; return; }
                srcstk[nsrc].f = f; srcstk[nsrc].s = 0; srcstk[nsrc].pos = 0; srcstk[nsrc].mac = -1;
                nsrc++;
        } else {
                while (c && c != '\n') c = rawget();
                error("#include: expected \"filename\"");
        }
        ungot = c;
}

/* consume a # control line (the leading # has been read) */
static void pp_directive(void)
{
        char word[16];
        int c, n;
        do c = rawget(); while (c == ' ' || c == '\t');
        n = 0;
        while (c >= 'a' && c <= 'z' && n < (int) sizeof word - 1) { word[n++] = c; c = rawget(); }
        word[n] = '\0';
        if (strcmp(word, "define") == 0)
                pp_define(c);
        else if (strcmp(word, "include") == 0)
                pp_include(c);
        else                            /* unknown/blank #-line: ignore it */
                while (c && c != '\n') c = rawget();
}

/* one input character, 0 at end of file. A # at the start of a line is a
 * preprocessor control line and is consumed here, not seen by the lexer. */
int c_getchar(void)
{
        static int atbol = 1;           /* still at the start of a line? */
        int c;
        for (;;) {
                c = rawget();
                if (c == '#' && atbol) {
                        pp_directive();
                        continue;       /* atbol stays true: # consumed the line */
                }
                if (c != ' ' && c != '\t')      /* blanks keep us "at bol" for # */
                        atbol = (c == '\n');
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
