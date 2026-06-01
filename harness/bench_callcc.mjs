// harness/bench_callcc.mjs — EXP2 pre-registered benchmark.
//
// Hypothesis (gap_closure_plan.md EXP2): CEK with call/cc runs a yield-style
// generator within 1.5× of bytecode_gc running the equivalent sum via
// straight tail recursion. The generator pulls N values; bytecode_gc sums
// 0..N-1 directly without call/cc. Same arithmetic result; different control
// flow.
//
// Falsifications (pre-registered):
//   • Ratio > 3×        → call/cc capture cost dominates; the "K is already
//                          first-class internally" framing was wrong.
//   • cek_callcc faster → finally a benchmark where CEK exclusively wins.
//                          Would justify CEK's existence on speed grounds.
//
// Both engines have GC, so arena exhaustion isn't the limiter here; we
// measure steady-state allocation + dispatch cost. We also run cek_gc on the
// explicit-recursion version as a reference, so we can decompose the gap
// between "call/cc is expensive" and "CEK is generally slower than bytecode."
//
//   node harness/bench_callcc.mjs

import fs from 'fs';

const N = 2000;
const REPS = 25;

// CEK runs this — a coroutine-style generator. yield captures the consumer's
// continuation, gen-loop is resumed via the stored resume-k.
const GEN = `
(begin
  (define resume nil)
  (define yield-k nil)
  (define yield (lambda (v)
    (call/cc (lambda (k) (begin (set! yield-k k) (resume v))))))
  (define gen-loop (lambda (i)
    (begin (yield i) (gen-loop (+ i 1)))))
  (define next (lambda ()
    (call/cc (lambda (k)
      (begin (set! resume k)
             (if (null? yield-k) (gen-loop 0) (yield-k 0)))))))
  (define sum-loop (lambda (i acc n)
    (if (= i n) acc (sum-loop (+ i 1) (+ acc (next)) n))))
  (sum-loop 0 0 ${N}))`;

// Same arithmetic via straight tail recursion.
const EXPLICIT = `
(begin
  (define sum-loop (lambda (i acc n)
    (if (= i n) acc (sum-loop (+ i 1) (+ acc i) n))))
  (sum-loop 0 0 ${N}))`;

// Microbench: one call/cc per loop iteration, synchronous invoke. Isolates
// the cost of capturing + invoking a continuation that survives only one
// step — no escape, no resume, no closure-stored k. This is the cheapest
// possible call/cc use; if even this is expensive, the generator gap above
// is a pure additive cost.
const SYNC_CC = `
(begin
  (define wrap (lambda (n) (call/cc (lambda (k) (k n)))))
  (define loop (lambda (i acc) (if (= i 0) acc (loop (- i 1) (+ acc (wrap i))))))
  (loop ${N} 0))`;

// Same shape as SYNC_CC but without call/cc — wrap is a plain identity. The
// CEK-internal cost of one extra function call per iteration; subtract this
// from SYNC_CC to isolate call/cc's marginal cost.
const SYNC_PLAIN = `
(begin
  (define wrap (lambda (n) n))
  (define loop (lambda (i acc) (if (= i 0) acc (loop (- i 1) (+ acc (wrap i))))))
  (loop ${N} 0))`;

async function load(file) {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(new URL('../'+file, import.meta.url)), {});
  const ex = instance.exports, mem = ex.memory;
  const run = (src) => {
    const e = new TextEncoder().encode(src);
    new Uint8Array(mem.buffer, ex.input_ptr(), e.length).set(e);
    const n = ex.eval_source(e.length);
    return new TextDecoder().decode(new Uint8Array(mem.buffer, ex.output_ptr(), n));
  };
  return { run, gc_count: ex.gc_count };
}

function best(eng, src, reps = REPS) {
  let lo = Infinity, res, gc = 0;
  for (let i = 0; i < reps; i++) {
    const t = process.hrtime.bigint();
    res = eng.run(src);
    const d = Number(process.hrtime.bigint() - t) / 1e6;
    if (d < lo) lo = d;
    if (eng.gc_count) gc = eng.gc_count();
  }
  return { ms: lo, res, gc };
}

const main = async () => {
  const cek_gc = await load('cek_gc.wasm');
  const bytecode_gc = await load('bytecode_gc.wasm');

  console.log(`benchmark: sum 0..${N-1} (N=${N})`);
  console.log(`reps: ${REPS}, min-of-3-passes\n`);

  // Min over 3 passes to suppress system noise (project's measure-don't-guess
  // discipline — bench variance is 10-25% per ENGINES.md).
  const passes = (run) => {
    let lo = Infinity, res = null, gc = 0;
    for (let p = 0; p < 3; p++) {
      const r = run();
      if (r.ms < lo) { lo = r.ms; res = r.res; gc = r.gc; }
    }
    return { ms: lo, res, gc };
  };
  const gen     = passes(() => best(cek_gc, GEN));
  const cekEx   = passes(() => best(cek_gc, EXPLICIT));
  const bcEx    = passes(() => best(bytecode_gc, EXPLICIT));
  const cekSync = passes(() => best(cek_gc, SYNC_CC));
  const cekPln  = passes(() => best(cek_gc, SYNC_PLAIN));
  const bcSync  = passes(() => best(bytecode_gc, SYNC_PLAIN));

  const w = 32;
  console.log('config'.padEnd(w) + 'ms'.padStart(12) + '  gc' + '   result');
  console.log('-'.repeat(70));
  const row = (lbl, r) => console.log(lbl.padEnd(w) + r.ms.toFixed(3).padStart(9) + 'ms' + String(r.gc).padStart(5) + `   ${r.res}`);
  row('cek_gc / call-cc generator',  gen);
  row('cek_gc / sync call/cc',       cekSync);
  row('cek_gc / plain fn (no cc)',   cekPln);
  row('cek_gc / explicit (no fn)',   cekEx);
  row('bytecode_gc / plain fn',      bcSync);
  row('bytecode_gc / explicit',      bcEx);

  console.log('\nratios:');
  console.log(`  cek_callcc_generator / bytecode_gc_explicit = ${(gen.ms / bcEx.ms).toFixed(2)}×  ← pre-registered ≤ 1.5×`);
  console.log(`  cek_sync_callcc / cek_plain_fn              = ${(cekSync.ms / cekPln.ms).toFixed(2)}×  ← marginal cost of one call/cc per iter`);
  console.log(`  cek_callcc_generator / cek_explicit         = ${(gen.ms / cekEx.ms).toFixed(2)}×  ← total generator tax inside CEK`);
  console.log(`  cek_explicit / bytecode_gc_explicit         = ${(cekEx.ms / bcEx.ms).toFixed(2)}×  ← baseline CEK vs bytecode gap on this shape`);

  // Two semantic groups: the "sum 0..N-1" group (gen, cekEx, bcEx) and the
  // "sum 1..N" group (cekSync, cekPln, bcSync — they go N..1 because that's
  // the shape that lets `wrap` see the loop index). Each group must self-agree.
  const a = [gen.res, cekEx.res, bcEx.res];
  const b = [cekSync.res, cekPln.res, bcSync.res];
  if (!a.every(r => r === a[0]) || !b.every(r => r === b[0])) {
    console.log('\n⚠ DISAGREE');
    console.log('  sum 0..N-1 group:', a);
    console.log('  sum 1..N   group:', b);
    process.exit(1);
  }
};

main().catch(e => { console.error(e); process.exit(1); });
