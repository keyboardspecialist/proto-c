/*
 * WAT backend regression suite for the b72 B compiler. Compiles each
 * tests/<name>.b with cfront-b, assembles, instantiates, asserts. Exit 1 on
 * any failure. `make watcheck`.
 */
const fs = require('fs');
const cp = require('child_process');
const path = require('path');

const DIR = __dirname;
const CFRONT = path.join(DIR, '..', 'cfront-b');
const WAT2WASM = process.env.WAT2WASM ||
	`${process.env.HOME}/tools/wabt-1.0.36/bin/wat2wasm`;

let fails = 0;
function ok(cond, msg) {
	console.log(`${cond ? 'ok  ' : 'FAIL'}  ${msg}`);
	if (!cond) fails++;
}
function build(name) {
	const wat = path.join(DIR, `${name}.wat`);
	const wasm = path.join(DIR, `${name}.wasm`);
	fs.writeFileSync(wat, cp.execSync(`${CFRONT} ${path.join(DIR, name + '.b')}`));
	cp.execSync(`${WAT2WASM} ${wat} -o ${wasm}`);
	return new WebAssembly.Instance(
		new WebAssembly.Module(fs.readFileSync(wasm)), {}).exports;
}

// recursion + iterative factorial
let e = build('fact');
[[0,1],[1,1],[5,120],[10,3628800]].forEach(([n,r]) => {
	ok(e.fact(n) === r, `fact(${n})=${r}`);
	ok(e.ifact(n) === r, `ifact(${n})=${r}`);
});

// labels + goto loop
e = build('sumto');
ok([0,1,5,10].every((n,i) => e.sumto(n) === [0,1,15,55][i]), 'goto loop sumto');

// switch fallthrough (no break) + old-form '=+'
e = build('sw');
ok([1,2,3,9].every((n,i) => e.classify(n) === [3,2,1,0][i]),
	'switch fallthrough (B: no break) + =+');

// B vectors: bare-size auto decl, indexing, pointer decay through a call
e = build('vec');
ok(e.triangle() === 10, 'auto a 4; a[i]=i+1; sum(a,4)=10');

// string literal: '*n' escape, EOT(04) terminator
e = build('hello');
const m = new Uint8Array(e.memory.buffer);
let addr = e.greeting() >>> 0, s = '';
for (let i = addr; m[i] !== 4 && i < addr + 64; i++) s += String.fromCharCode(m[i]);
ok(s === 'Hi!\n', `string "Hi!*n" EOT-terminated -> ${JSON.stringify(s)}`);

console.log(fails ? `\n${fails} FAILED` : '\nall passed');
process.exit(fails ? 1 : 0);
