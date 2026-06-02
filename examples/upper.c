/* read stdin, echo upper-cased, until EOF. 1120/1972 dialect.
   putchar/getchar are undefined here -> the compiler emits wasm imports. */
main() {
	auto c;
	while ((c = getchar()) > 0) {
		if (c >= 'a')
			if (c <= 'z')
				c =- 32;		/* to upper */
		putchar(c);
	}
	return(0);
}
