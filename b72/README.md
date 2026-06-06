# b72 — a B compiler (1972), WAT backend

A front end for Ken Thompson's **B** (the typeless, word-machine ancestor of C;
*Users' Reference to B*, Jan 1972), emitting WebAssembly text. It is **downstream
of `c89-1120`**: B's value model is identical to that 1972 C dialect's, so the
expression engine and the whole backend are reused verbatim and only the
B-specific surface is new.

## What's B-specific vs reused

Reused **byte-identical** from `../c89-1120`:

```
b03.c        type sizes + tree consumer (rcexpr)       [= c03.c]
tables.c     lexer/operator dope tables                [= tables.c]
runtime.c    I/O shims, name packing, errors           [= runtime.c]
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
b01.c   = c01.c, with B's typeless `*` (any word is an address) and `a[i]`
          lowering to plain *(a+i) (no scaling — see "Word machine")
b02.c   = c02.c, with `switch rvalue stmt` (no parens), switch fallthrough
          (no break — faithful to pre-ENDCASE BCPL/B), and word-index globals
wat.c   = wat.c, with the word-index pointer representation (the <<2/>>2 at
          deref/address-of, and word-index vector-cell init)
```

Notably **unchanged and already correct**: old-form assignment operators
(`=+`, `=-`, …) lex in `symbol()`; untyped globals (`name;`, `name N;`,
`name[N] …;`, `name() body`) parse in `extdef()`; the `$pc`-dispatch goto
relooper and switch-segment fallthrough already live in `wat.c`.

## Word machine

B is typeless: every cell is one word, and any word may be used as a value or an
address. A B **pointer is a word index** (not a byte address), so on the PDP-11
`p + 1` stepped one word with no scaling. b72 keeps that representation on
byte-addressed wasm linear memory:

- a pointer value is `byteAddress >> 2`; `&x` shifts right 2, `*p` and `p[i]`
  shift left 2 just before the `i32.load`/`i32.store`;
- arithmetic never scales — `p + 1`, `p = p + 2`, `++p`, `p[i] = *(p+i)` all step
  in whole words, the B/BCPL reassignable-pointer idiom;
- `*x` dereferences **any** word (no "illegal indirection"): a cell is both value
  and address. String rvalues and vector-cell initializers are word indices too.

So `auto v 10` (10 words of storage + a reassignable pointer cell), `v[i]`,
`v = v + k`, and passing a vector to a function all behave as in B.

**Pending** (not blocking the above): computed `goto rvalue` (only label
`goto name;` is wired, though the `$pc` dispatch in `wat.c` is the right
substrate), the B standard library (`char`, `lchar`, `printf`, `putstr`, …),
and mid-body `auto`/`extrn` (declarations are taken at block head).

## Build & test

```
make                 # native cfront-b
make watcheck        # compile tests/*.b -> wasm, assemble, instantiate, assert
./build-wasm.sh      # emcc -> cfront-b.{mjs,wasm}  (for the museum)
```

`tests/check.js` covers recursion + iterative `while` (`fact`), the goto loop
idiom (`sumto`), switch fallthrough + `=+` (`sw`), word-indexed vectors through
a call (`vec`), and `*n`/EOT string lowering (`hello`).
