/* C compiler

   Copyright 1972 Bell Telephone Laboratories, Inc.

   Pass 1: lexer, expression parser, declaration parser, symbol table.
   Modernized to portable C89 (see cc.h); PDP-11 specifics removed.
*/

#include "cc.h"

/* enter a keyword into the symbol table */
init(s, t)
char *s;
{
	word *np;
	int i;

	for (i = 0; i < NWPS; i++)
		symbuf[i] = 0;
	for (i = 0; i < NAMSIZ && s[i]; i++)
		symbuf[i / NCPW] |= ((word)(unsigned char) s[i]) << (8 * (i % NCPW));
	np = (word *) lookup();
	*np++ = 1;
	*np = t;
}

int main(argc, argv)
int argc;
char **argv;
{
	int ai = 1;

	if (argc >= 2 && strcmp(argv[1], "-t") == 0) {	/* dump trees, no WAT */
		treedump = 1;
		ai = 2;
	}
	if (argc < ai + 1) {
		fprintf(stderr, "usage: cfront [-t] file\n");
		exit(1);
	}
	if ((fin = fopen(argv[ai], "r")) == NULL) {
		fprintf(stderr, "Can't open %s\n", argv[ai]);
		exit(1);
	}
	fout = stdout;
	xdflg++;
	init("int", 0);
	init("char", 1);
	init("float", 2);
	init("double", 3);
	init("struct", 4);
	init("auto", 5);
	init("extern", 6);
	init("static", 7);
	init("goto", 10);
	init("return", 11);
	init("if", 12);
	init("while", 13);
	init("else", 14);
	init("switch", 15);
	init("case", 16);
	init("break", 17);
	init("continue", 18);
	init("do", 19);
	init("default", 20);
	xdflg = 0;
	watmodopen();
	while (!eof) {
		extdef();
		blkend();
	}
	watmodclose();
	flush();
	exit(nerror != 0);
	return 0;
}

word lookup()
{
	word *np, *sp, *rp;
	int i, j;

	i = 0;
	sp = symbuf;
	j = NWPS;
	while (j--)
		i += *sp++ & 077577;
	if (i < 0)
		i = -i;
	i %= HSHSIZ;
	i *= PSSIZ;
	while (*(np = &hshtab[i + 4])) {
		sp = symbuf;
		j = NWPS;
		while (j--)
			if ((*np++ & 077577) != *sp++)
				goto no;
		return (word) &hshtab[i];
	no:	if ((i += PSSIZ) >= HSHLEN)
			i = 0;
	}
	if (++hshused > HSHSIZ) {
		error("Symbol table overflow");
		exit(1);
	}
	rp = np = &hshtab[i];
	sp = symbuf;
	j = 4;
	while (j--)
		*np++ = 0;
	j = NWPS;
	while (j--)
		*np++ = *sp++;
	*np = 0;
	if (xdflg)
		rp[4] |= 0200;		/* mark non-deletable */
	return (word) rp;
}

