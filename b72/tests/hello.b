/* string literal lowering: '*n' is newline, the string is EOT('*e',04)
 * terminated (not NUL). greeting() returns the data address; the harness reads
 * memory until 04 and decodes "Hi!\n". */

greeting() {
	return("Hi!*n");
}
