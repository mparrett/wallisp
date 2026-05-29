// bench.mjs — canonical small-interpreter benchmarks across the three engines.
//   node bench.mjs
// Add a benchmark by appending to BENCHMARKS. Each program is self-contained
// (every eval_source re-inits, so no state persists between runs — include all
// defines in the source). Results are cross-checked: all three engines must
// agree, else it's flagged. Speed is best-of-N (min) to suppress GC/scheduler
// noise. Absolute ms are V8-on-wasm; trust the ratios, not the magnitudes.

import fs from 'fs';

const ENGINES = [
  ['tree-walker', 'lisp_big.wasm'],
  ['CEK',         'cek_big.wasm'],
  ['bytecode',    'bytecode_big.wasm'],
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
  return (src) => {
    const e = new TextEncoder().encode(src);
    new Uint8Array(mem.buffer, ex.input_ptr(), e.length).set(e);
    const n = ex.eval_source(e.length);
    return new TextDecoder().decode(new Uint8Array(mem.buffer, ex.output_ptr(), n));
  };
}

function best(run, src, reps = 25) {
  let lo = Infinity, res;
  for (let i = 0; i < reps; i++) {
    const t = process.hrtime.bigint();
    res = run(src);
    const d = Number(process.hrtime.bigint() - t) / 1e6;
    if (d < lo) lo = d;
  }
  return { ms: lo, res };
}

const main = async () => {
  const engines = [];
  for (const [name, file] of ENGINES) engines.push([name, await load(file)]);

  const w = 16;
  let header = 'benchmark'.padEnd(18);
  for (const [name] of engines) header += name.padStart(w);
  header += '   bc vs TW';
  console.log(header);
  console.log('-'.repeat(header.length));

  for (const [name, , src] of BENCHMARKS) {
    const runs = engines.map(([, run]) => best(run, src));
    // correctness cross-check
    const results = runs.map(r => r.res);
    const agree = results.every(r => r === results[0]);
    const flag = agree ? '' : '  ⚠ DISAGREE: ' + JSON.stringify(results);

    let line = name.padEnd(18);
    for (const r of runs) line += (r.ms.toFixed(3) + 'ms').padStart(w);
    const tw = runs[0].ms, bc = runs[2].ms;
    line += ('  ' + (tw / bc).toFixed(2) + 'x');
    console.log(line + flag);
  }

  console.log('\nresult sanity (should match across engines):');
  for (const [name, , src] of BENCHMARKS) {
    const r = engines[0][1](src);
    console.log(`  ${name.padEnd(18)} = ${r}`);
  }
  console.log('\nshapes:');
  for (const [name, shape] of BENCHMARKS) console.log(`  ${name.padEnd(18)} ${shape}`);
};

main().catch(e => { console.error(e); process.exit(1); });
