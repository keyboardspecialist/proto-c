bsem() {
	int z[3];      /* array name = pointer cell -> 3-int storage (B/BCPL) */
	int save[];    /* a bare reassignable pointer, no storage */
	int moved;

	save = z;      /* capture the address of z's storage */
	z[0] = 100;
	z[1] = 200;
	z[2] = 300;

	z = z + 2;     /* REASSIGN the array name -- slide it to z[2] */
	moved = z[0];  /* now reads the original z[2] = 300 */

	z = save;      /* repoint z back at its storage */
	return(moved + z[0]);   /* 300 + 100 = 400 */
}