int symbol()
{
	int c;

	if (peeksym >= 0) {
		c = peeksym;
		peeksym = -1;
		if (c == 20)
			mosflg = 0;
		return (c);
	}
	if (peekc) {
		c = peekc;
		peekc = 0;
	} else
		if (eof)
			return (0);
		else
			c = getchar();
loop:
	switch (ctab[c]) {

	case 125:	/* newline */
		line++;

	case 126:	/* white space */
		c = getchar();
		goto loop;

	case 0:		/* EOF */
		eof++;
		return (0);

	case 40:	/* + */
		return (subseq(c, 40, 30));

	case 41:	/* - */
		return (subseq(c, subseq('>', 41, 50), 31));

	case 80:	/* = */
		if (subseq(' ', 0, 1))
			return (80);
		c = symbol();
		if (c >= 40 & c <= 49)
			return (c + 30);
		if (c == 80)
			return (60);
		peeksym = c;
		return (80);

	case 63:	/* < */
		if (subseq(c, 0, 1))
			return (46);
		return (subseq('=', 63, 62));

	case 65:	/* > */
		if (subseq(c, 0, 1))
			return (45);
		return (subseq('=', 65, 64));

	case 34:	/* ! */
		return (subseq('=', 34, 61));

	case 43:	/* / */
		if (subseq('*', 1, 0))
			return (43);
com:
		c = getchar();
com1:
		if (c == '\0') {
			eof++;
			error("Nonterminated comment");
			return (0);
		}
		if (c == '\n')
			line++;
		if (c != '*')
			goto com;
		c = getchar();
		if (c != '/')
			goto com1;
		c = getchar();
		goto loop;

	case 120:	/* . */
	case 124:	/* number */
		peekc = c;
		switch (c = getnum(c == '0' ? 8 : 10)) {
		case 25:		/* float 0 */
			c = 23;
			break;

		case 23:		/* float non 0 */
			cval = isn++;
		}
		return (c);

	case 122:	/* " */
		return (getstr());

	case 121:	/* ' */
		return (getcc());

	case 123:	/* letter */
	{
		char id[NAMSIZ];
		int k = 0;

		if (mosflg) {
			id[k++] = '.';
			mosflg = 0;
		}
		while (ctab[c] == 123 | ctab[c] == 124) {
			if (k < NAMSIZ)
				id[k++] = c;
			c = getchar();
		}
		while (k < NAMSIZ)
			id[k++] = '\0';
		peekc = c;
		for (k = 0; k < NWPS; k++)
			symbuf[k] = 0;
		for (k = 0; k < NAMSIZ; k++)
			symbuf[k / NCPW] |= ((word)(unsigned char) id[k]) << (8 * (k % NCPW));
		csym = (word *) lookup();
		if (csym[0] == 1) {	/* keyword */
			cval = csym[1];
			return (19);
		}
		return (20);
	}

	case 127:	/* unknown */
		error("Unknown character");
		c = getchar();
		goto loop;

	}
	return (ctab[c]);
}

int subseq(c, a, b)
{
	if (!peekc)
		peekc = getchar();
	if (peekc != c)
		return (a);
	peekc = 0;
	return (b);
}

int getstr()
{
	char sbuf[2048];
	int c, n;

	n = 0;
	while ((c = mapch('"')) >= 0)
		if (n < (int) sizeof sbuf - 1)
			sbuf[n++] = c;
	sbuf[n++] = '\0';		/* NUL terminator */
	cval = gdata(n);		/* address of the string data */
	watdata(cval, sbuf, n);
	return (22);
}

int getcc()
{
	int c, cc;
	char *cp2;

	cval = 0;
	cp2 = (char *) &cval;
	cc = 0;
	while ((c = mapch('\'')) >= 0)
		if (cc++ < NCPW)
			*cp2++ = c;
	if (cc > NCPW)
		error("Long character constant");
	return (21);
}

int mapch(c)
{
	int a;

	if ((a = getchar()) == c)
		return (-1);
	switch (a) {

	case '\n':
	case 0:
		error("Nonterminated string");
		peekc = a;
		return (-1);

	case '\\':
		switch (a = getchar()) {

		case 't':
			return ('\t');

		case 'n':
			return ('\n');

		case '0':
			return ('\0');

		case 'r':
			return ('\r');

		case '\n':
			line++;
			return ('\n');
		}

	}
	return (a);
}

