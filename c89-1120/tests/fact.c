fact(n) { if (n < 2) return(1); return(n * fact(n - 1)); }
ifact(n) { auto r; r = 1; while (n > 1) { r = r * n; n = n - 1; } return(r); }
