/* B compiler (b-waterloo), pass 1 part 3: external definitions, statements.
 * Globals are untyped words: `name;`, `name const;`, `name[n] inits;`, or
 * `name(params) body`. `switch rvalue statement` takes NO parentheses. Over b72
 * this adds the Waterloo control flow: `for`, `repeat`, and switch range cases
 * (`case lo :: hi :`); break/next/do-while/default reuse the last1120 cases that
 * were already present. The `for` step expression appears before the body but
 * runs after it, so its code is captured (cg_cap_*) and replayed at the continue
 * point. Control flow is emitted as structured wasm (see cc.h, wat.c). */

#include "cc.h"

/* emit `width` little-endian bytes of v into a data buffer */
static int putw4(char *d, int dlen, long v)
{
	int k;
	for (k = 0; k < 4; k++)
		d[dlen++] = (char) (v >> (8 * k));
	return dlen;
}

function(buf)
char *buf;
{
	func_begin();
	lab_begin(isn++);
	declare(8);		/* parameter names, up to ) */
	declist();		/* parameter type declarations */
	statement(1);		/* body; blkhed (from {) does locals + frame */
	lab_end();
	func_emit(buf);
}

extdef()
{
	int o, nel, dlen;
	char buf[NAMSIZ + 1];
	char data[4096];

	if (((o = symbol()) == 0) || o == 1)	/* EOF / ; */
		return;
	if (o != 20)
		goto syntax;
	csym[0] = 6;			/* extern */
	namestr(&csym[4], buf);
	switch (o = symbol()) {

	case 80:			/* = : manifest constant (textual macro) */
		csym[0] = 4;		/* manifest class */
		csym[3] = (word) gettext();	/* raw text, re-lexed on each use */
		return;

	case 6:				/* ( : function */
		function(buf);
		return;

	case 21:			/* int global = const */
		dlen = putw4(data, 0, cval);
		watdata(galloc(buf, 4), data, dlen);
		if ((o = symbol()) != 1)	/* ; */
			goto syntax;
		return;

	case 1:				/* ; : uninitialized int */
		galloc(buf, 4);
		return;

	case 4:				/* [ : array */
		nel = 0;
		if ((o = symbol()) == 21) {	/* size const */
			nel = cval;
			o = symbol();
		}
		if (o != 5)		/* ] */
			goto syntax;
		{
			/* last1120 model: the named symbol is a pointer cell holding the
			 * address of the (separately allocated) storage. */
			int storage = gdata((nel ? nel : 1) * 4);
			int ptr = galloc(buf, 4);
			char pd[4];
			putw4(pd, 0, storage >> 2);	/* B pointer = word index */
			watdata(ptr, pd, 4);
			if ((o = symbol()) == 1)	/* ; uninitialized */
				return;
			dlen = 0;
			while (o == 21) {	/* const initializers */
				dlen = putw4(data, dlen, cval);
				if ((o = symbol()) == 1)	/* ; */
					break;
				if (o != 9)	/* , */
					goto syntax;
				o = symbol();
			}
			if (o != 1)
				goto syntax;
			watdata(storage, data, dlen);
		}
		return;

	case 0:				/* EOF */
		return;
	}
syntax:
	error("External definition syntax");
	errflush(o);
	statement(0);
}

