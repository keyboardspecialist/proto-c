/*
 * wat.c - WebAssembly-text code generator for the prestruct-c front end.
 *
 * Replaces the discarded PDP-11 backend. The 1972 front end builds expression
 * parse trees (in osbuf) and drives control flow syntax-directed from
 * statement() (c02.c). Here:
 *   - gexpr() walks an expression tree and emits wasm that leaves the value on
 *     the operand stack; gaddr() leaves an lvalue address.
 *   - All variables live in linear memory: globals at fixed addresses, autos
 *     and params in a per-function shadow-stack frame addressed off $fp.
 *   - Function bodies are emitted into a buffer first, so the (func ...) header
 *     (which needs the frame size that blkhed computes while parsing the body)
 *     can be written before the buffered body.
 *
 * wasm32 sizes: int/pointer = 4 bytes, char = 1. No 16-bit fidelity.
 */
#include "cc.h"

/* ---- output buffers ----
 * The whole module body (functions + data) is buffered in mbuf so that
 * (import) declarations for called-but-undefined functions can be emitted
 * ahead of it at close time. The current function body buffers separately in
 * fbuf, then func_emit appends it to mbuf. */

static char *mbuf, *fbuf;
static int   mcap, mlen, fcap, flen;
static int   infunc;

static void bput(char **buf, int *cap, int *len, char *s, int n)
{
	if (*len + n + 1 > *cap) {
		*cap = (*cap ? *cap * 2 : 8192);
		while (*len + n + 1 > *cap)
			*cap *= 2;
		*buf = realloc(*buf, *cap);
	}
	memcpy(*buf + *len, s, n);
	*len += n;
	(*buf)[*len] = '\0';
}

