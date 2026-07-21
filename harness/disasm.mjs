// harness/disasm.mjs — drive disasm.wasm against a source file or inline
// expression. Reads the source, hands it to the disasm-variant engine,
// prints the resulting bytecode listing.
//
//   bash harness/disasm.sh                                 # build disasm.wasm
//   node harness/disasm.mjs baselines/metacircular.lisp    # disasm a file
//   node harness/disasm.mjs -e "(+ 1 2)"                   # disasm an expression
//
// The output format mirrors the in-engine `disasm()` block:
//   ADDR: OPNAME    decoded-operand
//   ...
//   ; --- bytecode total: N words ---

import fs from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, '..', 'disasm.wasm');

if (!fs.existsSync(WASM)) {
  console.error(`error: ${WASM} not found. Run \`bash harness/disasm.sh\` first.`);
  process.exit(1);
}

const args = process.argv.slice(2);
let src;
if (args[0] === '-e') {
  src = args.slice(1).join(' ');
} else if (args.length === 1) {
  src = fs.readFileSync(args[0], 'utf8');
} else {
  console.error('usage: node harness/disasm.mjs <file>   |   -e "<expr>"');
  process.exit(2);
}

const { instance } = await WebAssembly.instantiate(fs.readFileSync(WASM), {});
const ex = instance.exports, mem = ex.memory;
const e = new TextEncoder().encode(src);
if (e.length > 8192) { console.error(`source too large (${e.length} > 8192 bytes) — the engine truncates at 8 KB (INCAP), which would disassemble a partial program`); process.exit(1); }
new Uint8Array(mem.buffer, ex.input_ptr(), e.length).set(e);
const n = ex.eval_source(e.length);
process.stdout.write(new TextDecoder().decode(new Uint8Array(mem.buffer, ex.output_ptr(), n)));
