/* C compiler, pass 1 part 4: type-size helpers and the tree consumer.
   Modernized to portable C89 (see cc.h).

   The original rcexpr serialized each parse tree to an intermediate file for
   the (now discarded) PDP-11 backend. Here it instead prints a readable dump
   of the tree, which is the "front end works" signal and the hand-off point
   for the future WAT backend. The custom printf/printn/error from the original
   are gone: printf is the C library's, error lives in runtime.c. */

#include "cc.h"

decref(t)
{
	if ((t & 077770) == 0) {
		error("Illegal indirection");
		return (t);
	}
	return ((t >> 2) & 077770 | t & 07);
}

incref(t)
{
	return ((t << 2) & 077740 | (t & 07) | 010);
}

jumpc(tree, lbl, cond)
word *tree;
{
	rcexpr(block(1, easystmt() + 103, (word) tree, lbl, (word) cond), cctab);
}

/* readable parse-tree dump (replaces the intermediate-file serialization) */
static void dumpnode(tw, depth)
word tw;
int depth;
{
	word *t;
	int op, i;

	t = (word *) tw;
	if (t == 0)
		return;
	op = (int) t[0];
	for (i = 0; i < depth; i++)
		printf("  ");
	switch (op) {

	case 20:		/* name */
		printf("name type=%o class=%o\n", (int) t[1], (int) t[3]);
		return;

	case 21:		/* integer constant */
		printf("const %d\n", (int) t[3]);
		return;

	case 23:		/* float constant */
		printf("fconst L%d\n", (int) t[3]);
		return;

	case 103:		/* conditional branch: subtree is in slot [1] */
	case 104:
		printf("cbranch L%d cond=%d\n", (int) t[2], (int) t[3]);
		dumpnode(t[1], depth + 1);
		return;
	}
	printf("op %d type=%o\n", op, (int) t[1]);
	if (op < 102 && (opdope[op] & 01) != 0) {	/* binary */
		dumpnode(t[3], depth + 1);
		dumpnode(t[4], depth + 1);
	} else if (op >= 29) {				/* unary */
		dumpnode(t[3], depth + 1);
	}
}

/* expression entry point: generate WAT (or dump the tree under -t).
 * table: efftab (1) -> value discarded; others -> value left on the stack. */
rcexpr(tree, table)
word tree;
{
	if (tree == 0)
		return (0);
	if (treedump) {
		printf("# tree (table=%d) line=%d\n", table, line);
		dumpnode(tree, 1);
		return (0);
	}
	gexpr((word *) tree);
	if (table == efftab)
		cg("drop ");
	return (0);
}

/* PDP-11 emitters, unused by the structured WAT path (kept as no-ops so any
 * stray reference links cleanly) */
jump(lab) { }
label(l) { }
branch(lab) { }
setstk(a) { }
retseq() { }

plength(p)
word *p;
{
	int t, l;

	if (((t = p[1]) & 077770) == 0)		/* not a reference */
		return (1);
	p[1] = decref(t);
	l = length(p);
	p[1] = t;
	return (l);
}

length(cs)
word *cs;
{
	int t;

	t = cs[1];
	if ((t & 030) == 030)		/* array */
		t = decref(t);
	if (t >= 010)
		return (4);		/* pointer (wasm32) */
	switch (t & 07) {

	case 0:		/* int */
		return (4);

	case 1:		/* char */
		return (1);

	case 2:		/* float (modeled as f64) */
		return (8);

	case 3:		/* double */
		return (8);

	case 4:		/* structure */
		if (cs >= hshtab)		/* in namelist */
			return (cs[3]);
		return (getlen(cs));

	case 5:
		error("Bad structure");
		return (0);
	}
	error("Compiler error (length)");
	return (0);
}

getlen(p)
word *p;
{
	word *p1;

	switch ((int) *p) {

	case 20:		/* name */
		return (p[2]);

	case 35:
	case 29:		/* & */
	case 36:		/* * */
	case 100:		/* call */
	case 41:		/* - */
		return (getlen((word *) p[3]));

	case 40:		/* + */
		p1 = (word *) p[4];
		if ((p1[1] & 07) == 04)
			return (getlen(p1));
		return (getlen((word *) p[3]));
	}
	error("Unimplemented pointer conversion");
	return (0);
}

rlength(cs)
word *cs;
{
	int l;

	if (((l = length(cs)) & 01) != 0)
		l++;
	return (l);
}

tlength(cs)
word *cs;
{
	int nel;

	if ((nel = cs[8]) == 0)
		nel = 1;
	return (length(cs) * nel);
}

trlength(cs)
word *cs;
{
	int l;

	if (((l = tlength(cs)) & 01) != 0)
		l++;
	return (l);
}