word tree()
{
	int opst[20], prst[20], *op, *pp;
	int andflg, o, p, ps, os;

	osleft = ossiz;
	space = &osbuf[1];
	osbuf[0] = 0;
	op = opst;
	pp = prst;
	cp = cmst;
	*op = 200;		/* stack EOF */
	*pp = 06;
	andflg = 0;

advanc:
	switch (o = symbol()) {

	/* name */
	case 20:
		if (*csym == 0)
			if ((peeksym = symbol()) == 6) {	/* ( */
				*csym = 6;		/* extern */
				csym[1] = 020;		/* int() */
			} else {
				csym[1] = 030;		/* array */
				if (csym[2] == 0)
					csym[2] = isn++;
			}
		*cp++ = block(2, 20, csym[1], csym[3], (word) *csym, (word) 0);
		if (*csym == 6) {		/* external */
			o = 3;
			while (++o < 8) {
				pblock(csym[o]);
				if ((csym[o] & 077400) == 0)
					break;
			}
		} else
			pblock(csym[2]);
		goto tand;

	/* short constant */
	case 21:
	case21:
		*cp++ = block(1, 21, ctyp, 0, (word) cval);
		goto tand;

	/* floating constant: stash the f64 bit pattern in the node's value slot */
	case 23:
	{
		word w;
		double d;
		d = fcval;
		memcpy(&w, &d, sizeof(double));
		*cp++ = block(1, 23, 3, 0, w);
		goto tand;
	}

	/* string constant: fake a static char array */
	case 22:
		*cp++ = block(3, 20, 031, 1, (word) 7, (word) 0, (word) cval);

tand:
		if (cp >= cmst + cmsiz) {
			error("Expression overflow");
			exit(1);
		}
		if (andflg)
			goto syntax;
		andflg = 1;
		goto advanc;

	/* ++, -- */
	case 30:
	case 31:
		if (andflg)
			o += 2;
		goto oponst;

	/* ! */
	case 34:
	/* ~ */
	case 38:
		if (andflg)
			goto syntax;
		goto oponst;

	/* - */
	case 41:
		if (!andflg) {
			peeksym = symbol();
			if (peeksym == 21) {
				peeksym = -1;
				cval = -cval;
				goto case21;
			}
			o = 37;
		}
		andflg = 0;
		goto oponst;

	/* & */
	/* * */
	case 47:
	case 42:
		if (andflg)
			andflg = 0;
		else
			if (o == 47)
				o = 35;
			else
				o = 36;
		goto oponst;

	/* ( */
	case 6:
		if (andflg) {
			o = symbol();
			if (o == 7)
				o = 101;
			else {
				peeksym = o;
				o = 100;
				andflg = 0;
			}
		}
		goto oponst;

	/* ) */
	/* ] */
	case 5:
	case 7:
		if (!andflg)
			goto syntax;
		goto oponst;

	case 39:	/* . */
	case 50:	/* -> */
		mosflg++;
		break;

	}
	/* binaries */
	if (!andflg)
		goto syntax;
	andflg = 0;

oponst:
	p = (opdope[o] >> 9) & 077;
opon1:
	ps = *pp;
	if (p > ps | p == ps & (opdope[o] & 0200) != 0) { /* right-assoc */
putin:
		switch (o) {

		case 6: /* ( */
		case 4: /* [ */
		case 100: /* call */
			p = 04;
		}
		if (op >= opst + 20) {		/* opstack size */
			error("expression overflow");
			exit(1);
		}
		*++op = o;
		*++pp = p;
		goto advanc;
	}
	--pp;
	switch (os = *op--) {

	/* EOF */
	case 200:
		peeksym = o;
		return (*--cp);

	/* call */
	case 100:
		if (o != 7)
			goto syntax;
		build(os);
		goto advanc;

	/* mcall */
	case 101:
		*cp++ = block(0, 0, 0, 0);	/* 0 arg call */
		os = 100;
		goto fbuild;

	/* ( */
	case 6:
		if (o != 7)
			goto syntax;
		goto advanc;

	/* [ */
	case 4:
		if (o != 5)
			goto syntax;
		build(4);
		goto advanc;
	}
fbuild:
	build(os);
	goto opon1;

syntax:
	error("Expression syntax");
	errflush(o);
	return (0);
}

scdeclare(kw)
{
	int o;

	while ((o = symbol()) == 20) {		/* name */
		if (*csym > 0 & *csym != kw)
			redec();
		*csym = kw;
		if (kw == 8) {		/* parameter */
			*csym = -1;
			if (paraml == 0)
				paraml = csym;
			else
				*parame = (word) csym;
			parame = csym;
		}
		if ((o = symbol()) != 9)	/* , */
			break;
	}
	if (o == 1 & kw != 8 | o == 7 & kw == 8)
		return;
	decsyn(o);
}

