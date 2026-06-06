/* recursion, if/return, while, auto scalars */

fact(n) {
	if (n < 2) return(1);
	return(n * fact(n - 1));
}

ifact(n) {
	auto p, i;
	p = 1;
	i = 1;
	while (i <= n) {
		p = p * i;
		i = i + 1;
	}
	return(p);
}
