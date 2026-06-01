# prestruct-c, modernized to C89, compiling to WebAssembly

A runnable port of Dennis Ritchie's 1972 "prestruct-c" compiler
(`../prestruct/c00.c`–`c03.c`) to portable C89, with a **new WebAssembly-text
(WAT) backend**. `cfront` lexes and parses the early-1972 C dialect and emits a
WAT module that runs in any wasm engine. The original PDP-11 code generator is
discarded (see below).

```c
/* tests/max.c — 1972 dialect */
max(a, b) { if (a > b) return(a); return(b); }
```
```
$ ./cfront tests/max.c
(module
  (memory (export "memory") 2)
  (global $sp (mut i32) (i32.const 131072))
  (func $max (export "max") (param i32) (param i32) (result i32)
    ...))
```

## Build & run

```
make            # builds ./cfront
make watcheck   # compile+run the WAT regression suite in node
./cfront f.c    # emit a WAT module to stdout
./cfront -t f.c # dump parse trees instead (debugging)
```

Building `cfront` needs only a C89 compiler and libc. `make watcheck` also needs
`wat2wasm` (wabt) and `node`.

## WAT backend

`wat.c` is the code generator. WASM is a stack machine with structured control,
so the original PDP-11 register/instruction-table machinery collapses:

- **Variables live in linear memory.** Globals/statics get fixed addresses;
  autos/params live in a per-function shadow-stack frame off `$fp` (a real
  C-in-wasm calling convention, prologue copies wasm params into the frame).
  This is what makes `&x`, `*p`, `a[i]` work.
- **Expressions** → `gexpr()` walks the parse tree post-order, emitting wasm
  that leaves the value on the operand stack; `gaddr()` leaves an lvalue
  address. Read-modify-write ops (`=`, `+=`, `++`) use two scratch locals
  (`$a` address, `$t` value) since wasm has no swap/dup.
- **Control flow** is emitted as structured wasm directly from `statement()`
  (the 1972 design is syntax-directed): `if`→`(if(then)(else))`,
  `while`/`do`→`(block(loop …))`, `break`/`continue`→`br`.
- **Sizes** are wasm32 (int/pointer 4, char 1) — 16-bit fidelity dropped.
- **Pointer scaling** (`p+i`, `a[i]`) is emitted as an explicit `i32.mul` by
  element size from `length()`, sidestepping the original opaque conversion
  ops (`convert()` in c01.c was simplified for this).

Verified working (see `tests/`, run via `make watcheck`): functions/params/
return, arithmetic, comparisons, assignment + compound + `++`/`--`, `if`/`else`,
`while`/`do`, recursion and calls, globals (incl. initializers), pointers
(`*p`,`&x`), arrays (`a[i]`), char arrays and `*s++`, **structs** (`.` and `->`),
**float/double** (f64; mixed-type coercion), **`?:`**, **`switch`/`case`/
`default`** (fall-through), **`goto`/labels**, **string literals**, and calls to
external functions (emitted as wasm `(import)`s).

Dialect notes (1972 semantics, intentional): identifiers truncate to 8 chars;
`&`/`|` are *logical* short-circuit operators in a condition (no `&&`/`||`
existed), so `if (1 & 2)` is true; `float` is modeled as f64.

Backend specifics: structs use the global member-name/offset model of the era;
`goto` is compiled to a `$pc`-dispatch loop (labels must be at function top
level); `switch` uses a nested-block `br_if` dispatch; float/struct conversions
are resolved at emit time rather than via the original `cvtab`.

**Still TODO:** function pointers, `unsigned`/`long`, true 32-bit `float`
storage, labels nested inside `if`/`while`.

## What this is

The original is a two-pass compiler:

- **pass 1** (`c00`–`c03`): lexer, expression parser, declaration parser,
  symbol table. Emits parse trees.
- **pass 2** (`c10`/`c11`): PDP-11 code generator.

Pass 2 was dropped on purpose. It matches trees against PDP-11 instruction
tables (`regtab`/`efftab`/`cctab`/`sptab`) that **were never on the prestruct-c
tape** — it cannot be built as shipped. It is replaced by `wat.c` (above), which
consumes the same parse trees and emits WebAssembly.

## How the 1972 code was made to run

The dialect predates K&R C. The substantive changes:

- **Reconstructed data tables.** `ctab` (char classes), `opdope` (operator
  precedence/flags) and `cvtab` (type-conversion matrix) were absent from the
  tape. They are transcribed in `tables.c` from the sibling `last1120/c0t.s`,
  with prestruct deltas (notably `ctab['.'] = 120`, since prestruct lexes `.`
  through `getnum` to tell a member operator from a leading-dot float).
- **`getnum` + I/O runtime** (`runtime.c`): the numeric lexer and the buffered
  `getchar`/`putchar`/`flush` were library routines, also absent. Rewritten
  against stdio.
- **int ≡ pointer.** The compiler stores node pointers in the same word slots
  as small integers, assuming the PDP-11's 16-bit `int == pointer`. A
  pointer-wide cell type `word` (`typedef intptr_t word`, in `cc.h`) is used for
  every cell that may hold a pointer (node arena, symbol table, stacks). Pure
  small-integer scratch stays `int`.
- **`block()` argument walking.** The tree builder read its variadic arguments
  by taking the address of its first parameter and walking memory — undefined
  on AArch64 (arguments arrive in registers). Rewritten with `<stdarg.h>`
  (`c01.c`); every call site casts node children to `word`.
- **The "space" kludge.** `tree()` originally reset the node arena to address 0
  and wrote there (the famous self-overwriting trick). Replaced with a real
  static arena (`osbuf`), reserving index 0 as the null node.
- **Symbol names.** Kept the original 2-chars-per-word packing (so `lookup`'s
  masks and the `0200` keep-flag are bit-identical) but in `word` cells, with
  explicit pack/unpack in `init`/`symbol`/`namestr`.
- **16-bit sentinels.** e.g. `peeksym`'s "no peek" value `0177777` was `-1` on
  16-bit hardware but `65535` (≥ 0) on a 32-bit `int`; corrected to `-1`.
- **Ancient operators** `=+ =- =* …` → `+= -= *= …`, and old typeless global
  definitions (`isn 1;`, `hshtab[900];`) → proper C declarations.

## Files

| File        | Role |
|-------------|------|
| `cc.h`      | `word` typedef, sizes, shared externs, prototypes |
| `tables.c`  | reconstructed `ctab` / `opdope` / `cvtab` |
| `runtime.c` | `getnum`, stdio shims, `error`, name unpacking |
| `c00.c`     | lexer, symbol table, expression parser driver, `main` |
| `c01.c`     | tree-node builder (`block`/`pblock`), typing |
| `c02.c`     | external definitions, statements (structured-wasm control) |
| `c03.c`     | type-size helpers, `rcexpr` dispatch, `-t` tree dumper |
| `wat.c`     | WAT code generator (`gexpr`/`gaddr`, frame, module framing) |
| `tests/`    | `*.c` samples + `check.js` regression suite |

## Status / next

The int/char/pointer/array subset compiles to working wasm end-to-end
(`make watcheck`). Next: float/double, structs (the `cvtab` struct rows are
still placeholder zeros), `switch`, `goto`, and global initializers.
