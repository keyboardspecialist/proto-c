classify(n) {
	int r; r = 0;
	switch (n) {
	case 1: r = 10; break;
	case 2:
	case 3: r = 23; break;
	default: r = 99;
	}
	return(r);
}
sumto(n) {
	int i, s; s = 0; i = 0;
loop:
	if (i >= n) goto done;
	s = s + i; i = i + 1;
	goto loop;
done:
	return(s);
}
land(a, b) { if (a & b) return(1); return(0); }
absx(a) { return(a < 0 ? -a : a); }
