n 42;
v[3] 10, 20, 30;
getn() { extern n; return(n); }
getv(i) { extern v[]; return(v[i]); }
bump() { extern n; n = n + 1; return(n); }
