# last1120-c, modernized to C89, compiling to WebAssembly

The same treatment as `../c89/` (prestruct), applied to **last1120-c** — the
*older* sibling compiler (`../last1120/c00.c`–`c03.c`): the last PDP-11/20 C
compiler, predating structures. Front end ported to portable C89, with the WAT
backend from `../c89/` adapted to last1120's conventions.

```
make            # builds ./cfront-1120
make watcheck   # compile+run the regression suite in node
./cfront-1120 f.c   # emit a WAT module (or ../cfront --dialect 1120)
./cfront-1120 -t f.c # dump parse trees
```

## Why a second compiler

last1120 is older than prestruct, and its declaration grammar is **looser**: a
single `declare()` parses storage class, type, and `[]` brackets together, so
`auto x[];` — a reassignable array pointer — is legal. prestruct split that into
`scdeclare`/`tdeclare` and dropped `[]` from the storage-class path, so prestruct
(and our `../c89/` port) *rejects* `auto x[]` even though prestruct's own source
uses it. This port handles the older, looser dialect.

```c
firstne(a, n) int a[]; {
    int p[];            /* reassignable pointer -- rejected by ../c89/ */
    auto i;
    p = a; i = 0;
    while (i < n) { if (*p) return(*p); p = p + 1; i = i + 1; }
    return(0);
}
```

## Differences from the prestruct port

What changed versus `../c89/`; the WAT infrastructure (frame, structured control,
switch/goto, imports, data segments) is identical.

- **Unified `declare()`** (c00.c) — storage class + type + `[]` in one loop;
  reassignable array pointers fall out for free.
- **Untyped globals** (c02.c `extdef`) — last1120 globals are int words written
  `name`, `name const`, `name[n] inits`, or `name() body` (no type keyword).
  An array global is a **pointer cell holding the address of its storage**, so
  `extern v[]` works; locals follow the same pointer+storage model.
- **Linear type encoding** — base 0-4, **+020 (16) per pointer level** (vs
  prestruct's 2-bit groups). `wat.c`'s `ischar`/`isftype` and `length()` and the
  `++`/`--` step are adapted accordingly; `convert()` scales a pointer index by
  the element size when the result type is a pointer.
- **Integer-only lexer** — numbers are lexed inline in `symbol()`; there is no
  `getnum` and no float literals.
- **No structures / no `.`/`->`/`mosflg`**; `ctab['.']` is unknown, `opdope` has
  no member operators.
- `pssiz = 8` (no `nel` slot); `lookup()` hashes without prestruct's keep-flag
  mask.

## Bugs found in the port

- **Pointer truncation in `build`'s commutative swap** (c01.c): the original
  swaps the two operand *pointers* through an `int` temp, which truncates a
  64-bit pointer. Fixed with a pointer-typed temp. (prestruct's `build` doesn't
  commute, so this was new here.)

## Faithful dialect quirks (kept)

- Identifiers truncate to 8 chars.
- `&`/`|` are logical short-circuit operators in a condition (no `&&`/`||`).
- **`pointer += k` does not scale** by element size (`build`'s assignment path
  masks the conversion code to its low bits, which are 0 for `int*`). Only the
  expression form `pointer + k` scales. Write `p = p + 1`, not `p =+ 1`.

## Files

`cc.h` `tables.c` `runtime.c` `c00.c` `c01.c` `c02.c` `c03.c` `wat.c`
`tests/` — mirrors `../c89/`. Originals in `../last1120/` untouched.

## Status / TODO

Int/char/pointer/array programs compile to working wasm (`make watcheck`).
Not done: float/double codegen (last1120 has the keywords but no float
literals), labels nested inside `if`/`while`, function pointers.
