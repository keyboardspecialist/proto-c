/*
 * cc.h - shared declarations for the modernized prestruct-c front end.
 *
 * The 1972 compiler assumed int == pointer == 16 bits. On a 64-bit host a
 * pointer no longer fits in an int, and the compiler routinely stores node
 * pointers into the same word slots it stores small integers. So we introduce
 * a single "word" cell type wide enough to hold either, and use it for every
 * cell that may carry a pointer (the node arena, the symbol table, the
 * comparison stack, the packed name buffer).
 */
#ifndef CC_H
#define CC_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

typedef intptr_t word;          /* a machine cell: an int value or a pointer */

/* The original buffered I/O routines read/wrote raw file descriptors. We
 * reimplement getchar/putchar against stdio (see runtime.c) and route the
 * source's getchar()/putchar() calls to them. <stdio.h> defines these as
 * macros, so undo that first. */
#undef getchar
#undef putchar
#define getchar c_getchar
#define putchar c_putchar

/* sizes (word counts, matching the original octal/decimal constants) */
#define NCPW    2               /* characters packed per name word */
#define NWPS    4               /* name words per symbol (namsiz/ncpw) */
#define NAMSIZ  8               /* characters in a name */
#define PSSIZ   9               /* words per symbol-table slot */
#define HSHSIZ  100             /* symbol-table slots */
#define HSHLEN  (HSHSIZ*PSSIZ)  /* symbol table length in words */
#define CMSIZ   40              /* comparison stack depth */
#define OSSIZ   4096            /* node arena size in words (was 250) */

/* reconstructed lexer/parser tables (tables.c) */
extern int  ctab[];             /* character class table */
extern word opdope[];           /* operator dope vector */
extern char cvtab[];            /* type-conversion matrix */

/* shared globals (defined in c00.c, except fin/fout in runtime.c) */
extern word symbuf[], hshtab[], cmst[], swtab[], osbuf[];
extern word *space, *cp, *defsym, *csym, *paraml, *parame, *swp;
extern int  namsiz, nwps, pssiz, hshsiz, hshlen, hshused, cmsiz, ossiz, osleft;
extern int  ctyp, isn, swsiz, contlab, brklab, deflab, nreg, nauto, stack;
extern int  peeksym, peekc, eof, line, xdflg, cval, ncpw, nerror;
extern int  strflg, mosflg, debug, regtab, efftab, cctab, sptab;
extern double fcval;
extern FILE *fin, *fout;

/* runtime shims (runtime.c) */
int   c_getchar(void);
int   c_putchar(int c);
void  flush(void);
int   getnum(int base);
void  error(char *s, ...);
void  pname(word *name);        /* print a packed name */
char *namestr();                /* unpack a packed name into a buffer */

/* node arena / builder and the pointer-returning routines. These MUST be
 * declared as returning word so callers don't implicitly truncate a returned
 * node pointer to int. */
word block(int n, word op, word t, word d, ...);
void pblock(word x);
word lookup();
word tree();
word disarray();
word optim();
word convert();
word pexpr();

/* used as function-pointer values, so must be declared before use */
int branch();
int jump();
int label();

/* WAT code generator (wat.c) */
void cg(char *fmt, ...);
void watmodopen(void);
void watmodclose(void);
void func_begin(void);
void func_emit(char *nm);
void watret(void);
int  galloc(char *nm, int size);
int  gdata(int size);
int  gref(char *nm);
void watdata(int addr, char *bytes, int n);
void gexpr();
void gaddr();
void gcond();
void gretval();
void sw_begin();
void sw_case();
void sw_default();
void sw_end();
void lab_begin();
void lab_define();
void lab_goto();
void lab_end();
extern int g_nparam, g_paramoff[], g_paramflt[], g_framesize, g_autobottom;
extern int g_rettype_flt;
extern int treedump;

/* debug instrumentation (-g): per-statement line markers + breakpoint hook,
 * and a per-function frame-variable map captured in blkhed for the IDE. */
extern int dbg;
void cg_break(int ln);
extern int  g_ndbgvar, g_dbgvar_off[], g_dbgvar_type[], g_dbgvar_size[];
extern char g_dbgvar_name[][NAMSIZ + 1];
int trlength();

#endif /* CC_H */
