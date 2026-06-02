# Example programs

Compile with the wrapper, run with `run.js` (which provides `getchar`/`putchar`
as wasm imports wired to stdin/stdout):

```
../cfront --dialect 1120 upper.c > upper.wat
wat2wasm upper.wat -o upper.wasm
echo "hello world" | node ../run.js upper.wasm     # -> HELLO WORLD
```

I/O model: any function the program leaves undefined (`getchar`, `putchar`, …)
is emitted as an `(import "env" ...)`. The host supplies it. Programs build
higher-level I/O (e.g. `putn` to print a number) from these primitives, exactly
as 1972 C did from the `read`/`write` char primitives.

- `upper.c`  — read stdin, echo upper-cased (getchar/putchar)
- `wc.c`     — count input bytes, print the count (recursive putn)
- `maxsub.c` — Maximum Subarray (Kadane); called with an array in memory
