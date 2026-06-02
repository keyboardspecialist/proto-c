# Build the modernized prestruct-c front end (pass 1).
# The 1972 sources are pre-K&R: implicit int, K&R definitions, and int/pointer
# interchange are intentional and legal under C89, so the warnings they trip
# are silenced rather than "fixed" away.

CC      ?= clang
CSTD    ?= -std=c89
WARN     = -Wno-implicit-int -Wno-deprecated-non-prototype \
           -Wno-implicit-function-declaration -Wno-int-conversion \
           -Wno-return-type -Wno-parentheses -Wno-unused-label
CFLAGS  ?= $(CSTD) $(WARN)

OBJS = c00.o c01.o c02.o c03.o tables.o runtime.o wat.o

cfront: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(OBJS): cc.h

.c.o:
	$(CC) $(CFLAGS) -c $<

watcheck: cfront
	node tests/check.js

clean:
	rm -f $(OBJS) cfront tests/*.wat tests/*.wasm
