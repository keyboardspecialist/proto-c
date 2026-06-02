/* Maximum Subarray (Kadane), 1972/1120 dialect.
   a is an int vector, n its length. Returns the largest contiguous sum. */
maxsub(a, n)
int a[]; {
	auto best, cur, i, x;

	best = a[0];
	cur = a[0];
	i = 1;
	while (i < n) {
		x = a[i];
		cur =+ x;		/* cur = cur + x  (ancient =+ op) */
		if (cur < x)		/* previous run was negative -> start fresh */
			cur = x;
		if (cur > best)
			best = cur;
		i =+ 1;
	}
	return(best);
}
