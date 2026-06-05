// bench_preproc.mjs — measure the source-to-source PE win from $-folding.
//
// Runs the same demo program through bytecode_gc.wasm in two forms:
//   raw     — the program with `$(sum-to 100)` left in source
//   folded  — preprocessed: `(work x)` body is `(+ x 5050)`
// Both should agree (the result is purely mathematical), and the folded
// version should be measurably faster — the engine no longer pays for
// 100 recursive sum-to calls per work invocation.

import fs from 'fs';
import { spawnSync } from 'child_process';
import { fileURLToPath } from 'url';
import path from 'path';

const ROOT = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const DEMO_MARKED   = path.join(ROOT, 'prototype/futamura/comptime_demo.lisp');
const DEMO_UNMARKED = path.join(ROOT, 'prototype/futamura/comptime_demo_unmarked.lisp');

// Build preproc if missing.
const PREPROC = path.join(ROOT, 'prototype/futamura/build/preproc');
if (!fs.existsSync(PREPROC)) {
  fs.mkdirSync(path.dirname(PREPROC), { recursive: true });
  const r = spawnSync('cc', ['-O2', '-o', PREPROC,
                             path.join(ROOT, 'prototype/futamura/preproc.c')],
                      { stdio: 'inherit' });
  if (r.status !== 0) { console.error('preproc build failed'); process.exit(1); }
}

// "unmarked" = the program as wallisp would see it without the preproc step:
// (work x) does (+ x (sum-to 100)), recomputing sum-to on every call.
// "folded"   = the same program with $(sum-to 100) preprocessed away.
const unmarked = fs.readFileSync(DEMO_UNMARKED, 'utf8');
const marked   = fs.readFileSync(DEMO_MARKED,   'utf8');
const folded   = spawnSync(PREPROC, [], { input: marked, encoding: 'utf8' }).stdout;

console.log('--- unmarked source (runtime fallback) ---');
console.log(unmarked.trim());
console.log('\n--- preprocessed source ---');
console.log(folded.trim());

async function load(file) {
  const bytes = fs.readFileSync(path.join(ROOT, file));
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const e = instance.exports;
  const mem = new Uint8Array(e.memory.buffer);
  const enc = new TextEncoder(), dec = new TextDecoder();
  return {
    run(src) {
      const b = enc.encode(src);
      mem.set(b, e.input_ptr());
      const n = e.eval_source(b.length);
      return dec.decode(mem.subarray(e.output_ptr(), e.output_ptr() + n));
    }
  };
}

function bestOf(fn, reps = 25) {
  let lo = Infinity, res;
  for (let i = 0; i < reps; i++) {
    const t = process.hrtime.bigint();
    res = fn();
    const d = Number(process.hrtime.bigint() - t) / 1e6;
    if (d < lo) lo = d;
  }
  return { ms: lo, res };
}

const main = async () => {
  for (const name of ['lisp_big.wasm', 'bytecode_gc.wasm']) {
    const eng = await load(name);
    for (let i = 0; i < 5; i++) { eng.run(unmarked); eng.run(folded); }

    const rU = bestOf(() => eng.run(unmarked));
    const rF = bestOf(() => eng.run(folded));

    console.log(`\n--- ${name}, best-of-25 ---`);
    console.log(`  unmarked: ${rU.ms.toFixed(3)} ms  ->  ${rU.res}`);
    console.log(`  folded  : ${rF.ms.toFixed(3)} ms  ->  ${rF.res}`);
    const agree = rU.res === rF.res ? '✓' : `⚠ DISAGREE (${rU.res} vs ${rF.res})`;
    console.log(`  agree   : ${agree}`);
    console.log(`  speedup : ${(rU.ms / rF.ms).toFixed(2)}×`);
  }
};

main().catch(e => { console.error(e); process.exit(1); });
