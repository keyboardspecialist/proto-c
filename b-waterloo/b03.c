/* C compiler (last1120), pass 1 part 4: type sizes + tree consumer.
   length() takes a TYPE (not a symbol) in last1120. rcexpr feeds the WAT
   backend (or dumps trees under -t). PDP-11 emitters are neutered. */

#include "cc.h"

length(t)
{
	if (t < 0)
		t += 020;
	if (t >= 020)
		return (4);		/* pointer (wasm32) */
	switch (t) {
	case 0:		/* int   */
		return (4);
	case 1:		/* char  */
		return (1);
	case 2:		/* float (modeled f64) */
		return (8);
	case 3:		/* double */
		return (8);
	case 4:
		return (4);
	}
	return (1024);
}

rlength(c)
{
	int l;
	return ((l = length(c)) == 1 ? 2 : l);
}

/* readable parse-tree dump (-t) */
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
	case 20:
		printf("name type=%o class=%o\n", (int) t[1], (int) t[3]);
		return;
	case 21:
		printf("const %d\n", (int) t[3]);
		return;
	case 22:
		printf("string @%d\n", (int) t[3]);
		return;
	case 103:
	case 104:
		printf("cbranch L%d cond=%d\n", (int) t[2], (int) t[3]);
		dumpnode(t[1], depth + 1);
		return;
	}
	printf("op %d type=%o\n", op, (int) t[1]);
	if (op < 102 && (opdope[op] & 01) != 0) {
		dumpnode(t[3], depth + 1);
		dumpnode(t[4], depth + 1);
	} else if (op >= 29) {
		dumpnode(t[3], depth + 1);
	}
}

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

/* PDP-11 emitters, unused by the structured WAT path */
jump(lab) { }
label(l) { }
branch(lab) { }
retseq() { }
slabel() { }
setstk(a) { }
defvec() { }
defstat(s) word *s; { }

jumpc(tree, lbl, cond)
word *tree;
{
}
