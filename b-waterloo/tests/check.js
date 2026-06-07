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

// full switch: single value, range, relational bound, fallthrough, default
e = build('sw');
ok([32, 53, 120, 90, 10, 200, 33].map(c => String.fromCharCode(e.kind(c))).join("") === "SDLLCHP",
	'switch single/range/relational-bound/fallthrough/default : kind');

// dotted names (Waterloo allows '.' in identifiers)
e = build('dot');
ok(e["sq.it"](6) === 36, "dotted names: sq.it(6)=36 with local my.val");

// f32 floats via #-operators
e = build('float');
ok(near(asF32(e.fadd()), 3.75), '#+ : 1.5 #+ 2.25 = 3.75');
ok(e.fcmp() === 1, '#< : 3.14 #< 3.15');
ok(near(asF32(e.favg(f32bits(3), f32bits(5))), 4.0), '#+ / #/ : favg(3,5)=4.0');

// string library: char/lchar provided as host imports (the museum's BWRuntime).
// B pointers are word indices, so the byte address of word-pointer w is w*4.
function buildLib(name) {
	const wat = path.join(DIR, `${name}.wat`);
	const wasm = path.join(DIR, `${name}.wasm`);
	fs.writeFileSync(wat, cp.execSync(`${CFRONT} ${path.join(DIR, name + '.b78')}`));
	cp.execSync(`${WAT2WASM} ${wat} -o ${wasm}`);
	let inst = null;
	const mem = () => new Uint8Array(inst.exports.memory.buffer);
	const env = {
		char: (s, i) => mem()[(s >>> 0) * 4 + (i | 0)],
		lchar: (s, i, c) => { mem()[(s >>> 0) * 4 + (i | 0)] = c & 0xff; return c & 0xff; },
	};
	inst = new WebAssembly.Instance(
		new WebAssembly.Module(fs.readFileSync(wasm)), { env });
	return inst.exports;
}
e = buildLib('lib');
const M = new Uint8Array(e.memory.buffer);
const putS = (w, str) => { let a = w * 4; for (const ch of str) M[a++] = ch.charCodeAt(0); M[a] = 0; };
const getS = (w) => { let a = w * 4, s = ''; while (M[a]) s += String.fromCharCode(M[a++]); return s; };
putS(100, 'hello');
ok(e.slen(100) === 5, 'slen("hello")=5 via char()');
e.upper(100);
ok(getS(100) === 'HELLO', 'upper() in place via char()/lchar() -> HELLO');

// printf: compiler marshals varargs to a buffer; host walks the format
function buildIO(name) {
	const wat = path.join(DIR, `${name}.wat`);
	const wasm = path.join(DIR, `${name}.wasm`);
	fs.writeFileSync(wat, cp.execSync(`${CFRONT} ${path.join(DIR, name + '.b78')}`));
	cp.execSync(`${WAT2WASM} ${wat} -o ${wasm}`);
	let inst = null, out = '';
	const m = () => new Uint8Array(inst.exports.memory.buffer);
	const rdstr = (wi) => { const mm = m(); let a = (wi >>> 0) * 4, s = ''; while (mm[a]) s += String.fromCharCode(mm[a++]); return s; };
	const rdword = (wi) => { const mm = m(); const a = (wi >>> 0) * 4; return mm[a] | (mm[a + 1] << 8) | (mm[a + 2] << 16) | (mm[a + 3] << 24); };
	const env = {
		putchar: (c) => { out += String.fromCharCode(c & 0xff); return c & 0xff; },
		printf: (fmt, argbuf, argc) => {
			const mm = m(); let a = (fmt >>> 0) * 4, ai = 0;
			const nx = () => rdword((argbuf >>> 0) + ai++);
			while (mm[a] !== 0) {
				const c = mm[a++];
				if (c === 37) { const f = mm[a++];
					if (f === 100) out += String(nx() | 0);
					else if (f === 99) out += String.fromCharCode(nx() & 0xff);
					else if (f === 111) out += (nx() >>> 0).toString(8);
					else if (f === 115) out += rdstr(nx());
					else out += String.fromCharCode(f);
				} else out += String.fromCharCode(c);
			}
			return 0;
		},
	};
	inst = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(wasm)), { env });
	inst.exports.main();
	return out;
}
ok(buildIO('printf') === 'n=42 c=X s=hi o=100\n', 'printf %d/%c/%s/%o -> n=42 c=X s=hi o=100');

console.log(fails ? `\n${fails} FAILED` : '\nall passed');
process.exit(fails ? 1 : 0);
