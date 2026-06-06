# b72 — a B compiler (1972), WAT backend

A front end for Ken Thompson's **B** (the typeless, word-machine ancestor of C;
*Users' Reference to B*, Jan 1972), emitting WebAssembly text. It is **downstream
of `c89-1120`**: B's value model is identical to that 1972 C dialect's, so the
expression engine and the whole backend are reused verbatim and only the
B-specific surface is new.

## What's B-specific vs reused

Reused **byte-identical** from `../c89-1120`:

```
b01.c        expression-tree builder (block/convert)   [= c01.c, + B's *[] rules]
b03.c        type sizes + tree consumer (rcexpr)       [= c03.c]
tables.c     lexer/operator dope tables                [= tables.c]
runtime.c    I/O shims, name packing, errors           [= runtime.c]
wat.c        WebAssembly-text code generator           [= wat.c]
cc.h         shared declarations                       [= cc.h]
```

B-specific (the actual lift):

```
b00.c   lexer + declarations:
          - keyword set: auto extrn goto return if else while switch case
            (no types, no break/continue/do/default)
          - escape char is '*' not '\' ('*n', '*t', '*e', '**', '*(' -> {, ...)
          - strings are EOT('*e', 04) terminated, not NUL
          - vectors declared with a bare size: `auto v 10;` (not `v[10]`)
b01.c   the [] / * rules (see "Word machine" below)
b02.c   statements: `switch rvalue stmt` takes NO parens; switch falls through
          (no break — faithful to pre-ENDCASE BCPL/B)
```

Notably **unchanged and already correct**: old-form assignment operators
(`=+`, `=-`, …) lex in `symbol()`; untyped globals (`name;`, `name N;`,
`name[N] …;`, `name() body`) parse in `extdef()`; the `$pc`-dispatch goto
relooper and switch-segment fallthrough already live in `wat.c`.

## Word machine

B is typeless: every cell is one word, and any word may be used as a value or an
address. This scaffold keeps the byte-addressed wasm model and expresses B's
word semantics with two rules in `b01.c`:

- `*x` dereferences **any** word (no "illegal indirection"); a cell is both
  value and address.
- `x[i]` lowers to `*(x + i*WORD)` — indexing is word-scaled, independent of any
  type. So `auto v 10` (10 words of storage + a reassignable pointer cell),
  `v[i]`, and passing a vector to a function all work.

**Scaffold limitation:** bare pointer arithmetic (`p = p + 1`) steps one *byte*,
not one word — use `v[i]` indexing. A fully word-addressed memory model (B's
true PDP-11 word pointers) is a future step. Also pending: computed `goto rvalue`
(only label `goto name;` is wired, though the `$pc` dispatch in `wat.c` is the
right substrate), the B standard library (`char`, `lchar`, `printf`, `putstr`,
…), and mid-body `auto`/`extrn` (declarations are taken at block head).

## Build & test

```
make                 # native cfront-b
make watcheck        # compile tests/*.b -> wasm, assemble, instantiate, assert
./build-wasm.sh      # emcc -> cfront-b.{mjs,wasm}  (for the museum)
```

`tests/check.js` covers recursion + iterative `while` (`fact`), the goto loop
idiom (`sumto`), switch fallthrough + `=+` (`sw`), word-indexed vectors through
a call (`vec`), and `*n`/EOT string lowering (`hello`).
