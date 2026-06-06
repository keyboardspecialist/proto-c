/* labels + goto: the B loop idiom (no for/do in the language) */

sumto(n) {
	auto s, i;
	s = 0;
	i = 1;
loop:
	if (i > n) goto done;
	s = s + i;
	i = i + 1;
	goto loop;
done:
	return(s);
}
