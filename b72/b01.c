/* C compiler (last1120), pass 1 part 2: expression-tree construction.
   Linear type encoding: base 0-4, +020 (16) per pointer reference level.
   Modernized to C89; convert() simplified to explicit scaling for the WAT
   backend (see cc.h, wat.c). */

#include "cc.h"

int maprel[] = { 60, 61, 64, 65, 62, 63, 68, 69, 66, 67 };

build(op)
{
	word *p1, *p2;
	int t1, d1, t2, d2;
	int t, d, dope, lr, cvn;

	t2 = 0;
	d2 = 0;
	t = 0;
	if (op == 4) {		/* a[i] -> *(a + i*WORD)  (B word-indexed vectors) */
		word *idx = (word *) cp[-1];	/* the index operand, top of cp */
		cp[-1] = block(2, 42, 0, idx[2] + 1, (word) idx,
			block(1, 21, 0, 0, (word) length(0)));	/* i * wordsize */
		build(40);
		op = 36;
	}
	dope = opdope[op];
	if ((dope & 01) != 0) {
		p2 = (word *)(*--cp);
		if (p2) {		/* p2 == 0 is the 0-arg call marker */
			t2 = p2[1];
			d2 = p2[2];
		}
	}
	p1 = (word *)(*--cp);
	t1 = p1[1];
	d1 = p1[2];
	switch (op) {

	/* , */
	case 9:
		*cp++ = block(2, 9, 0, 0, (word) p1, (word) p2);
		return;

	/* ? */
	case 90:
		if (*p2 != 8)
			error("Illegal conditional");
		goto goon;

	/* call */
	case 100:
		*cp++ = block(2, 100, t1, 24, (word) p1, (word) p2);
		return;

	/* * : B is typeless -- any word may be dereferenced, yielding a word.
	 * (No "illegal indirection": every cell is both value and address.) */
	case 36:
		t1 = 0;
		if (*p1 != 20 & d1 == 0)
			d1 = 1;
		*cp++ = block(1, 36, t1, d1, (word) p1);
		return;

	/* & unary */
	case 35:
		if (*p1 == 36) {		/* * */
			*cp++ = p1[3];
			return;
		}
		if (*p1 == 20) {
			*cp++ = block(1, p1[3] == 5 ? 29 : 35, t1 + 020, 1, (word) p1);
			return;
		}
		error("Illegal lvalue");
	}
goon:
	if ((dope & 02) != 0)		/* lvalue needed on left? */
		chklval(p1);
	if ((dope & 020) != 0)		/* word operand on left? */
		chkw(p1);
	if ((dope & 040) != 0)		/* word operand on right? */
		chkw(p2);
	if ((dope & 01) != 0) {		/* binary op? */
		cvn = cvtab[9 * lintyp(t1) + lintyp(t2)];
		if ((dope & 010) != 0) {	/* assignment? */
			t = t1;
			lr = 1;
			cvn &= 07;
		} else {
			t = (cvn & 0100) != 0 ? t2 : t1;
			lr = cvn & 0200;
			cvn = (cvn >> 3) & 07;
		}
		if (cvn) {
			if (cvn == 07) {
				error("Illegal conversion");
				goto nocv;
			}
			cvn += (dope & 010) != 0 ? 83 : 93;
			if (lr) {
				t2 = t;
				p2 = (word *) convert(p2, t, d2, cvn);
				d2 = p2[2];
			} else {
				t1 = t;
				p1 = (word *) convert(p1, t, d1, cvn);
				d1 = p1[2];
			}
nocv:		;	}
		if (d2 > d1 & (dope & 0100) != 0) {	/* flip commutative */
			word *pt;
			if ((dope & 04) != 0)		/* relational? */
				op = maprel[op - 60];
			d = d1; d1 = d2; d2 = d;
			pt = p1; p1 = p2; p2 = pt;	/* pointer swap (not via int d!) */
			d = t1; t1 = t2; t2 = d;
		}
		if (d1 == d2)
			d = d1 + 1;
		else
			d = max(d1, d2);
		if ((dope & 04) != 0)
			t = 0;			/* relational is integer */
		*cp++ = block(2, op, t, d, (word) p1, (word) p2);
		return;
	}
	*cp++ = block(1, op, t1, d1 == 0 ? 1 : d1, (word) p1);
}

/*
 * convert: the only effect the WAT backend needs is pointer index scaling.
 * When the target type t is a pointer (t >= 020), the operand is an integer
 * index being added to a pointer -> multiply it by the element size. Scalar
 * (char/int) coercions are identity in the i32 model. (The original's opaque
 * 83-99 conversion ops are bypassed.)
 */
word convert(p, t, d, cvn)
word *p;
int t, d, cvn;
{
	int sz;

	if (t >= 020) {			/* pointer arithmetic scaling */
		sz = length(t - 020);
		if (*p == 21) {		/* constant index folds */
			p[3] *= sz;
			return ((word) p);
		}
		return (block(2, 42, t, d + 2, (word) p, block(1, 21, 0, 0, (word) sz)));
	}
	return ((word) p);
}

chkw(p)
word *p;
{
	int t;
	if ((t = p[1]) > 1 & t < 16)
		error("Integer operand required");
}

lintyp(t)
{
	return (t < 16 ? t : (t < 32 ? t - 12 : 8));
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
