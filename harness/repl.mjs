#!/usr/bin/env node
// repl.mjs — interactive REPL over the tiny-lisp wasm (Milestone A prototype).
//
//   node harness/repl.mjs
//
// Unlike lisp-cli.mjs (one-shot: every eval_source() re-inits the VM), this
// driver uses the engine's *persistent* session exports so definitions survive
// across lines:
//
//   wallisp> (define x 5)
//   x
//   wallisp> (+ x 1)
//   6
//
// Contract: reset_session() starts (or clears) a session; eval_persistent()
// keeps globals, symbols, the cons arena, and the string heap across calls.
// Do NOT mix eval_source() in on the same instance — it calls init() and wipes
// the session. This REPL only ever calls eval_persistent().
//
// Pure host glue, zero imports — same shape any embedder would use.

import fs from 'fs';
import readline from 'readline';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, '..', 'bytecode_gc.wasm');
const PRELUDE = join(HERE, '..', 'standalone', 'prelude.lisp');
const INCAP = 8192;

async function load() {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(WASM), {}); // {} = no imports
  return instance.exports;
}

function evalLine(ex, src) {
  const data = new TextEncoder().encode(src);
  if (data.length > INCAP) throw new Error(`line too large (${data.length} > ${INCAP} bytes)`);
  new Uint8Array(ex.memory.buffer, ex.input_ptr(), data.length).set(data);
  const n = ex.eval_persistent(data.length);
  return new TextDecoder().decode(new Uint8Array(ex.memory.buffer, ex.output_ptr(), n));
}

// Feed the shared prelude (not, >, >=, <=, length, reverse, fold, append, map,
// filter, assoc) into the session so the REPL starts with the small stdlib
// instead of bare core primitives. It's plain wallisp source — one
// eval_persistent call defines it all. Silently skipped if the file is absent.
function loadPrelude(ex) {
  let src;
  try { src = fs.readFileSync(PRELUDE, 'utf8'); } catch { return false; }
  const out = evalLine(ex, src);
  if (out === '<error>') { console.error('warning: prelude failed to load'); return false; }
  return true;
}

async function main() {
  const ex = await load();
  ex.reset_session();
  const havePrelude = loadPrelude(ex);

  const rl = readline.createInterface({ input: process.stdin, output: process.stdout, prompt: 'wallisp> ' });
  console.error(`wallisp REPL — persistent session${havePrelude ? ' (prelude loaded)' : ''}. Ctrl-D to exit, :reset to clear.`);
  rl.prompt();

  rl.on('line', (line) => {
    const src = line.trim();
    if (src === '') { rl.prompt(); return; }
    if (src === ':reset') { ex.reset_session(); loadPrelude(ex); console.log('; session cleared'); rl.prompt(); return; }
    try {
      console.log(evalLine(ex, src));
    } catch (e) {
      console.error('error:', e.message);
    }
    rl.prompt();
  });

  rl.on('close', () => { console.error('\n; bye'); process.exit(0); });
}

main().catch(e => { console.error('repl:', e.message); process.exit(1); });
