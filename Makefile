# Build both modernized 1972 C compilers. Use ./cfront --dialect <name> to pick.
.PHONY: all check clean

all:
	$(MAKE) -C c89
	$(MAKE) -C c89-1120

check: all
	$(MAKE) -C c89 watcheck
	$(MAKE) -C c89-1120 watcheck

clean:
	$(MAKE) -C c89 clean
	$(MAKE) -C c89-1120 clean
