#!/usr/bin/env node
// cli.mjs — drive wallisp.wasm from the command line.
//
//   node cli.mjs program.lisp        # eval a file
//   node cli.mjs -e "(+ 1 2)"        # eval an inline expression
//   echo "(* 6 7)" | node cli.mjs    # eval from stdin
//
// Pure host glue: load the module (no imports), write source into its linear
// memory, call eval_source(len), read the result string back out.

import fs from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, 'wallisp.wasm');

async function load() {
  const bytes = fs.readFileSync(WASM);
  const { instance } = await WebAssembly.instantiate(bytes, {});
  return instance.exports;
}

function evalSource(ex, src) {
  const mem = ex.memory;
  const data = new TextEncoder().encode(src);
  if (data.length > 8192) throw new Error(`source too large (${data.length} > 8192 bytes)`);
  new Uint8Array(mem.buffer, ex.input_ptr(), data.length).set(data);
  const n = ex.eval_source(data.length);
  return new TextDecoder().decode(new Uint8Array(mem.buffer, ex.output_ptr(), n));
}

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
    if (!src) { console.error('usage: cli.mjs -e "<expr>"'); process.exit(2); }
  } else if (argv[0] && argv[0] !== '-') {
    src = fs.readFileSync(argv[0], 'utf8');
  } else if (!process.stdin.isTTY) {
    src = await readStdin();
  } else {
    console.error('usage: cli.mjs [file.lisp | -e "<expr>"]   (or pipe source on stdin)');
    process.exit(2);
  }

  const ex = await load();
  const result = evalSource(ex, src);
  console.log(result);
  if (result === '<error>') process.exit(1);
}

main().catch(e => { console.error('cli:', e.message); process.exit(1); });
