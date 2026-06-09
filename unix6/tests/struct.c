struct pt ( int x; int y; );
g() { struct pt p; p.x = 5; p.y = 7; return(p.x + p.y); }
setp(p, a, b) struct pt *p; { p->x = a; p->y = b; }
dot(p, q) struct pt *p; struct pt *q; { return(p->x * q->x + p->y * q->y); }
