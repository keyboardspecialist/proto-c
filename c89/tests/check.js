/*
 * WAT backend regression suite. Each entry compiles tests/<name>.c with cfront,
 * assembles with wat2wasm, instantiates, and asserts behavior. Exit 1 on any
 * failure. Run via `make watcheck`.
 */
const fs = require('fs');
const cp = require('child_process');
const path = require('path');

const DIR = __dirname;
const CFRONT = path.join(DIR, '..', 'cfront');
const WAT2WASM = process.env.WAT2WASM ||
	`${process.env.HOME}/tools/wabt-1.0.36/bin/wat2wasm`;

let fails = 0;
function ok(cond, msg) {
	console.log(`${cond ? 'ok  ' : 'FAIL'}  ${msg}`);
	if (!cond) fails++;
}

function build(name) {
	const c = path.join(DIR, `${name}.c`);
	const wat = path.join(DIR, `${name}.wat`);
	const wasm = path.join(DIR, `${name}.wasm`);
	const out = cp.execSync(`${CFRONT} ${c}`);
	fs.writeFileSync(wat, out);
	cp.execSync(`${WAT2WASM} ${wat} -o ${wasm}`);
	const inst = new WebAssembly.Instance(
		new WebAssembly.Module(fs.readFileSync(wasm)), {});
	return inst;
}

// max: arithmetic, if/else, return
let e = build('max').exports;
ok(e.max(7, 3) === 7, 'max(7,3)=7');
ok(e.max(2, 9) === 9, 'max(2,9)=9');
ok(e.max(-5, -2) === -2, 'max(-5,-2)=-2');

// fact: recursion, while, calls
e = build('fact').exports;
[[0, 1], [1, 1], [5, 120], [10, 3628800]].forEach(([n, r]) => {
	ok(e.fact(n) === r, `fact(${n})=${r}`);
	ok(e.ifact(n) === r, `ifact(${n})=${r}`);
});

// ptr: *p, &, a[i] over linear memory
e = build('ptr').exports;
let m = new Int32Array(e.memory.buffer);
m[100] = 5; m[200] = 9;
e.swap(400, 800);
ok(m[100] === 9 && m[200] === 5, 'swap via *p/*q');
[10, 20, 30, 40].forEach((v, i) => m[256 + i] = v);
ok(e.sum(1024, 4) === 100, 'sum a[i]=100');

// str: *s++ over chars, char loads, globals
e = build('str').exports;
let b = new Uint8Array(e.memory.buffer);
const s = 'hello, world', base = 2048;
for (let i = 0; i < s.length; i++) b[base + i] = s.charCodeAt(i);
b[base + s.length] = 0;
ok(e.strlen(base) === s.length, `strlen=${s.length}`);
ok(e.bump() === 1 && e.bump() === 2 && e.bump() === 3, 'global counter 1,2,3');

// struct: . and -> , struct pointers, member offsets
e = build('struct').exports;
ok(e.g() === 12, 'struct p.x+p.y=12');
m = new Int32Array(e.memory.buffer);
e.setp(1024, 3, 4);
ok(m[256] === 3 && m[257] === 4, 'p->x=3 p->y=4 (offsets)');
e.setp(1040, 5, 6);
ok(e.dot(1024, 1040) === 39, 'dot via -> = 39');

// float/double: arith, coercion, compare, ternary
e = build('flt').exports;
ok(e.favg(3, 4) === 3.5, 'favg(3,4)=3.5');
ok(e.mix() === 4.5, 'int->float mix=4.5');
ok(e.big(3) === 1 && e.big(1) === 0, 'float compare big()');

// control: switch fallthrough/default, goto loop, logical &, ternary
e = build('ctl').exports;
ok([1,2,3,5].every((n,i) => e.classify(n) === [10,23,23,99][i]), 'switch fallthrough+default');
ok([0,1,5,10].every((n,i) => e.sumto(n) === [0,0,10,45][i]), 'goto loop sumto');
ok(e.land(1,2) === 1 && e.land(1,0) === 0, 'logical & (1972 semantics)');
ok(e.absx(-5) === 5 && e.absx(7) === 7, 'ternary absx');

console.log(fails ? `\n${fails} FAILED` : '\nall passed');
process.exit(fails ? 1 : 0);