/* module-level emit (functions, data, imports) */
void mprintf(char *fmt, ...)
{
	char tmp[1024];
	int n;
	va_list ap;
	va_start(ap, fmt);
	n = vsnprintf(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if (n >= (int) sizeof tmp)
		n = sizeof tmp - 1;
	bput(&mbuf, &mcap, &mlen, tmp, n);
}

/* switch-body segment buffers (see the switch section) */
#define MAXSEG 128
static char *seg[MAXSEG];
static int   segc[MAXSEG], segl[MAXSEG];
static int   nseg, curseg, inswitch;

/* function-body label segment buffers (see the goto/label section) */
#define MAXLSEG 256
static char *lseg[MAXLSEG];
static int   lsegc[MAXLSEG], lsegl[MAXLSEG];
static int   nlseg, curlseg, inlabels;

/* capture sink: cg_cap_begin redirects cg() into a private buffer so a parsed
 * code fragment can be replayed later (the for-loop step runs after the body). */
static char *capbuf; static int capcap, caplen, incap;
void cg_cap_begin(void) { incap = 1; caplen = 0; if (capbuf) capbuf[0] = '\0'; }
char *cg_cap_end(void) { incap = 0; return capbuf ? capbuf : ""; }

/* current-sink emit: capture -> switch segment -> label segment -> body -> module */
void cg(char *fmt, ...)
{
	char tmp[1024];
	int n;
	va_list ap;
	va_start(ap, fmt);
	n = vsnprintf(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if (n >= (int) sizeof tmp)
		n = sizeof tmp - 1;
	if (incap)
		bput(&capbuf, &capcap, &caplen, tmp, n);
	else if (inswitch)
		bput(&seg[curseg], &segc[curseg], &segl[curseg], tmp, n);
	else if (inlabels)
		bput(&lseg[curlseg], &lsegc[curlseg], &lsegl[curlseg], tmp, n);
	else if (infunc)
		bput(&fbuf, &fcap, &flen, tmp, n);
	else
		bput(&mbuf, &mcap, &mlen, tmp, n);
}

/* ---- function name tracking, for import generation ---- */

#define MAXFN 512
static char defn[MAXFN][NAMSIZ + 1];
static int  ndef;
static struct { char name[NAMSIZ + 1]; int argc; } calln[MAXFN];
static int  ncall;

static void note_def(char *nm)
{
	if (ndef < MAXFN)
		strcpy(defn[ndef++], nm);
}

static void note_call(char *nm, int argc)
{
	int i;
	for (i = 0; i < ncall; i++)
		if (strcmp(calln[i].name, nm) == 0)
			return;
	if (ncall < MAXFN) {
		strcpy(calln[ncall].name, nm);
		calln[ncall].argc = argc;
		ncall++;
	}
}

/* ---- global symbol -> linear-memory address map ---- */

#define MAXGLOB 256
static struct { char name[NAMSIZ + 1]; int addr; } gtab[MAXGLOB];
static int nglob;
static int gwater = 16;			/* next free global address */

static int g_align4(int n) { return (n + 3) & ~3; }

/* reserve `size` bytes for a global named `nm`, return its address */
int galloc(char *nm, int size)
{
	int a = gwater;
	if (nglob >= MAXGLOB) {
		error("too many globals");
		exit(1);
	}
	strncpy(gtab[nglob].name, nm, NAMSIZ);
	gtab[nglob].name[NAMSIZ] = '\0';
	gtab[nglob].addr = a;
	nglob++;
	gwater += g_align4(size ? size : 4);
	return a;
}

static int glookup(char *nm)
{
	int i;
	for (i = 0; i < nglob; i++)
		if (strcmp(gtab[i].name, nm) == 0)
			return gtab[i].addr;
	return galloc(nm, 4);		/* forward/extern reference: reserve */
}

/* public: address of a named global (reserving it if new) */
int gref(char *nm)
{
	return glookup(nm);
}

/* reserve `size` anonymous bytes (e.g. for a string literal), return address */
int gdata(int size)
{
	int a = gwater;
	gwater += g_align4(size);
	return a;
}

/* emit a (data ...) segment of `n` raw bytes at `addr` (module level) */
void watdata(int addr, char *bytes, int n)
{
	int i;
	if (treedump)
		return;
	mprintf("  (data (i32.const %d) \"", addr);
	for (i = 0; i < n; i++)
		mprintf("\\%02x", (unsigned char) bytes[i]);
	mprintf("\")\n");
}

/* ---- per-function frame info (filled by blkhed in c02.c) ---- */

int g_nparam;
int g_paramoff[64];
int g_paramflt[64];		/* 1 if param k is float (f64) */
int g_framesize;
int g_autobottom;		/* most negative auto offset (<= 0) */
int g_narr;			/* sized array locals needing pointer init */
int g_arrcell[64], g_arrstore[64];

/* ---- module framing ---- */

void watmodopen(void)
{
	if (treedump)
		return;
	mlen = 0;			/* reset module buffer */
	ndef = ncall = 0;
}

void watmodclose(void)
{
	int i, j, k, imported;
	if (treedump)
		return;
	printf("(module\n");
	/* imports must precede all non-import definitions (incl. memory/global) */
	for (i = 0; i < ncall; i++) {
		imported = 0;
		for (j = 0; j < ndef; j++)
			if (strcmp(calln[i].name, defn[j]) == 0)
				imported = 1;
		if (imported)
			continue;
		printf("  (import \"env\" \"%s\" (func $%s", calln[i].name, calln[i].name);
		for (k = 0; k < calln[i].argc; k++)
			printf(" (param i32)");
		printf(" (result i32)))\n");
	}
	printf("  (memory (export \"memory\") 2)\n");
	printf("  (global $sp (mut i32) (i32.const 131072))\n");
	if (mbuf)
		fputs(mbuf, stdout);
	printf(")\n");
}

void func_begin(void)
{
	flen = 0;
	infunc = treedump ? 0 : 1;
	if (fbuf)
		fbuf[0] = '\0';
}

/* emit a completed function: header + prologue + buffered body + epilogue */
void func_emit(char *nm)
{
	int k;
	infunc = 0;
	if (treedump)
		return;
	note_def(nm);
	mprintf("  (func $%s (export \"%s\")", nm, nm);
	for (k = 0; k < g_nparam; k++)
		mprintf(" (param %s)", g_paramflt[k] ? "f64" : "i32");
	mprintf(" (result %s)\n", g_rettype_flt ? "f64" : "i32");
	mprintf("    (local $fp i32) (local $t i32) (local $a i32) (local $f f64) (local $sw i32) (local $pc i32)\n");
	/* prologue: allocate frame, set $fp, copy params into the frame */
	mprintf("    global.get $sp i32.const %d i32.sub global.set $sp\n", g_framesize);
	mprintf("    global.get $sp i32.const %d i32.add local.set $fp\n", -g_autobottom);
	for (k = 0; k < g_nparam; k++)
		mprintf("    local.get $fp i32.const %d i32.add local.get %d %s\n",
			g_paramoff[k], k, g_paramflt[k] ? "f64.store" : "i32.store");
	/* sized array locals: init the pointer cell to point at its storage.
	 * A B pointer value is a word index, so store (storage byte addr) >> 2. */
	for (k = 0; k < g_narr; k++)
		mprintf("    local.get $fp i32.const %d i32.add local.get $fp i32.const %d i32.add i32.const 2 i32.shr_u i32.store\n",
			g_arrcell[k], g_arrstore[k]);
	if (fbuf)
		bput(&mbuf, &mcap, &mlen, fbuf, flen);
	/* epilogue + default return value */
	mprintf("    global.get $sp i32.const %d i32.add global.set $sp\n", g_framesize);
	mprintf(g_rettype_flt ? "    f64.const 0)\n" : "    i32.const 0)\n");
}

/* emit the function epilogue immediately before a `return` (value already on
 * the operand stack), used by statement()'s return case */
void watret(void)
{
	cg("global.get $sp i32.const %d i32.add global.set $sp return ", g_framesize);
}

/* ---- expression code generation ---- */

/* last1120 type encoding: base 0-4, +020 per pointer level. char == 1 exactly. */
static int ischar(word *p) { return p[1] == 1; }

/* a scalar float/double type (not a pointer to one) */
static int isftype(int ty) { return ty < 020 && (ty == 2 || ty == 3); }

void gexpr();

/* result of expression p is a float (f64) value */
int g_rettype_flt;
static int tflt(word *p)
{
	if (p == 0)
		return 0;
	switch ((int) p[0]) {
	case 21:				/* int const */
	case 35: case 29:			/* address */
	case 34:				/* ! */
	case 60: case 61: case 62:		/* comparisons -> int */
	case 63: case 64: case 65:
		return 0;
	case 23:				/* float const */
		return 1;
	}
	return isftype((int) p[1]);
}

/* leave the ADDRESS of an lvalue on the operand stack */
void gaddr(p)
word *p;
{
	int op = (int) p[0];
	char buf[NAMSIZ + 1];

	switch (op) {
	case 20:		/* name */
		switch ((int) p[3]) {	/* storage class */
		case 5:			/* auto / param: $fp + offset */
			cg("local.get $fp i32.const %d i32.add ", (int) p[5] + (int) p[4]);
			return;
		case 6:			/* extern: address looked up by name */
			cg("i32.const %d ", glookup(namestr(&p[5], buf)) + (int) p[4]);
			return;
		case 7:			/* static: address stored directly in p[5] */
			cg("i32.const %d ", (int) p[5] + (int) p[4]);
			return;
		}
		error("bad storage class %d", (int) p[3]);
		return;

	case 36:		/* *x : address is the pointer value (word index -> byte) */
		gexpr((word *) p[3]);
		cg("i32.const 2 i32.shl ");
		return;

	case 35:		/* &x where x is itself an lvalue */
	case 29:
		gaddr((word *) p[3]);
		return;
	}
	error("not an lvalue (op %d)", op);
}

/* opcode -> wasm op string; f selects the f64 variant */
static char *binop(int op, int f)
{
	if (f)
		switch (op) {
		case 40: return "f64.add";
		case 41: return "f64.sub";
		case 42: return "f64.mul";
		case 43: return "f64.div";
		case 60: return "f64.eq";
		case 61: return "f64.ne";
		case 62: return "f64.le";
		case 63: return "f64.lt";
		case 64: return "f64.ge";
		case 65: return "f64.gt";
		}
	else
		switch (op) {
		case 40: return "i32.add";
		case 41: return "i32.sub";
		case 42: return "i32.mul";
		case 43: return "i32.div_s";
		case 44: return "i32.rem_s";
		case 45: return "i32.shr_s";
		case 46: return "i32.shl";
		case 47: return "i32.and";
		case 48: return "i32.or";
		case 49: return "i32.xor";
		case 60: return "i32.eq";
		case 61: return "i32.ne";
		case 62: return "i32.le_s";
		case 63: return "i32.lt_s";
		case 64: return "i32.ge_s";
		case 65: return "i32.gt_s";
		}
	return 0;
}

/* Waterloo float operator -> f32 wasm op (operands already reinterpreted to
 * f32). Arithmetic (52-55) yields f32; comparisons (56-59,82,83) yield i32. */
static char *fbinop(int op)
{
	switch (op) {
	case 52: return "f32.mul";
	case 53: return "f32.div";
	case 54: return "f32.add";
	case 55: return "f32.sub";
	case 56: return "f32.eq";
	case 57: return "f32.ne";
	case 58: return "f32.lt";
	case 59: return "f32.le";
	case 82: return "f32.gt";
	case 83: return "f32.ge";
	}
	return 0;
}

/* emit p's value, converting to int or f64 as requested */
static void gval(word *p, int wantf)
{
	int isf = tflt(p);
	gexpr(p);
	if (isf && !wantf)
		cg("i32.trunc_f64_s ");
	else if (!isf && wantf)
		cg("f64.convert_i32_s ");
}

/* walk a call argument tree (comma nodes), pushing args left-to-right */
static void gargs(word *p)
{
	if (p == 0 || p[0] == 0)
		return;
	if (p[0] == 9) {		/* comma */
		gargs((word *) p[3]);
		gargs((word *) p[4]);
		return;
	}
	gexpr(p);
}

static int countargs(word *p)
{
	if (p == 0 || p[0] == 0)
		return 0;
	if (p[0] == 9)
		return countargs((word *) p[3]) + countargs((word *) p[4]);
	return 1;
}

static void loadof(word *p)
{
	if (tflt(p))
		cg("f64.load ");
	else if (ischar(p))
		cg("i32.load8_s ");
	else
		cg("i32.load ");
}

static void storeof(word *p)
{
	if (tflt(p))
		cg("f64.store ");
	else if (ischar(p))
		cg("i32.store8 ");
	else
		cg("i32.store ");
}

/* leave the VALUE of an expression on the operand stack */
void gexpr(p)
word *p;
{
	int op;
	char buf[NAMSIZ + 1];

	if (p == 0)
		return;
	op = (int) p[0];

	if (binop(op, 0) != 0) {	/* binary arithmetic / comparison */
		int fop;
		if (op >= 60 && op <= 65)	/* compare: float if either operand is */
			fop = tflt((word *) p[3]) || tflt((word *) p[4]);
		else
			fop = tflt(p);		/* arithmetic: by result type */
		gval((word *) p[3], fop);
		gval((word *) p[4], fop);
		cg("%s ", binop(op, fop));
		return;
	}

	if (fbinop(op) != 0) {		/* Waterloo #-float op: reinterpret words as f32 */
		gexpr((word *) p[3]); cg("f32.reinterpret_i32 ");
		gexpr((word *) p[4]); cg("f32.reinterpret_i32 ");
		cg("%s ", fbinop(op));
		if (op < 56)		/* arith (#* #/ #+ #-) -> word holding f32 bits */
			cg("i32.reinterpret_f32 ");
		return;		/* compares (#== ...) already leave i32 */
	}

	switch (op) {
	case 50:		/* && : logical, 1/0 */
		gcond((word *) p[3]);
		cg("(if (result i32) (then ");
		gcond((word *) p[4]);
		cg("i32.eqz i32.eqz ) (else i32.const 0)) ");
		return;

	case 51:		/* || : logical, 1/0 */
		gcond((word *) p[3]);
		cg("(if (result i32) (then i32.const 1) (else ");
		gcond((word *) p[4]);
		cg("i32.eqz i32.eqz )) ");
		return;
	case 21:		/* integer constant */
		cg("i32.const %d ", (int) p[3]);
		return;

	case 22:		/* string literal: word index of its data (B pointer) */
		cg("i32.const %d ", (int) p[3] >> 2);
		return;

	case 23:		/* float constant (f64 bits stashed in p[3]) */
	{
		word w = p[3];
		double d;
		memcpy(&d, &w, sizeof(double));
		cg("f64.const %.17g ", d);
		return;
	}

	case 20:		/* name: load from its address */
		gaddr(p);
		loadof(p);
		return;

	case 36:		/* *p : load word at the pointer (word index -> byte) */
		gexpr((word *) p[3]);
		cg("i32.const 2 i32.shl ");
		loadof(p);
		return;

	case 35:		/* &x : a B pointer is a word index (byte address >> 2) */
	case 29:
		gaddr((word *) p[3]);
		cg("i32.const 2 i32.shr_u ");
		return;

	case 37:		/* unary - */
		if (tflt((word *) p[3])) {
			gexpr((word *) p[3]);
			cg("f64.neg ");
		} else {
			cg("i32.const 0 ");
			gexpr((word *) p[3]);
			cg("i32.sub ");
		}
		return;

	case 38:		/* ~ */
		gexpr((word *) p[3]);
		cg("i32.const -1 i32.xor ");
		return;

	case 34:		/* ! */
		gval((word *) p[3], 0);
		cg("i32.eqz ");
		return;

	case 80:		/* assignment: lhs = rhs, value is rhs */
	{
		/* Keep the lhs address on the wasm operand stack (not in a scratch
		 * local) so a side-effecting rhs -- e.g. c = *s++ -- can't clobber it. */
		int lf = tflt(p);
		char *s = lf ? "$f" : "$t";
		gaddr((word *) p[3]);		/* [addr] */
		gval((word *) p[4], lf);	/* [addr, val]  (coerce rhs to lhs type) */
		cg("local.tee %s ", s);		/* [addr, val], scratch = val */
		storeof((word *) p[3]);		/* consumes [addr, val] */
		cg("local.get %s ", s);		/* result = val */
		return;
	}

	case 70: case 71: case 72: case 73: case 74:	/* compound =op */
	case 75: case 76: case 77: case 78: case 79:
	{
		int lf = tflt(p);
		char *s = lf ? "$f" : "$t";
		gaddr((word *) p[3]);
		cg("local.set $a local.get $a ");
		loadof((word *) p[3]);
		gval((word *) p[4], lf);
		cg("%s local.set %s local.get $a local.get %s ", binop(op - 30, lf), s, s);
		storeof((word *) p[3]);
		cg("local.get %s ", s);
		return;
	}

	case 30: case 31:	/* pre ++ / -- */
	case 32: case 33:	/* post ++ / -- */
	{
		int dec = (op & 1);		/* 31,33 are -- */
		int post = (op >= 32);
		/* pointer ++/-- steps by element size; scalars step by 1 */
		int ty = (int) p[1];
		int delta = (ty >= 020) ? length(ty - 020) : 1;
		int lf = tflt(p);
		char *as = lf ? (dec ? "f64.sub" : "f64.add")
			      : (dec ? "i32.sub" : "i32.add");
		char *s = lf ? "$f" : "$t";
		char step[32];
		if (delta == 0) delta = 1;
		if (lf)
			sprintf(step, "f64.const %d ", delta);
		else
			sprintf(step, "i32.const %d ", delta);
		gaddr((word *) p[3]);
		cg("local.set $a local.get $a ");
		loadof((word *) p[3]);			/* old value */
		cg("local.set %s ", s);
		if (post) {				/* result = old */
			cg("local.get $a local.get %s %s%s ", s, step, as);
			storeof((word *) p[3]);
			cg("local.get %s ", s);
		} else {				/* result = new */
			cg("local.get %s %s%s local.set %s ", s, step, as, s);
			cg("local.get $a local.get %s ", s);
			storeof((word *) p[3]);
			cg("local.get %s ", s);
		}
		return;
	}

	case 100:		/* call */
	{
		word *fn = (word *) p[3];
		namestr(&fn[5], buf);
		note_call(buf, countargs((word *) p[4]));
		gargs((word *) p[4]);
		cg("call $%s ", buf);
		return;
	}

	case 9:			/* comma */
		gexpr((word *) p[3]);
		cg("drop ");
		gexpr((word *) p[4]);
		return;

	case 90:		/* ?: conditional. p[3]=cond, p[4]=':' node */
	{
		word *col = (word *) p[4];
		int rf = tflt((word *) col[3]) || tflt((word *) col[4]);
		gval((word *) p[3], 0);
		cg("(if (result %s) (then ", rf ? "f64" : "i32");
		gval((word *) col[3], rf);
		cg(") (else ");
		gval((word *) col[4], rf);
		cg(")) ");
		return;
	}

	case 110:		/* force result */
		gexpr((word *) p[3]);
		return;
	}
	error("WAT: unsupported op %d", op);
}

/* a condition: leave an i32 (nonzero = true) on the stack.
 * Waterloo B has real short-circuit && / || (distinct from bitwise & / |, which
 * here just fall through to gval and are truthy iff nonzero). */
void gcond(p)
word *p;
{
	int op;

	if (p == 0) {
		cg("i32.const 1 ");
		return;
	}
	op = (int) p[0];
	switch (op) {
	case 50:		/* && : logical and (short-circuit) */
		gcond((word *) p[3]);
		cg("(if (result i32) (then ");
		gcond((word *) p[4]);
		cg(") (else i32.const 0)) ");
		return;

	case 51:		/* || : logical or (short-circuit) */
		gcond((word *) p[3]);
		cg("(if (result i32) (then i32.const 1) (else ");
		gcond((word *) p[4]);
		cg(")) ");
		return;

	case 34:		/* ! */
		gcond((word *) p[3]);
		cg("i32.eqz ");
		return;
	}
	if (tflt(p)) {		/* float truthiness: f != 0.0 */
		gexpr(p);
		cg("f64.const 0 f64.ne ");
	} else
		gval(p, 0);	/* nonzero = true */
}

/* emit a return value (converted to the function's return type), or the
 * default 0 when p == 0 */
void gretval(p)
word *p;
{
	if (p == 0)
		cg(g_rettype_flt ? "f64.const 0 " : "i32.const 0 ");
	else
		gval(p, g_rettype_flt);
}

/* ---- switch / case / default ----
 * The body is collected into per-case segments (split at each case/default
 * label). Then a nested-block br_if dispatch is emitted: each case value
 * compares against $sw and branches to its segment; cases fall through
 * naturally; break -> the $L<brk> block. */

static int swval[MAXSEG], swseg[MAXSEG], nswcase, swdefault, swbrk;
static int swrlo[MAXSEG], swrhi[MAXSEG], swrseg[MAXSEG], nswrange;

void sw_begin(expr, brk)
word *expr;
{
	if (inswitch) {
		error("WAT v1: nested switch unsupported");
		return;
	}
	/* Evaluate the switch value NOW, while its tree is still valid: the body
	 * parse reuses the node arena and would clobber it. Emitted to the
	 * function body before we start collecting segments. */
	gval(expr, 0);
	cg("local.set $sw ");
	inswitch = 1;
	nseg = 1;			/* segment 0 = code before the first case */
	curseg = 0;
	segl[0] = 0;
	if (seg[0])
		seg[0][0] = '\0';
	nswcase = 0;
	nswrange = 0;
	swdefault = -1;
	swbrk = brk;
}

static void sw_newseg(void)
{
	curseg = nseg++;
	if (curseg >= MAXSEG) {
		error("switch too large");
		exit(1);
	}
	segl[curseg] = 0;
	if (seg[curseg])
		seg[curseg][0] = '\0';
}

void sw_case(v)
{
	swval[nswcase] = v;
	swseg[nswcase] = nseg;		/* the segment about to start */
	nswcase++;
	sw_newseg();
}

/* Waterloo range case: case lo :: hi : */
void sw_caserange(lo, hi)
{
	swrlo[nswrange] = lo;
	swrhi[nswrange] = hi;
	swrseg[nswrange] = nseg;
	nswrange++;
	sw_newseg();
}

void sw_default(void)
{
	swdefault = nseg;
	sw_newseg();
}

void sw_end(void)
{
	int i;
	inswitch = 0;			/* emit to the function body now */
	cg("(block $L%d ", swbrk);			/* break target */
	for (i = nseg - 1; i >= 0; i--)			/* seg blocks, seg0 innermost */
		cg("(block $Lc%d_%d ", swbrk, i);
	for (i = 0; i < nswcase; i++)			/* dispatch: equality */
		cg("local.get $sw i32.const %d i32.eq br_if $Lc%d_%d ",
			swval[i], swbrk, swseg[i]);
	for (i = 0; i < nswrange; i++)			/* dispatch: lo <= sw <= hi */
		cg("local.get $sw i32.const %d i32.ge_s local.get $sw i32.const %d i32.le_s i32.and br_if $Lc%d_%d ",
			swrlo[i], swrhi[i], swbrk, swrseg[i]);
	if (swdefault >= 0)
		cg("br $Lc%d_%d ", swbrk, swdefault);
	else
		cg("br $L%d ", swbrk);
	for (i = 0; i < nseg; i++) {			/* close block, emit segment */
		cg(") ");
		if (seg[i])
			cg("%s", seg[i]);
	}
	cg(") ");					/* close break block */
}

/* ---- goto / labels ----
 * The whole function body is collected into segments split at each top-level
 * label. If any labels exist, the segments are wrapped in a $pc-dispatch loop:
 * goto sets $pc to the target label's id and branches to the dispatch; the
 * dispatch maps id -> segment. Segments fall through in order. Labels must be
 * at the function's top level (a label nested inside if/while would split a
 * structured block and produce unbalanced output). */

static int labid[MAXLSEG], labseg[MAXLSEG], nlab;
static int fndisp;

void lab_begin(dispid)
{
	inlabels = 1;
	nlseg = 1;
	curlseg = 0;
	lsegl[0] = 0;
	if (lseg[0])
		lseg[0][0] = '\0';
	nlab = 0;
	fndisp = dispid;
}

static void lab_newseg(void)
{
	curlseg = nlseg++;
	if (curlseg >= MAXLSEG) {
		error("too many labels");
		exit(1);
	}
	lsegl[curlseg] = 0;
	if (lseg[curlseg])
		lseg[curlseg][0] = '\0';
}

void lab_define(id)
{
	labid[nlab] = id;
	labseg[nlab] = nlseg;
	nlab++;
	lab_newseg();
}

void lab_goto(id)
{
	cg("i32.const %d local.set $pc br $disp%d ", id, fndisp);
}

int lab_used(void)
{
	return nlab > 0;
}

/* flush the collected body to fbuf: flat if no labels, else a dispatch loop */
void lab_end(void)
{
	int i;
	inlabels = 0;
	if (nlab == 0) {
		if (lseg[0])
			bput(&fbuf, &fcap, &flen, lseg[0], lsegl[0]);
		return;
	}
	cg("i32.const 0 local.set $pc ");
	cg("(block $brk%d (loop $disp%d ", fndisp, fndisp);
	for (i = nlseg - 1; i >= 0; i--)		/* seg blocks, seg0 innermost */
		cg("(block $Ls%d_%d ", fndisp, i);
	for (i = 0; i < nlab; i++)			/* dispatch on $pc */
		cg("local.get $pc i32.const %d i32.eq br_if $Ls%d_%d ",
			labid[i], fndisp, labseg[i]);
	cg("br $Ls%d_0 ", fndisp);			/* entry: pc 0 -> segment 0 */
	for (i = 0; i < nlseg; i++) {
		cg(") ");
		if (lseg[i])
			cg("%s", lseg[i]);
	}
	cg("br $brk%d ", fndisp);			/* fall-through exit */
	cg(")) ");					/* close loop, break block */
}
