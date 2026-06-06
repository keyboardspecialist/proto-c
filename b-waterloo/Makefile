# Build the b-waterloo B compiler (pass 1) + the WAT backend.
# Waterloo B (Honeywell 6000/66, 1978; descendant GCOS8 B) is the richer later
# dialect: for/do/repeat, break/next, switch with default + range cases, modern
# op= assignment, && / ||, and f32 floats (#-prefixed operators). Pre-K&R style
# (implicit int, K&R defs, int/pointer interchange) is intentional, so those
# warnings are silenced rather than "fixed" away. Forked from ../b72.

CC      ?= clang
CSTD    ?= -std=c89
WARN     = -Wno-implicit-int -Wno-deprecated-non-prototype \
           -Wno-implicit-function-declaration -Wno-int-conversion \
           -Wno-return-type -Wno-parentheses -Wno-unused-label
CFLAGS  ?= $(CSTD) $(WARN)

OBJS = b00.o b01.o b02.o b03.o tables.o runtime.o wat.o

cfront-bw: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(OBJS): cc.h

.c.o:
	$(CC) $(CFLAGS) -c $<

watcheck: cfront-bw
	node tests/check.js

clean:
	rm -f $(OBJS) cfront-bw tests/*.wat tests/*.wasm