statement(d)
{
	int o, o1, o2;
	word *np;

stmt:
	switch (o = symbol()) {

	case 0:
		error("Unexpected EOF");
	case 1:
	case 3:
		return;

	case 2:				/* { */
		if (d)
			blkhed();
		while (!eof) {
			if ((o = symbol()) == 3)	/* } */
				goto bend;
			peeksym = o;
			statement(0);
		}
		error("Missing '}'");
	bend:
		return;

	case 19:			/* keyword */
		switch (cval) {

		case 10:		/* goto */
			if ((o = symbol()) != 20)
				goto syntax;
			if (csym[2] == 0)
				csym[2] = isn++;
			lab_goto((int) csym[2]);
			goto semi;

		case 11:		/* return */
			if ((peeksym = symbol()) == 6)	/* ( */
				gretval((word *) pexpr());
			else
				gretval((word *) 0);
			watret();
			goto semi;

		case 12:		/* if */
			gcond((word *) pexpr());
			cg("(if (then ");
			statement(0);
			if ((o = symbol()) == 19 & cval == 14) {	/* else */
				cg(") (else ");
				statement(0);
				cg("))");
				return;
			}
			peeksym = o;
			cg("))");
			return;

		case 13:		/* while */
			o1 = contlab;
			o2 = brklab;
			contlab = isn++;
			brklab = isn++;
			cg("(block $L%d (loop $L%d ", brklab, contlab);
			gcond((word *) pexpr());
			cg("i32.eqz br_if $L%d ", brklab);
			statement(0);
			cg("br $L%d))", contlab);
			contlab = o1;
			brklab = o2;
			return;

		case 17:		/* break */
			if (brklab == 0)
				error("Nothing to break from");
			cg("br $L%d ", brklab);
			goto semi;

		case 18:		/* continue */
			if (contlab == 0)
				error("Nothing to continue");
			cg("br $L%d ", contlab);
			goto semi;

		case 19:		/* do */
		{
			int looplab;
			o1 = contlab;
			o2 = brklab;
			looplab = isn++;
			brklab = isn++;
			contlab = isn++;
			cg("(block $L%d (loop $L%d (block $L%d ",
				brklab, looplab, contlab);
			statement(0);
			cg(")");
			if ((o = symbol()) == 19 & cval == 13) {	/* while */
				gcond((word *) tree());
				cg("br_if $L%d ))", looplab);
				contlab = o1;
				brklab = o2;
				goto semi;
			}
			goto syntax;
		}

		case 22:		/* for ( e1 ; e2 ; e3 ) stmt */
		{
			char stepcode[2048];
			int oc = contlab, ob = brklab, toplab;
			if ((o = symbol()) != 6)	/* ( */
				goto syntax;
			if ((peeksym = symbol()) != 1)		/* e1 (init) */
				rcexpr(tree(), efftab);
			if ((o = symbol()) != 1)	/* ; */
				goto syntax;
			contlab = isn++; brklab = isn++; toplab = isn++;
			cg("(block $L%d (loop $L%d ", brklab, toplab);
			if ((peeksym = symbol()) != 1) {	/* e2 (cond) */
				gcond((word *) tree());
				cg("i32.eqz br_if $L%d ", brklab);
			}
			if ((o = symbol()) != 1)	/* ; */
				goto syntax;
			/* e3 (step) appears before the body but runs after it: codegen now,
			 * capture the text, replay it past the body (continue lands here). */
			stepcode[0] = '\0';
			if ((peeksym = symbol()) != 7) {
				cg_cap_begin();
				rcexpr(tree(), efftab);
				strncpy(stepcode, cg_cap_end(), sizeof stepcode - 1);
				stepcode[sizeof stepcode - 1] = '\0';
			}
			if ((o = symbol()) != 7)	/* ) */
				goto syntax;
			cg("(block $L%d ", contlab);	/* continue target wraps the body */
			statement(0);
			cg(")%s br $L%d))", stepcode, toplab);
			contlab = oc; brklab = ob;
			return;
		}

		case 23:		/* repeat stmt  (loop until break/goto) */
		{
			int oc = contlab, ob = brklab, toplab;
			contlab = isn++; brklab = isn++; toplab = isn++;
			cg("(block $L%d (loop $L%d (block $L%d ",
				brklab, toplab, contlab);
			statement(0);
			cg(") br $L%d))", toplab);
			contlab = oc; brklab = ob;
			return;
		}

		case 16:		/* case const :  | case lo :: hi :  (Waterloo range) */
		{
			int lo, hi;
			if ((o = symbol()) != 21)
				goto syntax;
			lo = hi = cval;
			if ((o = symbol()) != 8)	/* : */
				goto syntax;
			if ((peeksym = symbol()) == 8) {	/* second ':' -> range */
				peeksym = -1;
				if ((o = symbol()) != 21)
					goto syntax;
				hi = cval;
				if ((o = symbol()) != 8)	/* closing ':' */
					goto syntax;
			}
			if (lo == hi)
				sw_case(lo);
			else
				sw_caserange(lo, hi);
			goto stmt;
		}

		case 15:		/* switch rvalue statement  (B: no parens) */
			o1 = brklab;
			brklab = isn++;
			np = (word *) tree();	/* stops at the body's '{' */
			sw_begin(np, brklab);
			statement(0);
			sw_end();
			brklab = o1;
			return;

		case 20:		/* default */
			if ((o = symbol()) != 8)	/* : */
				goto syntax;
			sw_default();
			goto stmt;
		}

		error("Unknown keyword");
		goto syntax;

	case 20:			/* name (maybe a label) */
		if (peekc == ':') {
			peekc = 0;
			if (csym[0] > 0) {
				error("Redefinition");
				goto stmt;
			}
			csym[0] = 2;
			if (csym[2] == 0)
				csym[2] = isn++;
			lab_define((int) csym[2]);
			goto stmt;
		}
	}

	peeksym = o;
	rcexpr(tree(), efftab);
	goto semi;

semi:
	if ((o = symbol()) != 1)	/* ; */
		goto syntax;
	return;

syntax:
	error("Statement syntax");
	errflush(o);
	goto stmt;
}

