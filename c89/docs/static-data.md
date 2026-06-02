# Example: static / global data → WebAssembly

How `cfront` lays out initialized globals, string literals, and function-local
statics in the wasm linear memory, with a runnable program.

## Source (1972 dialect)

```c
/* initialized global array */
int dmon[12] 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31;
days(m) { return(dmon[m]); }

/* pointer to a string literal in static data */
char *greet "hi!";
initial() { return(*greet); }

/* function-local static: persists across calls */
nextid() {
	static n;
	n = n + 1;
	return(n);
}
```

Dialect notes:
- Initializers follow the declarator with no `=`: `int dmon[12] 31, 28, …;`.
- Storage class is its own declarator (no `=`, type implicit-int): write
  `static n;` — **not** `static int n;` (the lexer takes `static` and `int` as
  two separate declarations, which is a syntax error here).
- `char *greet "hi!";` is a pointer initialized to a string literal.

## Compile

```
./cfront static-data.c > out.wat
wat2wasm out.wat -o out.wasm
```

## What gets emitted

Every variable lives in linear memory. The compiler hands out addresses from a
low watermark (16); the shadow stack for locals grows down from the top
(`$sp` = 131072). Initialized data and string literals become `(data)`
segments:

```wat
(global $sp (mut i32) (i32.const 131072))
;; dmon[12] — twelve little-endian i32s (0x1f = 31, 0x1c = 28, 0x1e = 30 …)
(data (i32.const 16) "\1f\00\00\00\1c\00\00\00\1f\00\00\00\1e\00\00\00…")
;; the string "hi!\0"
(data (i32.const 64) "\68\69\21\00")
;; greet — a pointer holding 0x40 = 64, the address of that string
(data (i32.const 68) "\40\00\00\00")
```

### Memory map

| Address | Bytes | Holds |
|--------:|------:|-------|
| 16      | 48    | `dmon[12]` (12 × i32) |
| 64      | 4     | the string `"hi!\0"` |
| 68      | 4     | `greet` = `64` (points at the string) |
| 72…     |       | `nextid`'s static `n` (zeroed; no `(data)` since uninitialized) |
| ↓131072 |       | shadow-stack frames for autos/params |

Accessor codegen falls straight out of the addressing model. `dmon[m]` is
`*(dmon + m)` with index scaling, so `days` loads from `16 + m*4`:

```wat
(func $days (export "days") (param i32) (result i32)
  …
  i32.const 16              ;; &dmon
  local.get $fp i32.const 4 i32.add i32.load  ;; m
  i32.const 4 i32.mul i32.add                  ;; + m*4
  i32.load …)
```

`initial` dereferences `greet`: load the pointer at 68, then load the byte it
points to (`'h'` = 104). `nextid`'s `static n` keeps its address across calls,
so it counts up.

## Run

```js
const e = new WebAssembly.Instance(new WebAssembly.Module(
	require('fs').readFileSync('out.wasm')), {}).exports;

e.days(0);  e.days(1);  e.days(11);   // 31  28  31
e.initial();                          // 104  ('h')
e.nextid(); e.nextid(); e.nextid();   // 1  2  3
```

Initialized globals read back through `days`; the string pointer chases through
two memory loads; the static local accumulates — all backed by the `(data)`
segments and the zero-initialized memory above the watermark.
```
```
