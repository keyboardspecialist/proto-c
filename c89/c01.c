/* C compiler, pass 1 part 2: expression-tree construction and typing.
   Modernized to portable C89 (see cc.h). */

#include "cc.h"

build(op)
{
	word *p1, *p2;
	int t1, d1, t2, d2;
	int d, dope, leftc, cvn, pcvn, t;

	t2 = 0;
	d2 = 0;
	if (op == 4) {		/* [] */
		build(40);	/* + */
		op = 36;	/* * */
	}
	dope = opdope[op];
	if ((dope & 01) != 0) {	/* binary */
		p2 = (word *) disarray((word *)(*--cp));
		t2 = p2[1];
		chkfun(p2);
		d2 = p2[2];
		if (*p2 == 20)
			d2 = 0;
	}
	p1 = (word *) disarray((word *)(*--cp));
	if (op != 100 & op != 35)	/* call, * */
		chkfun(p1);
	t1 = p1[1];
	d1 = p1[2];
	if (*p1 == 20)
		d1 = 0;
	pcvn = 0;
	switch (op) {

	/* : */
	case 8:
		if (t1 != t2)
			error("Type clash in conditional");
		t = t1;
		goto nocv;

	/* , */
	case 9:
		*cp++ = block(2, 9, 0, 0, (word) p1, (word) p2);
		return;

	/* ? */
	case 90:
		if (*p2 != 8)
			error("Illegal conditional");
		t = t2;
		goto nocv;

	/* call */
	case 100:
		if ((t1 & 030) != 020)
			error("Call of non-function");
		*cp++ = block(2, 100, decref(t1), 24, (word) p1, (word) p2);
		return;

	/* * */
	case 36:
		if (*p1 == 35 | *p1 == 29) {	/* & unary */
			*cp++ = p1[3];
			return;
		}
		if (*p1 != 20 & d1 == 0)
			d1 = 1;
		if ((t1 & 030) == 020)		/* function */
			error("Illegal indirection");
		*cp++ = block(1, 36, decref(t1), d1, (word) p1);
		return;

	/* & unary */
	case 35:
		if (*p1 == 36) {		/* * */
			*cp++ = p1[3];
			return;
		}
		if (*p1 == 20) {
			*cp++ = block(1, p1[3] == 5 ? 29 : 35, incref(t1), 1, (word) p1);
			return;
		}
		error("Illegal lvalue");
		break;

	case 43:	/* / */
	case 44:	/* % */
	case 73:	/* =/ */
	case 74:	/* =% */
		d1++;
		d2++;

	case 42:	/* * */
	case 72:	/* =* */
		d1++;
		d2++;
		break;

	case 30:	/* ++ -- pre and post */
	case 31:
	case 32:
	case 33:
		chklval(p1);
		*cp++ = block(2, op, t1, max(d1, 1), (word) p1, (word) plength(p1));
		return;

	case 39:	/* . (structure ref) */
	case 50:	/* -> (indirect structure ref) */
		if (p2[0] != 20 | p2[3] != 4)		/* not mos */
			error("Illegal structure ref");
		*cp++ = (word) p1;
		t = t2;
		if ((t & 030) == 030)	/* array */
			t = decref(t);
		if (op == 39) {		/* "." : p1 is a struct value */
			setype(p1, t);
			build(35);	/* take its address */
		} else {		/* "->" : p1 is already a pointer */
			setype(p1, incref(t));
		}
		*cp++ = block(1, 21, 7, 0, (word) p2[5]);
		build(40);		/* + */
		if ((t2 & 030) != 030)	/* not array */
			build(36);	/* unary * */
		return;
	}
	if ((dope & 02) != 0)		/* lvalue needed on left? */
		chklval(p1);
	if ((dope & 020) != 0)		/* word operand on left? */
		chkw(p1);
	if ((dope & 040) != 0)		/* word operand on right? */
		chkw(p2);
	if ((dope & 01) == 0) {		/* unary op? */
		*cp++ = block(1, op, t1, max(d1, 1), (word) p1);
		return;
	}
	if (t2 == 7) {
		t = t1;
		p2[1] = 0;	/* no int cv for struct */
		t2 = 0;
		goto nocv;
	}
	cvn = cvtab[11 * lintyp(t1) + lintyp(t2)];
	leftc = cvn & 0100;
	t = leftc ? t2 : t1;
	if (op == 80 & t1 != 4 & t2 != 4) {	/* = */
		t = t1;
		if (leftc | cvn != 1)
			goto nocv;
	}
	if ((cvn &= 077)) {
		if (cvn == 077) {
	illcv:
			error("Illegal conversion");
			goto nocv;
		}
		if (cvn > 4 & cvn < 10) {	/* ptr conv */
			t = 0;			/* integer result */
			cvn = 0;
			if ((dope & 04) != 0)	/* relational? */
				goto nocv;
			if (op != 41)	/* - */
				goto illcv;
			pcvn = cvn;
			goto nocv;
		}
		if (leftc) {
			if ((dope & 010) != 0) {	/* =op */
				if (cvn == 1) {
					leftc = 0;
					cvn = 8;
					t = t1;
					goto rcvt;
				} else
					goto illcv;
			}
			p1 = (word *) convert(p1, t, d1, cvn, plength(p2));
			d1 = p1[2];
		} else {
	rcvt:
			p2 = (word *) convert(p2, t, d2, cvn, plength(p1));
			d2 = p2[2];
		}
nocv:	;	}
	if (d1 == d2)
		d = d1 + 1;
	else
		d = max(d1, d2);
	if ((dope & 04) != 0) {		/* relational? */
		if (op > 61 & t >= 010)
			op += 4;	  /* ptr relation */
		t = 0;		/* relational is integer */
	}
	*cp++ = optim((word *) block(2, op, t, d, (word) p1, (word) p2));
	if (pcvn) {
		p1 = (word *)(*--cp);
		*cp++ = block(1, 50 + pcvn, 0, d, (word) p1);
	}
	return;
}

