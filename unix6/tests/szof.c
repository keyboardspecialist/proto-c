/* 1975: sizeof yields a compile-time int (bytes). */
struct pt ( int x; int y; );
szi() { return(sizeof(int)); }
szc() { return(sizeof(char)); }
szd() { return(sizeof(double)); }
szp() { return(sizeof(char *)); }
szs() { return(sizeof(struct pt)); }
szv() { int v[10]; return(sizeof(v)); }
