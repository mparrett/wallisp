// harness/parity.mjs — cross-engine semantic parity.
//
// All eight engines export the same wasm ABI and claim to implement the same
// language. bench.mjs cross-checks results at runtime (it flags ⚠ DISAGREE),
// but it's a benchmark, not a failing test. This is the failing test:
// every engine, every program, must produce identical output. Exits 1 on
// any disagreement so CI catches engine drift.
//
//   node harness/parity.mjs

import fs from 'fs';

const ENGINES = [
  'lisp.wasm',
  'lisp_trampoline.wasm',
  'lisp_gc.wasm',
  'lisp_region.wasm',
  'cek.wasm',
  'cek_gc.wasm',
  'bytecode.wasm',
  'bytecode_gc.wasm',
];

// Programs cover: numeric arithmetic, list ops, quote, cond/if, let, lambda,
// closures, recursion (tail and non-tail), mutual recursion, primitive
// rebinding (must defeat any inline-prim shortcut), edge cases on car/cdr.
// Designed for "must match across engines," not for "must match a known
// answer" — we use the tree-walker (lisp.wasm) as the reference and require
// all seven others to agree with it.
const PROGRAMS = [
  // arithmetic
  '(+ 1 2 3)',
  '(- 100 7 3)',
  '(* 6 7)',
  '(+ (* 2 3) (- 10 4))',
  '(= 0 0)',
  '(= 0 1)',
  '(< 3 5)',
  '(< 5 3)',

  // booleans / nil
  '(if (< 1 2) 1 2)',
  '(if (= 1 2) 1 2)',
  '(null? nil)',
  '(null? (quote ()))',
  '(null? (quote (1)))',
  '(pair? (quote (1 2)))',
  '(pair? nil)',

  // cons / car / cdr
  '(cons 1 2)',
  '(cons 1 (cons 2 (cons 3 nil)))',
  '(car (quote (a b c)))',
  '(cdr (quote (a b c)))',
  '(car (cons 9 8))',
  '(cdr (cons 9 8))',

  // quote
  '(quote a)',
  '(quote (1 2 3))',
  '(quote ((1 2) (3 4)))',

  // let
  '(let ((x 5)) x)',
  '(let ((x 5) (y 7)) (+ x y))',
  '(let ((x 1)) (let ((y 2)) (+ x y)))',

  // lambda + closures
  '((lambda (x) (* x x)) 9)',
  '((lambda (x y) (+ x y)) 3 4)',
  '(begin (define add (lambda (a) (lambda (b) (+ a b)))) ((add 10) 32))',
  '(begin (define make-counter (lambda (n) (lambda () (+ n 1)))) ((make-counter 41)))',

  // recursion (non-tail)
  '(begin (define fact (lambda (n) (if (< n 1) 1 (* n (fact (- n 1)))))) (fact 5))',
  '(begin (define fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))) (fib 10))',
  '(begin (define len (lambda (l) (if (null? l) 0 (+ 1 (len (cdr l)))))) (len (quote (a b c d e))))',

  // recursion (tail)
  '(begin (define loop (lambda (i a) (if (= i 0) a (loop (- i 1) (+ a i))))) (loop 100 0))',
  '(begin (define cd (lambda (n) (if (= n 0) (quote done) (cd (- n 1))))) (cd 500))',

  // mutual recursion
  '(begin (define ev (lambda (n) (if (= n 0) (quote t) (od (- n 1))))) (define od (lambda (n) (if (= n 0) (quote ()) (ev (- n 1))))) (ev 12))',
  '(begin (define ev (lambda (n) (if (= n 0) (quote t) (od (- n 1))))) (define od (lambda (n) (if (= n 0) (quote ()) (ev (- n 1))))) (od 7))',

  // list reverse + sum (small)
  '(begin (define ap (lambda (a b) (if (null? a) b (cons (car a) (ap (cdr a) b))))) (define rv (lambda (l) (if (null? l) nil (ap (rv (cdr l)) (cons (car l) nil))))) (rv (quote (1 2 3 4 5))))',
  '(begin (define sm (lambda (l) (if (null? l) 0 (+ (car l) (sm (cdr l)))))) (sm (quote (1 2 3 4 5 6 7 8 9 10))))',

  // primitive rebinding — must NOT be silently bypassed by any inline-prim path
  '(begin (define + (lambda (a b) 99)) (+ 1 2))',
  '(begin (define * (lambda (a b) 0)) (* 7 8))',

  // comments
  '; comment\n(+ 2 40) ; trailing',
];

async function load(file) {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(new URL('../' + file, import.meta.url)), {});
  const ex = instance.exports, mem = ex.memory;
  return (src) => {
    const e = new TextEncoder().encode(src);
    new Uint8Array(mem.buffer, ex.input_ptr(), e.length).set(e);
    const n = ex.eval_source(e.length);
    return new TextDecoder().decode(new Uint8Array(mem.buffer, ex.output_ptr(), n));
  };
}

const main = async () => {
  const engines = [];
  for (const f of ENGINES) engines.push([f, await load(f)]);
  const [refName, refRun] = engines[0];

  let failures = 0, programs = 0;
  for (const src of PROGRAMS) {
    programs++;
    const expected = refRun(src);
    const diffs = [];
    for (let i = 1; i < engines.length; i++) {
      const [name, run] = engines[i];
      const got = run(src);
      if (got !== expected) diffs.push({ name, got });
    }
    if (diffs.length) {
      failures++;
      console.log(`DISAGREE  ${JSON.stringify(src).slice(0, 70)}`);
      console.log(`  ${refName.padEnd(24)} => ${JSON.stringify(expected)}`);
      for (const { name, got } of diffs) {
        console.log(`  ${name.padEnd(24)} => ${JSON.stringify(got)}`);
      }
    }
  }
  console.log(`\n${programs - failures}/${programs} programs agree across all ${engines.length} engines`);
  if (failures) process.exit(1);
};

main().catch(e => { console.error(e); process.exit(1); });