setype(p, t)
word *p;
{
	if ((p[1] & 07) != 4)		/* not structure */
		return;
	p[1] = t;
	switch ((int) *p) {

	case 29:		/* & */
	case 35:
		setype((word *) p[3], decref(t));
		return;

	case 36:		/* * */
		setype((word *) p[3], incref(t));
		return;

	case 40:		/* + */
		setype((word *) p[4], t);
	}
}

chkfun(p)
word *p;
{
	if ((p[1] & 030) == 020)	/* func */
		error("Illegal use of function");
}

word optim(p)
word *p;
{
	word *p1, *p2;
	word t;

	if (*p != 40)				/* + */
		return ((word) p);
	p1 = (word *) p[3];
	p2 = (word *) p[4];
	if (*p1 == 21) {			/* const */
		t = (word) p1;
		p1 = p2;
		p2 = (word *) t;
	}
	if (*p2 != 21)				/* const */
		return ((word) p);
	if ((t = p2[3]) == 0)			/* const 0 */
		return ((word) p1);
	if (*p1 != 35 & *p1 != 29)		/* not & */
		return ((word) p);
	p2 = (word *) p1[3];
	if (*p2 != 20) {			/* name? */
		error("C error (optim)");
		return ((word) p);
	}
	p2[4] += t;
	return ((word) p1);
}

word disarray(p)
word *p;
{
	word t;

	if (((t = p[1]) & 030) != 030 | p[0] == 20 & p[3] == 4)	/* array & not MOS */
		return ((word) p);
	p[1] = decref(t);
	*cp++ = (word) p;
	build(35);				/* add & */
	return (*--cp);
}

word convert(p, t, d, cvn, len)
word *p;
int t, d, cvn, len;
{
	/*
	 * The original wrapped operands in opaque PDP-11 conversion ops (50+cvn)
	 * whose meaning lived in the instruction tables. For the WAT backend we
	 * only need two real effects, both expressible as ordinary tree nodes:
	 *   - pointer index scaling (cvn high): operand * element_size(len)
	 *   - scalar widening (char/int, cvn low): identity in the i32 model
	 *     (memory loads already sign-extend char).
	 * `len` is the element size in modern (wasm32) bytes via length().
	 */
	if (cvn >= 32) {		/* pointer index scaling */
		if (*p == 21) {		/* constant index folds */
			p[3] *= len;
			return ((word) p);
		}
		return (block(2, 42, t, d + 2, (word) p, block(1, 21, 0, 0, (word) len)));
	}
	return ((word) p);		/* scalar coercion: identity */
}

chkw(p)
word *p;
{
	int t;

	if ((t = p[1]) > 1 && t <= 07)
		error("Integer operand required");
}

lintyp(t)
{
	if (t <= 07)
		return (t);
	if ((t & 037) == t)
		return ((t & 07) + 5);
	return (10);
}

word block(int n, word op, word t, word d, ...)
{
	va_list ap;
	word *p = space;

	pblock(op);
	pblock(t);
	pblock(d);
	va_start(ap, d);
	while (n-- > 0)
		pblock(va_arg(ap, word));
	va_end(ap);
	return (word) p;
}

void pblock(word x)
{
	*space++ = x;
	if (--osleft <= 0) {
		error("Expression overflow");
		exit(1);
	}
}

chklval(p)
word *p;
{
	if (*p != 20 & *p != 36)
		error("Lvalue required");
}

max(a, b)
{
	if (a > b)
		return (a);
	return (b);
}
