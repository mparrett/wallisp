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
import { loadEngine } from './engine.mjs';

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

// Shared ABI handshake — see harness/engine.mjs, which also carries the INCAP
// guard this file used to implement itself (truncated source would disassemble
// a partial program).
const { run } = await loadEngine(WASM);
try {
  process.stdout.write(run(src));
} catch (e) {
  console.error(`disasm: ${e.message}`);
  process.exit(1);
}
