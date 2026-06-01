/* C compiler, pass 1 part 3: external definitions, statements, declarations.
   Rewritten to drive the WAT backend (wat.c): control flow is emitted as
   structured wasm here (the 1972 design is syntax-directed), and expressions
   go through rcexpr -> gexpr. */

#include "cc.h"

extdef()
{
	int o, type, width, nel;
	char buf[NAMSIZ + 1];

	if (((o = symbol()) == 0) || o == 1)	/* EOF */
		return;
	type = 0;
	if (o == 19) {			/* keyword */
		if ((type = cval) > 4)
			goto syntax;	/* not type */
	} else {
		if (o == 20)
			csym[4] |= 0200;	/* remember name */
		peeksym = o;
	}
	defsym = 0;
	xdflg++;
	tdeclare(type, 0, 0);
	if (defsym == 0)
		return;
	*defsym = 6;			/* extern */
	namestr(&defsym[4], buf);	/* capture name before the body is parsed */
	strflg = 1;
	xdflg = 0;
	type = defsym[1];
	if ((type & 030) == 020) {	/* a function */
		/* return base type (low 3 bits); scalar float/double return -> f64.
		 * (a function returning a float* is rare and not distinguished here) */
		g_rettype_flt = ((type & 07) == 2 || (type & 07) == 3);
		func_begin();
		lab_begin(isn++);	/* collect body into label segments */
		declist(0);		/* parameter declarations */
		strflg = 0;
		if ((peeksym = symbol()) != 2)	/* not '{' : single-statement body */
			blkhed();
		statement(1);
		lab_end();		/* flush segments (flat or dispatch loop) */
		func_emit(buf);
		return;
	}
	/* a data object: reserve zeroed linear memory */
	width = length(defsym);
	if ((type & 030) == 030)	/* array */
		width = plength(defsym);
	nel = defsym[8];
	{
		int addr = galloc(buf, nel * width);
		if ((peeksym = symbol()) == 1) {	/* uninitialized (zeroed) */
			peeksym = -1;
			return;
		}
		ginit(addr, width);		/* parse the initializer list */
		return;
	}
syntax:
	error("External definition syntax");
	errflush(o);
	statement(0);
}

bxdec()
{
	error("Inconsistent external initialization");
}

/* parse a global initializer list and emit it as a (data) segment.
 * Each element is `width` little-endian bytes at increasing addresses. */
ginit(addr, width)
{
	char data[4096];
	char nm[NAMSIZ + 1];
	int dlen, o, k;
	long v;

	dlen = 0;
	for (;;) {
		o = symbol();
		v = 0;
		switch (o) {

		case 41:			/* - constant */
			if ((o = symbol()) != 21)
				goto syntax;
			v = -cval;
			break;

		case 21:			/* integer constant */
			v = cval;
			break;

		case 20:			/* name: its address */
			if (width != 4)
				bxdec();
			v = gref(namestr(&csym[4], nm));
			break;

		case 22:			/* string: its data address */
			if (width != 4)
				bxdec();
			v = cval;
			break;

		default:
			goto syntax;
		}
		for (k = 0; k < width && dlen < (int) sizeof data; k++)
			data[dlen++] = (char) (v >> (8 * k));
		if ((o = symbol()) == 9)	/* , */
			continue;
		break;
	}
	if (o != 1)				/* ; */
		goto syntax;
	watdata(addr, data, dlen);
	return;
syntax:
	error("External definition syntax");
	errflush(o);
}

