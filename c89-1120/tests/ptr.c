swap(a, b) int a[]; int b[]; { auto t; t = *a; *a = *b; *b = t; }
sum(a, n) int a[]; { auto i, s; s = 0; i = 0; while (i < n) { s = s + a[i]; i = i + 1; } return(s); }
firstne(a, n) int a[]; {
	int p[];		/* reassignable pointer -- the auto x[] idiom */
	auto i;
	p = a;
	i = 0;
	while (i < n) {
		if (*p) return(*p);
		p = p + 1;
		i = i + 1;
	}
	return(0);
}
