# b-waterloo — Waterloo B (1978), WAT backend

A front end for **Waterloo B** — the richer, later strand of Thompson's B, as
documented in *User's Reference to B for Honeywell Series 6000/66* (R.P. Gurd,
University of Waterloo, Sept 1978; compiler by R. Braga) and its descendant
GCOS8 B. Forked from [`../b72`](../b72) (the 1972 PDP-11 dialect); the WAT
backend and expression engine are shared, the front end carries the Waterloo
deltas.

## What Waterloo B adds over 1972 B

Per the 1978 manual, the dialect differs from Thompson's original by an
"expanded SWITCH statement, floating point operators, proper logical operators,
and altered order of evaluation." Implemented here:

| Area | b72 (1972) | b-waterloo (1978) |
|---|---|---|
| Loops | `while`, `goto` | + `for (e1;e2;e3)`, `do … while`, `repeat` |
| Loop exit | — | `break`, `next` (B's `continue`) |
| Switch | fallthrough, no `break`/`default` | `break`, `default`, **range cases** `case lo :: hi :` (still fallthrough by default) |
| Assignment | old `=+` | modern `+= -= *= /= %= &= |= ^=` |
| Logic | `&`/`|` truthy in bool ctx | real short-circuit `&&` / `||` (`&`/`|` are bitwise) |
| Floats | none | f32 via `#`-operators: `#+ #- #* #/  #== #!= #< #<= #> #>=` |
| Strings | `*e` (EOT) terminated | NUL (000) terminated |

Escape char stays `*` (the 1978 manual still uses `*n`, `*0`; backslash is a
later GCOS8 alias). Vectors keep the bare-size form (`auto v 10`).

## Floats on a typeless 4-byte word

B is typeless: every cell is one word. A C-style `float` (f64, 8 bytes) won't fit
a 4-byte word without breaking that, so a Waterloo float here is **f32 stored in
the word as its bit pattern**. There is no float *type* — the `#`-prefixed
operator is what says "interpret these two words as f32": each `#`-op
`f32.reinterpret_i32`s its operands, computes, and (for arithmetic)
`i32.reinterpret_f32`s the result back into a word. A float literal (`3.14`,
`1.5e3`) lexes straight to the i32 word holding its f32 bits.

## Build & test

```
make                 # native cfront-bw
make watcheck        # compile tests/*.b78 -> wasm, assemble, instantiate, assert
./build-wasm.sh      # emcc -> cfront-bw.{mjs,wasm}  (for the museum)
```

`tests/check.js` covers `for`/`+=`, `repeat`/`break`, `next`, `do-while`,
`&&`/`||`, range-case `switch` with `default`+`break`, and the f32 `#`-operators.

## String library (host-provided)

The B string primitives are supplied by the host as memory-aware imports rather
than compiled — the compiler already lowers an undeclared `name(...)` to an
`env` import, so a program just `extrn`s and calls them. B pointers are word
indices, so the byte address of word-pointer `w` is `w*4`; strings are
NUL-terminated, one char per byte:

- `char(s, i)` — the i-th character of the string at `s`
- `lchar(s, i, c)` — store `c` as the i-th character; returns `c`
- `putstr(s)` — write the NUL-terminated string at `s`
- `getstr(s)` — read one input line into the buffer at `s`

The museum's `BWRuntime` (`lang-bw.mjs`) implements them; `tests/check.js`
exercises `char`/`lchar` natively (`slen`, in-place `upper`).

## Pending

`printf` needs varargs, which the compiler does not yet support (each call site
fixes an import's arity). `%file` inclusion and `#`-directives, manifest
constants, BCD constants (`` `…` `` / `$'…'`), and unit-based file I/O are also
unimplemented. Negative float *literals* (`-3.14`) negate the bit pattern rather
than the value — use `0.0 #- x`.
