/* a label inside a switch (goto target) -- rejected: switch re-entry is
   irreducible control flow the segment dispatch cannot express. */
labinsw(x) {
	int r; r = 0;
	switch (x) {
	case 0:
loop:
		r = r + 1;
		if (r < 3) goto loop;
	}
	return(r);
}
