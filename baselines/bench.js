// baselines/bench.js — hand-written JS equivalents of the five Lisp
// benchmarks, run on V8 directly (no wasm, no interpreter). The reference
// point at the top end: what these algorithms cost when V8 sees JavaScript
// instead of our wasm-hosted Lisp.
//
//   node baselines/bench.js
//
// Compare against:
//   - harness/bench.mjs            (eight Lisp engines, wasm-on-V8)
//   - ./native_bench_baseline      (hand-written C, -O2, native)
//
// Output format matches the native bench: ENGINE \t BENCHMARK \t MS \t RESULT.

function fib(n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

function tak(x, y, z) {
  return y < x ? tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y)) : z;
}

function ack(m, n) {
  if (m === 0) return n + 1;
  if (n === 0) return ack(m - 1, 1);
  return ack(m - 1, ack(m, n - 1));
}

// Cons-cell shaped linked list to match the Lisp algorithm; switching to
// arrays would change the asymptotic memory pattern and make the comparison
// dishonest.
const cons = (h, t) => ({ h, t });
function iota(n) { return n === 0 ? null : cons(n, iota(n - 1)); }
function app(a, b) { return a === null ? b : cons(a.h, app(a.t, b)); }
function nrev(l) { return l === null ? null : app(nrev(l.t), cons(l.h, null)); }
function lsum(l) { return l === null ? 0 : l.h + lsum(l.t); }

// V8 never shipped proper TCE, so the recursive form blows the stack at
// 30000. The JS equivalent of the tail-recursive Lisp version is the
// iterative form — what a JS programmer would actually write for this shape.
function tailsum(n) { let acc = 0; while (n > 0) { acc += n; n--; } return acc; }

const BENCHES = [
  ['fib(24)',        () => fib(24)],
  ['tak(18,12,6)',   () => tak(18, 12, 6)],
  ['ack(3,4)',       () => ack(3, 4)],
  ['nrev+sum(150)',  () => lsum(nrev(iota(150)))],
  ['tailsum(30000)', () => tailsum(30000)],
];

const REPS = 25;
console.log('engine\tbenchmark\tms\tresult');
for (const [name, fn] of BENCHES) {
  let lo = Infinity, res;
  for (let i = 0; i < REPS; i++) {
    const t = process.hrtime.bigint();
    res = fn();
    const d = Number(process.hrtime.bigint() - t) / 1e6;
    if (d < lo) lo = d;
  }
  console.log(`js\t${name}\t${lo.toFixed(4)}\t${res}`);
}
