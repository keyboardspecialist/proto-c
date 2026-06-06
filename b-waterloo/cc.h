/*
 * cc.h - shared declarations for the modernized last1120-c front end.
 *
 * last1120 is the older (PDP-11/20) compiler: no structures, integer-only
 * literals, a single unified declare() (so `auto x[]` works), and a linear
 * type encoding (+020 per pointer level) rather than prestruct's 2-bit groups.
 * Same int==pointer==16-bit assumptions, so we use a pointer-wide cell type.
 */
#ifndef CC_H
#define CC_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

typedef intptr_t word;          /* a machine cell: an int value or a pointer */

#undef getchar
#undef putchar
#define getchar c_getchar
#define putchar c_putchar

/* sizes (word counts) */
#define NCPW    2
#define NWPS    4
#define NAMSIZ  8
#define PSSIZ   8               /* words per symbol slot (last1120: 8, no nel) */
#define HSHSIZ  100
#define HSHLEN  (HSHSIZ*PSSIZ)  /* 800 */
#define CMSIZ   40
#define OSSIZ   4096

/* reconstructed tables (tables.c) */
extern int  ctab[];
extern word opdope[];
extern char cvtab[];

/* shared globals (defined in c00.c, except fin/fout in runtime.c) */
extern word symbuf[], hshtab[], cmst[], swtab[], osbuf[];
extern word *space, *cp, *csym, *paraml, *parame, *swp;
extern int  namsiz, nwps, pssiz, hshsiz, hshlen, hshused, cmsiz, ossiz, osleft;
extern int  ctyp, isn, swsiz, contlab, brklab, deflab, nreg, nauto, stack;
extern int  peeksym, peekc, eof, line, cval, ncpw, nerror;
extern int  regtab, efftab, cctab, sptab;
extern FILE *fin, *fout;

/* runtime shims (runtime.c) */
int   c_getchar(void);
int   c_putchar(int c);
void  flush(void);
void  error(char *s, ...);
void  pname(word *name);
char *namestr();
void  src_push_str();		/* manifest substitution */
void  src_push_char();
void  src_push_file();		/* %filename inclusion */
char *gettext();		/* capture manifest text (b00.c) -- returns char* */

/* node builder + pointer-returning routines */
word block(int n, word op, word t, word d, ...);
void pblock(word x);
word lookup();
word tree();
word convert();
word pexpr();
int  branch();
int  jump();
int  label();

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
void sw_caserange();
void sw_casecmp();
void sw_default();
void sw_end();
void cg_cap_begin();
char *cg_cap_end();		/* returns char* -- must be declared on 64-bit */
void lab_begin();
void lab_define();
void lab_goto();
void lab_end();
extern int g_nparam, g_paramoff[], g_paramflt[], g_framesize, g_autobottom;
extern int g_narr, g_arrcell[], g_arrstore[];
extern int g_rettype_flt;
extern int treedump;
int length();

/* debug instrumentation (-g): per-statement line markers + breakpoint hook,
 * and a per-function frame-variable map captured in blkhed for the IDE. */
extern int dbg;
void cg_break(int ln);
extern int  g_ndbgvar, g_dbgvar_off[], g_dbgvar_type[], g_dbgvar_size[];
extern char g_dbgvar_name[][NAMSIZ + 1];
int rlength();

#endif /* CC_H */
