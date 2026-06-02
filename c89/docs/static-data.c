/* initialized global array */
int dmon[12] 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31;
days(m) { return(dmon[m]); }

/* pointer to a string literal in static data */
char *greet "hi!";
initial() { return(*greet); }

/* function-local static: persists across calls */
nextid() {
	static n;
	n = n + 1;
	return(n);
}
