/* 1975: the for statement. */
forsum(n) { int i, s; s = 0; for (i = 0; i < n; i++) s =+ i; return(s); }
forbrk()  { int i; for (i = 0;; i++) if (i >= 7) break; return(i); }