statement(d)
{
	int o, o1, o2;
	word *np;

stmt:
	switch (o = symbol()) {

	/* EOF */
	case 0:
		error("Unexpected EOF");
	/* ; */
	case 1:
	/* } */
	case 3:
		return;

	/* { */
	case 2:
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

	/* keyword */
	case 19:
		switch (cval) {

		/* goto */
		case 10:
			if ((o = symbol()) != 20)	/* label name */
				goto syntax;
			if (csym[2] == 0)
				csym[2] = isn++;	/* assign label id */
			lab_goto((int) csym[2]);
			goto semi;

		/* return */
		case 11:
			if ((peeksym = symbol()) == 6)	/* ( */
				gretval((word *) pexpr());
			else
				gretval((word *) 0);	/* void return */
			watret();
			goto semi;

		/* if */
		case 12:
			gcond((word *) pexpr());
			cg("(if (then ");
			statement(0);
			if ((o = symbol()) == 19 & cval == 14) {  /* else */
				cg(") (else ");
				statement(0);
				cg("))");
				return;
			}
			peeksym = o;
			cg("))");
			return;

		/* while */
		case 13:
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

		/* break */
		case 17:
			if (brklab == 0)
				error("Nothing to break from");
			cg("br $L%d ", brklab);
			goto semi;

		/* continue */
		case 18:
			if (contlab == 0)
				error("Nothing to continue");
			cg("br $L%d ", contlab);
			goto semi;

		/* do */
		case 19:
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
			cg(")");			/* close continue block */
			if ((o = symbol()) == 19 & cval == 13) { /* while */
				gcond((word *) tree());
				cg("br_if $L%d ))", looplab);
				contlab = o1;
				brklab = o2;
				goto semi;
			}
			goto syntax;
		}

		/* case */
		case 16:
			if ((o = symbol()) != 21) {	/* constant */
				if (o != 41)		/* - */
					goto syntax;
				if ((o = symbol()) != 21)
					goto syntax;
				cval = -cval;
			}
			if ((o = symbol()) != 8)	/* : */
				goto syntax;
			sw_case(cval);
			goto stmt;

		/* switch */
		case 15:
			o1 = brklab;
			brklab = isn++;
			np = (word *) pexpr();
			sw_begin(np, brklab);
			statement(0);
			sw_end();
			brklab = o1;
			return;

		/* default */
		case 20:
			if ((o = symbol()) != 8)	/* : */
				goto syntax;
			sw_default();
			goto stmt;
		}

		error("Unknown keyword");
		goto syntax;

	/* name */
	case 20:
		if (peekc == ':') {		/* label definition */
			peekc = 0;
			if (csym[0] > 0 && csym[0] != 7) {
				error("Redefinition");
				goto stmt;
			}
			csym[0] = 7;
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
	if ((o = symbol()) != 1)		/* ; */
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
	int al, pl, hl;
	word *cs, *t;
	char buf[NAMSIZ + 1];

	declist(0);
	stack = al = 0;
	pl = 4;
	g_nparam = 0;
	while (paraml) {
		*parame = 0;
		cs = paraml;
		paraml = (word *) *cs;
		if (cs[1] == 2)		/* float args -> double */
			cs[1] = 3;
		cs[2] = pl;
		g_paramflt[g_nparam] = ((cs[1] & 070) == 0 &&
			((cs[1] & 07) == 2 || (cs[1] & 07) == 3));
		g_paramoff[g_nparam] = pl;
		g_nparam++;
		*cs = 10;
		if ((cs[1] & 030) == 030)	/* array */
			cs[1] -= 020;		/* set ref */
		pl += rlength(cs);
	}
	cs = hshtab;
	hl = hshsiz;
	while (hl--) {
	    if (cs[4]) {
		if (cs[0] > 1 & (cs[1] & 07) == 05) {  /* referred structure */
			t = (word *) cs[3];
			cs[3] = t[3];
			cs[1] = cs[1] & 077770 | 04;
		}
		switch ((int) cs[0]) {

		/* sort unmentioned */
		case -2:
			cs[0] = 5;		/* auto */

		/* auto */
		case 5:
			al -= trlength(cs);
			cs[2] = al;
			break;

		/* parameter */
		case 10:
			cs[0] = 5;
			break;

		/* static -> reserve linear memory */
		case 7:
			cs[2] = galloc(namestr(&cs[4], buf), trlength(cs));
			break;

		}
	    }
	    cs = cs + pssiz;
	}
	g_framesize = pl - al;		/* al <= 0 */
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
			if ((hshtab[i + 4] & 0200) == 0) {	/* not top-level */
				hshtab[i + 4] = 0;
				hshused--;
			}
		}
		i += pssiz;
	}
}

errflush(o)
{
	while (o > 3)	/* ; { } */
		o = symbol();
	peeksym = o;
}

declist(mosflg)
{
	int o, offset;

	offset = 0;
	while ((o = symbol()) == 19 & cval < 10)
		if (cval <= 4)
			offset = tdeclare(cval, offset, mosflg);
		else
			scdeclare(cval);
	peeksym = o;
	return (offset);
}

easystmt()
{
	if ((peeksym = symbol()) == 20)	/* name */
		return (peekc != ':');	 /* not label */
	if (peeksym == 19) {		/* keyword */
		switch (cval)

		case 10:	/* goto */
		case 11:	/* return */
		case 17:	/* break */
		case 18:	/* continue */
			return (1);
		return (0);
	}
	return (peeksym != 2);		/* { */
}
