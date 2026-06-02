/*
 * run.js - host runtime for compiled 1972-C programs.
 *
 *   echo input | node run.js prog.wasm [entry]
 *
 * Provides the era's char I/O primitives as wasm imports: getchar() reads the
 * next byte of stdin (0 at EOF), putchar(c) writes a byte to stdout. The
 * compiler emits these as `(import "env" ...)` automatically for any function
 * the program leaves undefined. Calls the `entry` export (default main) and
 * exits with its return value.
 */
const fs = require('fs');

const wasmPath = process.argv[2];
const entry = process.argv[3] || 'main';

let stdin = Buffer.alloc(0);
try { stdin = fs.readFileSync(0); } catch (e) { /* no stdin */ }
let inpos = 0;

const out = [];
const flush = () => { if (out.length) { process.stdout.write(Buffer.from(out)); out.length = 0; } };

const imports = {
	env: {
		getchar: () => (inpos < stdin.length ? stdin[inpos++] : 0),
		putchar: (c) => { out.push(c & 0xff); if (out.length > 4096) flush(); return c & 0xff; },
		/* a couple more primitives programs might reach for */
		putn: (n) => { String(n | 0).split('').forEach(ch => out.push(ch.charCodeAt(0))); return n; },
		exit: (code) => { flush(); process.exit(code | 0); },
	},
};

const inst = new WebAssembly.Instance(
	new WebAssembly.Module(fs.readFileSync(wasmPath)), imports);

let rc = 0;
if (typeof inst.exports[entry] === 'function')
	rc = inst.exports[entry]() | 0;
else
	process.stderr.write(`run.js: no export '${entry}'\n`), rc = 1;

flush();
process.exit(rc & 0xff);
