/* switch with B semantics: no parens, no break -> cases FALL THROUGH.
 * Also exercises the old-form compound assignment '=+'.
 * classify(1)=3 (falls through all three), (2)=2, (3)=1, (9)=0 (no match). */

classify(n) {
	auto r;
	r = 0;
	switch n {
	case 1:
		r =+ 1;
	case 2:
		r =+ 1;
	case 3:
		r =+ 1;
	}
	return(r);
}
