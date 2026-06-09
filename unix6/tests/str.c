int counter;

strlen(s)
char s[]; {
	int n;
	n = 0;
	while (*s++)
		n = n + 1;
	return(n);
}
bump() {
	counter = counter + 1;
	return(counter);
}
