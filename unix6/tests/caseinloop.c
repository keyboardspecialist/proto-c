/* a case nested inside a loop within a switch -- rejected (Duff's-device
   do-while form: dispatching into a loop body is irreducible control flow). */
f(x) {
	int i;
	i = 0;
	switch (x) {
	case 0:
	do {
	case 1: i = i + 1;
	} while (i < 3);
	}
	return(i);
}
