# Build the unix6 front end (pass 1): C as of the C Reference Manual shipped
# with 6th Edition Unix (1975). A superset of prestruct-c (c89): same archaic
# =+ assignment ops and equals-less initializers, plus the 1975 additions --
# for, sizeof, the #define/#include preprocessor, and the entry/register keywords.
# The sources are pre-K&R: implicit int, K&R definitions, and int/pointer
# interchange are intentional and legal under C89, so the warnings they trip
# are silenced rather than "fixed" away.

CC      ?= clang
CSTD    ?= -std=c89
WARN     = -Wno-implicit-int -Wno-deprecated-non-prototype \
           -Wno-implicit-function-declaration -Wno-int-conversion \
           -Wno-return-type -Wno-parentheses -Wno-unused-label
CFLAGS  ?= $(CSTD) $(WARN)

OBJS = c00.o c01.o c02.o c03.o tables.o runtime.o wat.o

cfront-unix6: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(OBJS): cc.h

.c.o:
	$(CC) $(CFLAGS) -c $<

watcheck: cfront-unix6
	node tests/check.js

clean:
	rm -f $(OBJS) cfront-unix6 tests/*.wat tests/*.wasm
