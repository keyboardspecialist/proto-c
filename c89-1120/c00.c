/* C compiler (last1120), pass 1: lexer, parser, unified declarations.
   Modernized to portable C89 with a WAT backend (see cc.h, wat.c). */

#include "cc.h"

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

	while (ai < argc && argv[ai][0] == '-') {	/* leading flags: -t dump, -g debug */
		if (strcmp(argv[ai], "-t") == 0) treedump = 1;
		else if (strcmp(argv[ai], "-g") == 0) dbg = 1;
		ai++;
	}
	if (argc < ai + 1) {
		fprintf(stderr, "usage: cfront [-t] [-g] file\n");
		exit(1);
	}
	if ((fin = fopen(argv[ai], "r")) == NULL) {
		fprintf(stderr, "Can't open %s\n", argv[ai]);
		exit(1);
	}
	fout = stdout;
	init("int", 0);
	init("char", 1);
	init("float", 2);
	init("double", 3);
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
		i += *sp++;
	if (i < 0)
		i = -i;
	i %= HSHSIZ;
	i *= PSSIZ;
	while (*(np = &hshtab[i + 4])) {
		sp = symbuf;
		j = NWPS;
		while (j--)
			if (*np++ != *sp++)
				goto no;
		return (word) &hshtab[i];
	no:	if ((i += PSSIZ) >= HSHLEN)
			i = 0;
	}
	if (hshused++ > HSHSIZ) {
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
	return (word) rp;
}

int symbol()
{
	int b, c;

	if (peeksym >= 0) {
		c = peeksym;
		peeksym = -1;
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
		return (subseq(c, 41, 31));

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

	case 124:	/* number (integer only) */
		cval = 0;
		b = (c == '0') ? 8 : 10;
		while (ctab[c] == 124) {
			cval = cval * b + c - '0';
			c = getchar();
		}
		peekc = c;
		return (21);

	case 122:	/* " */
		return (getstr());

	case 121:	/* ' */
		return (getcc());

	case 123:	/* letter */
	{
		char id[NAMSIZ];
		int k = 0;

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
	sbuf[n++] = '\0';
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
		case 't': return ('\t');
		case 'n': return ('\n');
		case '0': return ('\0');
		case 'r': return ('\r');
		case 'b': return ('\b');
		case 'f': return ('\f');
		case '\n': line++; return ('\n');
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
	*op = 200;
	*pp = 06;
	andflg = 0;

advanc:
	switch (o = symbol()) {

	/* name */
	case 20:
		if (*csym == 0)
			if ((peeksym = symbol()) == 6)
				*csym = 6;		/* extern */
			else {
				if (csym[2] == 0)
					csym[2] = isn++;
			}
		if (*csym == 6)			/* extern: name embedded at p[5..] */
			*cp++ = block(6, 20, csym[1], 0, (word) *csym, (word) 0,
				csym[4], csym[5], csym[6], csym[7]);
		else
			*cp++ = block(3, 20, csym[1], 0, (word) *csym, (word) 0,
				csym[2]);
		goto tand;

	/* short constant */
	case 21:
	case21:
		*cp++ = block(1, 21, ctyp, 0, (word) cval);
		goto tand;

	/* string constant */
	case 22:
		*cp++ = block(1, 22, 17, 0, (word) cval);

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
	}

	/* binaries */
	if (!andflg)
		goto syntax;
	andflg = 0;

oponst:
	p = (opdope[o] >> 9) & 077;
opon1:
	ps = *pp;
	if (p > ps | p == ps & (opdope[o] & 0200) != 0) {
putin:
		switch (o) {
		case 6:
		case 4:
		case 100:
			p = 04;
		}
		if (op >= opst + 20) {
			error("expression overflow");
			exit(1);
		}
		*++op = o;
		*++pp = p;
		goto advanc;
	}
	--pp;
	switch (os = *op--) {

	case 200:
		peeksym = o;
		return (*--cp);

	case 100:
		if (o != 7)
			goto syntax;
		build(os);
		goto advanc;

	case 101:
		*cp++ = 0;		/* 0 arg call */
		os = 100;
		goto fbuild;

	case 6:
		if (o != 7)
			goto syntax;
		goto advanc;

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

/* unified declaration: storage class (kw>=5) and/or type (kw<5), each name
   may carry [] vector brackets -- the form last1120 actually uses. */
declare(kw)
{
	int o;
	char buf[NAMSIZ + 1];

	while ((o = symbol()) == 20) {		/* name */
		if (kw >= 5) {			/* storage class / sort */
			if (*csym > 0)
				error("%s redeclared", namestr(&csym[4], buf));
			*csym = kw;
		} else {			/* type */
			if ((csym[1] & 017) != 0)
				error("%s redeclared", namestr(&csym[4], buf));
			csym[1] |= (csym[1] & 0760) | kw;
			if (*csym == 0)
				*csym = -2;
		}
		while ((o = symbol()) == 4) {	/* [ */
			if ((o = symbol()) == 21) {	/* const */
				if (csym[1] >= 020)
					error("Bad vector");
				csym[3] = cval;
				o = symbol();
			}
			if (o != 5)		/* ] */
				goto syntax;
			csym[1] += 020;		/* one reference level */
		}
		if (kw == 8) {			/* parameter */
			*csym = -1;
			if (paraml == 0)
				paraml = csym;
			else
				*parame = (word) csym;
			parame = csym;
		}
		if (o != 9)			/* , */
			break;
	}
	if (o == 1 & kw != 8 | o == 7 & kw == 8)
		return;
syntax:
	error("Declaration syntax");
	errflush(o);
}

/* storage */

int regtab = 0;
int efftab = 1;
int cctab = 2;
int sptab = 3;
word symbuf[NWPS];
int pssiz = 8;
int namsiz = 8;
int nwps = 4;
int hshused = 0;
int hshsiz = 100;
int hshlen = 800;
word hshtab[HSHLEN];
word *space;
word *cp;
int cmsiz = 40;
word cmst[CMSIZ];
int ctyp = 0;
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
int peeksym = -1;	/* was 0177777 = -1 in 16-bit */
int peekc;
int eof;
int line = 1;
word *csym;
int cval;
int ncpw = 2;
int nerror;
word *paraml;
word *parame;
int treedump;
int dbg;

word osbuf[OSSIZ];
int osleft;
int ossiz = OSSIZ;
