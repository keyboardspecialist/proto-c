#!/bin/sh
# Build the b72 B compiler to wasm (emscripten), matching the C compilers'
# packaging: an ES6 module whose factory callMain's the compiler over a MEMFS
# source file, printing WAT to stdout. Default output: ./cfront-b.mjs.
# (The museum can point OUTDIR at its built site/compilers.)
set -e
here=$(cd "$(dirname "$0")" && pwd)
out=${OUTDIR:-$here}

WARN="-Wno-implicit-int -Wno-deprecated-non-prototype -Wno-implicit-function-declaration \
-Wno-int-conversion -Wno-return-type -Wno-parentheses -Wno-unused-label"
EMFLAGS="-sMODULARIZE -sEXPORT_ES6 -sEXPORTED_RUNTIME_METHODS=callMain,FS -sINVOKE_RUN=0 \
-sEXIT_RUNTIME=0 -sALLOW_MEMORY_GROWTH=1"

echo "emcc b72 -> cfront-b"
emcc -std=c89 -O2 $WARN \
	"$here/b00.c" "$here/b01.c" "$here/b02.c" "$here/b03.c" \
	"$here/tables.c" "$here/runtime.c" "$here/wat.c" \
	$EMFLAGS -o "$out/cfront-b.mjs"
echo "done -> $out/cfront-b.{mjs,wasm}"