word pexpr()
{
	int o;
	word t;

	if ((o = symbol()) != 6)	/* ( */
		goto syntax;
	t = tree();
	if ((o = symbol()) != 7)	/* ) */
		goto syntax;
	return (t);
syntax:
	error("Statement syntax");
	errflush(o);
	return (0);
}

blkhed()
{
	int al, pl, hl, cls;
	word *cs;
	char buf[NAMSIZ + 1];

	declist();
	al = 0;
	pl = 4;
	g_nparam = 0;
	g_narr = 0;
	while (paraml) {
		*parame = 0;
		cs = paraml;
		paraml = (word *) *cs;
		cs[2] = pl;
		g_paramflt[g_nparam] = 0;
		g_paramoff[g_nparam] = pl;
		g_nparam++;
		*cs = 10;
		pl += rlength(cs[1]);
	}
	cs = hshtab;
	hl = hshsiz;
	while (hl--) {
		if (cs[4]) {
			cls = cs[0];
			if (cls == -2)
				cls = 5;
			if (cls == 5) {			/* auto */
				if (cs[3]) {		/* sized array: storage + ptr + init */
					int esz = length(cs[1] - 020);
					int storage = (cs[3] * esz + 1) & ~1;
					al -= storage;
					if (g_narr < 64) {
						g_arrstore[g_narr] = al;
						al -= rlength(cs[1]);
						cs[2] = al;
						g_arrcell[g_narr] = al;
						g_narr++;
					} else {
						al -= rlength(cs[1]);
						cs[2] = al;
					}
				} else {
					al -= rlength(cs[1]);
					cs[2] = al;
				}
				cs[0] = 5;
			} else if (cls == 10) {		/* parameter */
				cs[0] = 5;
			} else if (cls == 7) {		/* static -> memory */
				cs[2] = galloc(namestr(&cs[4], buf),
					cs[3] ? cs[3] * length(cs[1] - 020) : rlength(cs[1]));
			}
		}
		cs = cs + pssiz;
	}
	g_framesize = pl - al;
	g_autobottom = al;
}

blkend()
{
	int i, hl;

	i = 0;
	hl = hshsiz;
	while (hl--) {
		if (hshtab[i + 4]) {
			if (hshtab[i] == 0) {
				char buf[NAMSIZ + 1];
				error("%s undefined", namestr(&hshtab[i + 4], buf));
			}
			if (hshtab[i] != 1 && hshtab[i] != 4) {	/* keep keywords + manifests */
				hshused--;
				hshtab[i + 4] = 0;
			}
		}
		i += pssiz;
	}
}

errflush(o)
{
	while (o > 3)
		o = symbol();
	peeksym = o;
}

declist()
{
	int o;

	while ((o = symbol()) == 19 & cval < 10)
		declare(cval);
	peeksym = o;
}

easystmt()
{
	if ((peeksym = symbol()) == 20)
		return (peekc != ':');
	if (peeksym == 19) {
		switch (cval)

		case 10:
		case 11:
		case 17:
		case 18:
			return (1);
		return (0);
	}
	return (peeksym != 2);
}
