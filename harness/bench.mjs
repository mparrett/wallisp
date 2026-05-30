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
import { spawnSync } from 'child_process';

// Metacircular eval: same algorithm shape as the direct fib benchmark, but
// run *through* a tiny Lisp interpreter written in wallisp's own Lisp. The
// resulting tax (meta-fib / direct-fib) is "interpretation on top of
// interpretation" — the cost of hosting an evaluator inside an evaluator.
// fib(12) lands in the 3-30ms range; small enough to keep bench runtime
// reasonable, large enough that noise doesn't dominate.
const META_N = 12;
const META_SRC = fs.readFileSync(new URL('../baselines/metacircular.lisp', import.meta.url), 'utf8')
  .replace(/\b8\)\)/, `${META_N}))`);

const ENGINES = [
  ['tree-walker', 'lisp_big.wasm',             false],
  ['TW_tramp',    'lisp_trampoline_big.wasm',  false],  // explicit while(TRUE) (H1)
  ['TW_region',   'lisp_region_big.wasm',      true],   // region-drop GC (H2 zero floor)
  ['TW_gc',       'lisp_gc.wasm',              true],   // mark-sweep GC (H4)
  ['CEK',         'cek_big.wasm',              false],
  ['CEK_gc',      'cek_gc.wasm',               true],
  ['bytecode',    'bytecode_big.wasm',         false],
  ['bytecode_gc', 'bytecode_gc.wasm',          true],
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

  [`meta-fib(${META_N})`, 'metacircular evaluator (Lisp-in-Lisp) running Y-combinator fib',
   META_SRC],
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

  printBaselines(engines.find(([n]) => n === 'bytecode_gc')[2]);
};

// Reference points: same algorithms hand-written in JS (V8 native, no
// interpreter) and C (native, -O2). Standalone equivalents live in
// baselines/bench.{js,c}; inlined here so `node harness/bench.mjs` produces
// the full comparison in one pass.
function jsBaselines() {
  function fib(n) { return n < 2 ? n : fib(n-1) + fib(n-2); }
  function tak(x,y,z) { return y < x ? tak(tak(x-1,y,z), tak(y-1,z,x), tak(z-1,x,y)) : z; }
  function ack(m,n) { if (m===0) return n+1; if (n===0) return ack(m-1,1); return ack(m-1, ack(m,n-1)); }
  const cons = (h,t) => ({h,t});
  const iota = n => n===0 ? null : cons(n, iota(n-1));
  const app  = (a,b) => a===null ? b : cons(a.h, app(a.t, b));
  const nrev = l => l===null ? null : app(nrev(l.t), cons(l.h, null));
  const lsum = l => l===null ? 0 : l.h + lsum(l.t);
  // V8 has no TCE — iterative form matches the algorithm a JS programmer would write.
  const tailsum = n => { let a = 0; while (n > 0) { a += n; n--; } return a; };
  return [
    () => fib(24),
    () => tak(18,12,6),
    () => ack(3,4),
    () => lsum(nrev(iota(150))),
    () => tailsum(30000),
  ];
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

function runCBaseline() {
  // Spawn the prebuilt baseline binary if present; otherwise skip the C row.
  const bin = new URL('../native_bench_baseline', import.meta.url).pathname;
  if (!fs.existsSync(bin)) return null;
  const out = spawnSync(bin, [], { encoding: 'utf8' });
  if (out.status !== 0) return null;
  // TSV: engine \t benchmark \t ms \t result
  const map = new Map();
  for (const line of out.stdout.split('\n').slice(1)) {
    const [, name, ms, res] = line.split('\t');
    if (name) map.set(name, { ms: parseFloat(ms), res });
  }
  return map;
}

function printBaselines(bcEngine) {
  const fns = jsBaselines();
  const w = 14;
  console.log('\nbaselines (same algorithms, no interpreter):');
  let header = 'benchmark'.padEnd(18) + 'bytecode_gc'.padStart(w) + 'js (V8)'.padStart(w) + 'c (-O2)'.padStart(w) + '   bc_gc / js     bc_gc / c';
  console.log(header);
  console.log('-'.repeat(header.length));
  const cmap = runCBaseline();
  // The hand-written JS/C baselines only cover the original five direct
  // benchmarks; meta-fib(12) has no comparable hand-written form (you'd
  // be hand-writing a metacircular evaluator in JS, which defeats the
  // point). Iterate over only the benchmarks that have a JS twin.
  for (let i = 0; i < fns.length; i++) {
    const [name, , src] = BENCHMARKS[i];
    const bc = bestOf(() => bcEngine.run(src));
    const js = bestOf(fns[i]);
    const c  = cmap ? cmap.get(name) : null;
    let line = name.padEnd(18);
    line += (bc.ms.toFixed(3) + 'ms').padStart(w);
    line += (js.ms.toFixed(3) + 'ms').padStart(w);
    line += (c ? (c.ms.toFixed(3) + 'ms').padStart(w) : 'n/a'.padStart(w));
    line += ('   ' + (bc.ms / js.ms).toFixed(1) + 'x').padStart(15);
    line += (c ? ('   ' + (c.ms > 0 ? (bc.ms / c.ms).toFixed(1) + 'x' : '∞')).padStart(15) : '   n/a'.padStart(15));
    console.log(line);
  }
  if (!cmap) console.log('  (c baseline skipped: run `bash build.sh --native` to build native_bench_baseline)');
}

main().catch(e => { console.error(e); process.exit(1); });