tdeclare(kw, offset, mos)
{
	int o;
	word elsize, *ds, *ssym;

	elsize = 0;
	if (kw == 4) {				/* struct */
		ssym = 0;
		ds = defsym;
		mosflg = mos;
		if ((o = symbol()) == 20) {	/* name */
			ssym = csym;
			o = symbol();
		}
		mosflg = mos;
		if (o != 6) {			/* ( */
			if (ssym == 0)
				goto syntax;
			if (*ssym != 8)		/* class structname */
				error("Bad structure name");
			if (ssym[3] == 0) {	/* no size yet */
				kw = 5;		/* deferred MOS */
				elsize = (word) ssym;
			} else
				elsize = ssym[3];
			peeksym = o;
		} else {
			if (ssym) {
				if (*ssym)
					redec();
				*ssym = 8;
				ssym[3] = 0;
			}
			elsize = declist(4);
			if ((elsize & 01) != 0)
				elsize++;
			defsym = ds;
			if ((o = symbol()) != 7)	/* ) */
				goto syntax;
			if (ssym)
				ssym[3] = elsize;
		}
	}
	mosflg = mos;
	if ((peeksym = symbol()) == 1) {	/* ; */
		peeksym = -1;
		mosflg = 0;
		return (offset);
	}
	do {
		offset += t1dec(kw, offset, mos, elsize);
		if (xdflg & !mos)
			return (offset);
	} while ((o = symbol()) == 9);		/* , */
	if (o == 1)
		return (offset);
syntax:
	decsyn(o);
	return (offset);
}

t1dec(kw, offset, mos, elsize)
int kw, offset, mos;
word elsize;
{
	int type, nel, t1;

	nel = 0;
	mosflg = mos;
	if ((t1 = getype(&nel)) < 0)
		goto syntax;
	type = 0;
	do
		type = type << 2 | (t1 & 03);
	while (t1 >>= 2);
	t1 = type << 3 | kw;
	if (defsym[1] & defsym[1] != t1)
		redec();
	defsym[1] = t1;
	defsym[3] = elsize;
	elsize = length(defsym);
	if (mos) {
		if (*defsym)
			redec();
		else
			*defsym = 4;
		if ((offset & 1) != 0 & elsize != 1)
			offset++;
		defsym[2] = offset;
	} else
		if (*defsym == 0)
			*defsym = -2;		/* default auto */
	if (nel == 0)
		nel = 1;
	defsym[8] = nel;
syntax:
	return (nel * elsize);
}

getype(pnel)
int pnel[];
{
	word o;
	int type;

	switch ((int)(o = symbol())) {

	case 42:					/* * */
		return (getype(pnel) << 2 | 01);

	case 6:						/* ( */
		type = getype(pnel);
		if ((o = symbol()) != 7)		/* ) */
			goto syntax;
		goto getf;

	case 20:					/* name */
		defsym = csym;
		type = 0;
	getf:
		switch ((int)(o = symbol())) {

		case 6:					/* ( */
			if (xdflg) {
				xdflg = 0;
				o = (word) defsym;
				scdeclare(8);
				defsym = (word *) o;
				xdflg++;
			} else
				if ((o = symbol()) != 7)	/* ) */
					goto syntax;
			type = type << 2 | 02;
			goto getf;

		case 4:					/* [ */
			if ((o = symbol()) != 5) {	/* ] */
				if (o != 21)		/* const */
					goto syntax;
				*pnel = cval;
				if ((o = symbol()) != 5)
					goto syntax;
			}
			type = type << 2 | 03;
			goto getf;
		}
		peeksym = o;
		return (type);
	}
syntax:
	decsyn((int) o);
	return (-1);
}

decsyn(o)
{
	error("Declaration syntax");
	errflush(o);
}

redec()
{
	char buf[NAMSIZ + 1];

	error("%s redeclared", namestr(&csym[4], buf));
}

/* storage */

int regtab = 0;
int efftab = 1;
int cctab = 2;
int sptab = 3;
word symbuf[NWPS];
int pssiz = 9;
int namsiz = 8;
int nwps = 4;
int hshused;
int hshsiz = 100;
int hshlen = 900;	/* 9*hshsiz */
word hshtab[HSHLEN];
word *space;
word *cp;
int cmsiz = 40;
word cmst[CMSIZ];
int ctyp;
int isn = 1;
int swsiz = 120;
word swtab[120];
word *swp;
int contlab;
int brklab;
int deflab;
int nreg = 4;
int nauto;
int stack;
int peeksym = -1;	/* was 0177777, i.e. -1 in 16-bit; sentinel "no peek" */
int peekc;
int eof;
int line = 1;
word *defsym;
int xdflg;
word *csym;
int cval;
double fcval = 0;	/* a double number */
int ncpw = 2;
int nerror;
word *paraml;
word *parame;
int strflg;
int ossiz = OSSIZ;
int osleft;
int mosflg;
int debug = 0;
int treedump;

word osbuf[OSSIZ];
