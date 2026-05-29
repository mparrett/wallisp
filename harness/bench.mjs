// bench.mjs — canonical small-interpreter benchmarks across the four engines.
//   node bench.mjs
// Add a benchmark by appending to BENCHMARKS. Each program is self-contained
// (every eval_source re-inits, so no state persists between runs — include all
// defines in the source). Results are cross-checked: all four engines must
// agree, else it's flagged. Speed is best-of-N (min) to suppress GC/scheduler
// noise. Absolute ms are V8-on-wasm; trust the ratios, not the magnitudes.
//
// bytecode_gc.wasm uses its DEFAULT arena (262K cells) deliberately — that's
// what exposes GC pressure so the gc_count column is meaningful. The other
// three use *_big variants (16M cells) because they have no GC and would
// exhaust the arena on heavy benchmarks otherwise.

import fs from 'fs';

const ENGINES = [
  ['tree-walker', 'lisp_big.wasm',         false],
  ['TW_region',   'lisp_region_big.wasm',  true],   // region-drop GC (H2 zero floor)
  ['TW_gc',       'lisp_gc.wasm',          true],   // mark-sweep GC (H4)
  ['CEK',         'cek_big.wasm',          false],
  ['CEK_gc',      'cek_gc.wasm',           true],
  ['bytecode',    'bytecode_big.wasm',     false],
  ['bytecode_gc', 'bytecode_gc.wasm',      true],
];

// Each: [name, shape-it-stresses, lisp-source]
const BENCHMARKS = [
  ['fib(24)', 'deep non-tail recursion, scalar arithmetic, no allocation',
   `(begin
      (define fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2))))))
      (fib 24))`],

  ['tak(18,12,6)', 'extreme call volume, no allocation (Gabriel classic)',
   `(begin
      (define tak (lambda (x y z)
        (if (< y x)
            (tak (tak (- x 1) y z) (tak (- y 1) z x) (tak (- z 1) x y))
            z)))
      (tak 18 12 6))`],

  ['ack(3,4)', 'deeply nested non-tail recursion (Ackermann)',
   `(begin
      (define ack (lambda (m n)
        (if (= m 0) (+ n 1)
          (if (= n 0) (ack (- m 1) 1)
            (ack (- m 1) (ack m (- n 1)))))))
      (ack 3 4))`],

  ['nrev+sum(150)', 'ALLOCATION-bound: O(n^2) conses (naive reverse)',
   `(begin
      (define iota (lambda (n) (if (= n 0) nil (cons n (iota (- n 1))))))
      (define app  (lambda (a b) (if (null? a) b (cons (car a) (app (cdr a) b)))))
      (define nrev (lambda (l) (if (null? l) nil (app (nrev (cdr l)) (cons (car l) nil)))))
      (define sum  (lambda (l) (if (null? l) 0 (+ (car l) (sum (cdr l))))))
      (sum (nrev (iota 150))))`],

  ['tailsum(30000)', 'tight tail-recursive loop with accumulator',
   `(begin
      (define loop (lambda (i acc) (if (= i 0) acc (loop (- i 1) (+ acc i)))))
      (loop 30000 0))`],
];

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

function best(eng, src, reps = 25) {
  // gc_count() returns g_numgc, which init() resets to 0 at the start of every
  // eval_source — so we record per-run and report the per-run count (same value
  // each run since the program is deterministic).
  let lo = Infinity, res, gc = 0;
  for (let i = 0; i < reps; i++) {
    const t = process.hrtime.bigint();
    res = eng.run(src);
    const d = Number(process.hrtime.bigint() - t) / 1e6;
    if (d < lo) lo = d;
    if (eng.gc_count) gc = eng.gc_count(); // overwrites; identical across reps
  }
  return { ms: lo, res, gc };
}

const main = async () => {
  const engines = [];
  for (const [name, file, hasGc] of ENGINES) engines.push([name, hasGc, await load(file)]);

  const w = 14;
  const bcIdx = engines.findIndex(([n]) => n === 'bytecode');
  let header = 'benchmark'.padEnd(18);
  for (const [name] of engines) header += name.padStart(w);
  header += '   bc vs TW   gc (tw_rgn / tw_gc / cek_gc / bc_gc)';
  console.log(header);
  console.log('-'.repeat(header.length));

  for (const [name, , src] of BENCHMARKS) {
    const runs = engines.map(([, , eng]) => best(eng, src));
    const results = runs.map(r => r.res);
    const agree = results.every(r => r === results[0]);
    const flag = agree ? '' : '  ⚠ DISAGREE: ' + JSON.stringify(results);

    let line = name.padEnd(18);
    for (const r of runs) line += (r.ms.toFixed(3) + 'ms').padStart(w);
    const tw = runs[0].ms, bc = runs[bcIdx].ms;
    line += ('       ' + (tw / bc).toFixed(2) + 'x').padStart(12);
    const gcRuns = engines.map(([, hasGc], i) => hasGc ? runs[i].gc : null).filter(g => g !== null);
    line += ('   ' + gcRuns.join(' / ')).padStart(22);
    console.log(line + flag);
  }

  console.log('\nresult sanity (should match across engines):');
  for (const [name, , src] of BENCHMARKS) {
    const r = engines[0][2].run(src);
    console.log(`  ${name.padEnd(18)} = ${r}`);
  }
  console.log('\nshapes:');
  for (const [name, shape] of BENCHMARKS) console.log(`  ${name.padEnd(18)} ${shape}`);
};

main().catch(e => { console.error(e); process.exit(1); });
