/* regression: a switch case body exceeding 1KB once truncated in cg().
   case 0 sums 1..40 = 820; other x falls through to 0. */
swbig(x) {
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
		r = r + 31;
		r = r + 32;
		r = r + 33;
		r = r + 34;
		r = r + 35;
		r = r + 36;
		r = r + 37;
		r = r + 38;
		r = r + 39;
		r = r + 40;
	}
	return(r);
}
