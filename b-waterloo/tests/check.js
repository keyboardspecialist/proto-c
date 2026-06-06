/*
 * WAT backend regression suite for the b-waterloo (Waterloo B) compiler.
 * Compiles each tests/<name>.b78 with cfront-bw, assembles, instantiates,
 * asserts. Exit 1 on any failure. `make watcheck`.
 */
const fs = require('fs');
const cp = require('child_process');
const path = require('path');

const DIR = __dirname;
const CFRONT = path.join(DIR, '..', 'cfront-bw');
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
	fs.writeFileSync(wat, cp.execSync(`${CFRONT} ${path.join(DIR, name + '.b78')}`));
	cp.execSync(`${WAT2WASM} ${wat} -o ${wasm}`);
	return new WebAssembly.Instance(
		new WebAssembly.Module(fs.readFileSync(wasm)), {}).exports;
}
// reinterpret between a word and an f32 (Waterloo floats are f32 bits in a word)
const _b = new ArrayBuffer(4), _i = new Int32Array(_b), _f = new Float32Array(_b);
const asF32 = (i) => { _i[0] = i | 0; return _f[0]; };
const f32bits = (x) => { _f[0] = x; return _i[0]; };
const near = (a, b) => Math.abs(a - b) < 1e-5;

// for loop + +=
let e = build('forloop');
ok([1, 5, 10].every((n, i) => e.sumfor(n) === [1, 15, 55][i]), 'for + += : sumfor');

// repeat/break, for/next, do-while
e = build('ctl');
ok(e.firstmul(10, 7) === 14, 'repeat + break : firstmul(10,7)=14');
ok(e.countodd(10) === 5, 'for + next(continue) : countodd(10)=5');
ok(e.dowhile(5) === 15, 'do-while : dowhile(5)=15');

// && / || and range-case switch with default + break
e = build('logic');
ok(e.band(1, 0) === 0 && e.band(2, 3) === 1, '&& short-circuit');
ok(e.bor(0, 0) === 0 && e.bor(0, 5) === 1, '|| short-circuit');
ok([50, 70, 95, 200].every((n, i) => e.grade(n) === [0, 1, 2, 9][i]),
	'switch range cases + default + break : grade');

// f32 floats via #-operators
e = build('float');
ok(near(asF32(e.fadd()), 3.75), '#+ : 1.5 #+ 2.25 = 3.75');
ok(e.fcmp() === 1, '#< : 3.14 #< 3.15');
ok(near(asF32(e.favg(f32bits(3), f32bits(5))), 4.0), '#+ / #/ : favg(3,5)=4.0');

console.log(fails ? `\n${fails} FAILED` : '\nall passed');
process.exit(fails ? 1 : 0);
