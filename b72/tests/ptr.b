/* B word pointers: bare pointer arithmetic steps WORDS, no scaling.
 * slide() reassigns a vector name (z = z + 2) to slide it two words, the
 * classic B/BCPL reassignable-pointer idiom. firstne() walks a vector with
 * p = p + 1 and dereferences with *p. */

slide() {
	auto z 3, save, moved;
	save = z;
	z[0] = 100;
	z[1] = 200;
	z[2] = 300;
	z = z + 2;		/* slide two words -> points at z[2] */
	moved = z[0];		/* 300 */
	z = save;		/* repoint at storage */
	return(moved + z[0]);	/* 300 + 100 = 400 */
}

firstne(v, n) {
	auto p, i;
	p = v;
	i = 0;
	while (i < n) {
		if (*p) return(*p);
		p = p + 1;	/* step one word */
		i = i + 1;
	}
	return(0);
}
