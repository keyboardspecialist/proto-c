/*
 * WAT backend regression suite for the last1120 compiler. Compiles each
 * tests/<name>.c with cfront, assembles, instantiates, asserts. Exit 1 on
 * any failure. `make watcheck`.
 */
const fs = require('fs');
const cp = require('child_process');
const path = require('path');

const DIR = __dirname;
const CFRONT = path.join(DIR, '..', 'cfront-1120');
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
	fs.writeFileSync(wat, cp.execSync(`${CFRONT} ${path.join(DIR, name + '.c')}`));
	cp.execSync(`${WAT2WASM} ${wat} -o ${wasm}`);
	return new WebAssembly.Instance(
		new WebAssembly.Module(fs.readFileSync(wasm)), {}).exports;
}

let e = build('max');
ok(e.max(7, 3) === 7 && e.max(2, 9) === 9 && e.max(-5, -2) === -2, 'max');

e = build('fact');
[[0,1],[1,1],[5,120],[10,3628800]].forEach(([n,r]) => {
	ok(e.fact(n) === r, `fact(${n})=${r}`);
	ok(e.ifact(n) === r, `ifact(${n})=${r}`);
});

// pointers via the auto x[] reassignable-pointer idiom (prestruct can't parse this)
e = build('ptr');
let m = new Int32Array(e.memory.buffer);
m[100] = 5; m[200] = 9;
e.swap(400, 800);
ok(m[100] === 9 && m[200] === 5, 'swap via *a/*b');
[10,20,30,40].forEach((v,i) => m[256+i] = v);
ok(e.sum(1024, 4) === 100, 'sum a[i]=100');
[0,0,7,0].forEach((v,i) => m[300+i] = v);   // first non-zero is 7
ok(e.firstne(1200, 4) === 7, 'firstne via reassignable p[]');

// control: switch, goto loop, logical &
e = build('ctl');
ok([1,2,3,5].every((n,i) => e.classify(n) === [10,23,23,99][i]), 'switch fallthrough+default');
ok([0,1,5,10].every((n,i) => e.sumto(n) === [0,0,10,45][i]), 'goto loop sumto');
ok(e.land(1,2) === 1 && e.land(1,0) === 0, 'logical & (1972 semantics)');

// untyped globals: scalar + array (pointer-cell model) + extern access
// B/BCPL semantics: an array name is a reassignable pointer cell + storage,
// auto-initialized to point at the storage. Reassigning it (z = z+2, z = save)
// is illegal under prestruct/c89's C-style decay but works here.
e = build('bsem');
ok(e.bsem() === 400, 'B-style reassignable array pointer (bsem=400)');

e = build('data');
ok(e.getn() === 42, 'global n=42');
ok([0,1,2].every(i => e.getv(i) === [10,20,30][i]), 'global v[] via extern v[]');
ok(e.bump() === 43 && e.bump() === 44, 'global mutate via extern');

// regression: a switch case body over 1KB once truncated in cg() (fixed tmp[]).
e = build('swbig');
ok(e.swbig(0) === 820 && e.swbig(5) === 0, 'big switch case (>1KB segment) not truncated');

// regression: goto exiting a switch, with a >1KB case body (the Duff attempt bug).
e = build('gotosw');
ok([0,1,2,9].every((n,i) => e.gotosw(n) === [465,2,103,100][i]), 'goto out of switch + big case body');

// a label INSIDE a switch is rejected (switch re-entry is unrepresentable here).
// cfront exits 0 even on error, so capture stderr (1>/dev/null keeps only it).
const labErr = cp.execSync(
	`${CFRONT} ${path.join(DIR, 'labinsw.c')} 2>&1 1>/dev/null || true`, { encoding: 'utf8' });
ok(/label inside switch/.test(labErr), 'label inside switch is diagnosed, not miscompiled');

// a case nested in a loop within a switch is rejected (Duff do-while form).
const loopErr = cp.execSync(
	`${CFRONT} ${path.join(DIR, 'caseinloop.c')} 2>&1 1>/dev/null || true`, { encoding: 'utf8' });
ok(/case inside a loop/.test(loopErr), 'case inside a loop in switch is diagnosed');

// printf: the compiler marshals varargs to a buffer of 8-byte slots; the host
// walks the format, reading an i32 per conversion (this dialect has no float).
// Pointers are byte addresses here (unlike B's word indices).
function buildIO(name) {
	const c = path.join(DIR, `${name}.c`);
	const wat = path.join(DIR, `${name}.wat`);
	const wasm = path.join(DIR, `${name}.wasm`);
	fs.writeFileSync(wat, cp.execSync(`${CFRONT} ${c}`));
	cp.execSync(`${WAT2WASM} ${wat} -o ${wasm}`);
	let inst = null, out = '';
	const m = () => new Uint8Array(inst.exports.memory.buffer);
	const dv = () => new DataView(inst.exports.memory.buffer);
	const rdstr = (a) => { const mm = m(); a >>>= 0; let s = ''; while (mm[a]) s += String.fromCharCode(mm[a++]); return s; };
	const env = {
		printf: (fmt, argbuf, argc) => {
			const mm = m(), v = dv(); let a = fmt >>> 0, sl = argbuf >>> 0;
			const i32 = () => { const r = v.getInt32(sl, true); sl += 8; return r; };
			while (mm[a]) { const ch = mm[a++];
				if (ch === 37) { const f = mm[a++];
					if (f === 100) out += String(i32() | 0);
					else if (f === 99) out += String.fromCharCode(i32() & 0xff);
					else if (f === 115) out += rdstr(i32());
					else if (f === 120) out += (i32() >>> 0).toString(16);
					else out += String.fromCharCode(f);
				} else out += String.fromCharCode(ch);
			}
			return 0;
		},
	};
	inst = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(wasm)), { env });
	inst.exports.main();
	return out;
}
ok(buildIO('printf') === 'n=42 c=X s=hi x=ff\n',
	'printf %d/%c/%s/%x marshalled (i32 slots)');

console.log(fails ? `\n${fails} FAILED` : '\nall passed');
process.exit(fails ? 1 : 0);
