# Build the b72 B compiler (pass 1) + the shared WAT backend.
# B is pre-K&R by nature: implicit int, K&R definitions, and int/pointer
# interchange are intentional and legal under C89, so the warnings they trip
# are silenced rather than "fixed" away. The backend trio (tables.c, runtime.c,
# wat.c) and the expression engine (b01.c, b03.c) are byte-identical to
# ../c89-1120; only b00.c (lexer/keywords/declare) and b02.c (statements) are
# B-specific.

CC      ?= clang
CSTD    ?= -std=c89
WARN     = -Wno-implicit-int -Wno-deprecated-non-prototype \
           -Wno-implicit-function-declaration -Wno-int-conversion \
           -Wno-return-type -Wno-parentheses -Wno-unused-label
CFLAGS  ?= $(CSTD) $(WARN)

OBJS = b00.o b01.o b02.o b03.o tables.o runtime.o wat.o

cfront-b: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(OBJS): cc.h

.c.o:
	$(CC) $(CFLAGS) -c $<

watcheck: cfront-b
	node tests/check.js

clean:
	rm -f $(OBJS) cfront-b tests/*.wat tests/*.wasm
