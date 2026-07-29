#!/usr/bin/env node
// lisp-cli.mjs — run the tiny-lisp wasm from the command line.
//
//   node lisp-cli.mjs program.lisp        # eval a file
//   node lisp-cli.mjs -e "(+ 1 2)"         # eval an inline expression
//   echo "(* 6 7)" | node lisp-cli.mjs     # eval from stdin
//
// Pure host glue: load the module (no imports), write source into its linear
// memory, call eval_source(len), read the result string back out. Same four
// exports a 20-line program in any language/runtime would use.

import fs from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { loadEngine } from './engine.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, '..', 'bytecode_gc.wasm');

// Loading and the ABI handshake (including the INCAP overflow guard that used
// to live here) are shared — see harness/engine.mjs.

async function readStdin() {
  const chunks = [];
  for await (const c of process.stdin) chunks.push(c);
  return Buffer.concat(chunks).toString('utf8');
}

async function main() {
  const argv = process.argv.slice(2);
  let src;
  if (argv[0] === '-e') {
    src = argv.slice(1).join(' ');
    if (!src) { console.error('usage: lisp-cli.mjs -e "<expr>"'); process.exit(2); }
  } else if (argv[0] && argv[0] !== '-') {
    src = fs.readFileSync(argv[0], 'utf8');
  } else if (!process.stdin.isTTY) {
    src = await readStdin();
  } else {
    console.error('usage: lisp-cli.mjs [file.lisp | -e "<expr>"]   (or pipe source on stdin)');
    process.exit(2);
  }

  const { run } = await loadEngine(WASM);
  const result = run(src);
  console.log(result);
  // surface evaluation errors as a nonzero exit so it composes in shell pipelines
  if (result === '<error>') process.exit(1);
}

main().catch(e => { console.error('lisp-cli:', e.message); process.exit(1); });
