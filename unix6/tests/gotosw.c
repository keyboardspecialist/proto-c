/* regression: goto exiting a switch, with a >1KB case body. case 0: sum
   1..30 then goto out -> 465. case 1 -> 2; case 2 -> 3+100; no match -> 100. */
gotosw(x) {
	int r; r = 0;
	switch (x) {
	case 0:
		r = r + 1;
		r = r + 2;
		r = r + 3;
		r = r + 4;
		r = r + 5;
		r = r + 6;
		r = r + 7;
		r = r + 8;
		r = r + 9;
		r = r + 10;
		r = r + 11;
		r = r + 12;
		r = r + 13;
		r = r + 14;
		r = r + 15;
		r = r + 16;
		r = r + 17;
		r = r + 18;
		r = r + 19;
		r = r + 20;
		r = r + 21;
		r = r + 22;
		r = r + 23;
		r = r + 24;
		r = r + 25;
		r = r + 26;
		r = r + 27;
		r = r + 28;
		r = r + 29;
		r = r + 30;
		goto out;
	case 1: r = 2; goto out;
	case 2: r = 3;
	}
	r = r + 100;
out:
	return(r);
}
