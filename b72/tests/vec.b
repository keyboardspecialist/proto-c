/* B vectors: `auto a 4` is a bare-size declaration -> a reassignable pointer
 * cell plus 4 words of storage. Indexing is *(a+i); a vector name passed to a
 * function decays to its pointer. triangle() fills a[i]=i+1 and sums -> 10. */

sum(v, n) {
	auto s, i;
	s = 0;
	i = 0;
	while (i < n) {
		s = s + v[i];
		i = i + 1;
	}
	return(s);
}

triangle() {
	auto a 4, i;
	i = 0;
	while (i < 4) {
		a[i] = i + 1;
		i = i + 1;
	}
	return(sum(a, 4));
}
