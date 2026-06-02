/* count input characters, print the count as decimal. */
putn(n) {
	if (n > 9)
		putn(n / 10);
	putchar(n - n / 10 * 10 + '0');
}
main() {
	auto c, k;
	k = 0;
	while ((c = getchar()) > 0)
		k =+ 1;
	putn(k);
	putchar('\n');
	return(0);
}
