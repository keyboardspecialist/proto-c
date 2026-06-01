swap(p, q)
int p[];
int q[]; {
	int t;
	t = *p;
	*p = *q;
	*q = t;
}
sum(a, n)
int a[]; {
	int i, s;
	s = 0;
	i = 0;
	while (i < n) {
		s = s + a[i];
		i = i + 1;
	}
	return(s);
}
