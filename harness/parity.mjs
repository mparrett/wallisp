// harness/parity.mjs — cross-engine semantic parity.
//
// All eight engines export the same wasm ABI and claim to implement the same
// language. bench.mjs cross-checks results at runtime (it flags ⚠ DISAGREE),
// but it's a benchmark, not a failing test. This is the failing test:
// every engine, every program, must produce identical output. Exits 1 on
// any disagreement so CI catches engine drift.
//
//   node harness/parity.mjs
//
// Uses default-arena builds (the small *.wasm files), so programs here must
// fit a 131K–262K cell heap. The metacircular evaluator in baselines/
// allocates ~10× more than that and lives in harness/bench.mjs, which uses
// the big-arena variants and cross-checks engine output the same way.

import fs from 'fs';

const ENGINES = [
  'lisp.wasm',
  'lisp_trampoline.wasm',
  'lisp_gc.wasm',
  'lisp_region.wasm',
  'lisp_rc.wasm',
  'cek.wasm',
  'cek_gc.wasm',
  'bytecode.wasm',
  'bytecode_gc.wasm',
];


// Programs cover: numeric arithmetic, list ops, quote, cond/if, let, lambda,
// closures, recursion (tail and non-tail), mutual recursion, primitive
// rebinding (must defeat any inline-prim shortcut), edge cases on car/cdr,
// and (since PR1c) primitive arity / type errors, division, modulo, and the
// 30-bit overflow trap. Designed for "must match across engines," not for
// "must match a known answer" — we use the tree-walker (lisp.wasm) as the
// reference and require all seven others to agree with it.
const PROGRAMS = [
  // arithmetic
  '(+ 1 2 3)',
  '(- 100 7 3)',
  '(* 2 3 4)',
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

  // reader sugar: fn(a, b) ≡ (fn a b), from the shared reader.h. Covered on the
  // wasm engines here — the native-only reader_sugar.sh doesn't exercise these.
  '(begin (define add (lambda (x y) (+ x y))) add(2, 3))',
  '(begin (define add (lambda (x y) (+ x y))) add(add(1, 2), 3))',
  '(begin (define sq (lambda (x) (* x x))) sq(4))',
  '(begin (define k (lambda () 42)) k())',
  'car((quote (7 8 9)))',

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

  // arity check: under- and over-supply error; exact match still works.
  '((lambda (x y) x) 1)',
  '((lambda (x) x) 1 2)',
  '((lambda (x y) (+ x y)) 1 2)',

  // define-form shorthand: (define (name args...) body)
  '(begin (define (sq x) (* x x)) (sq 9))',
  '(begin (define (add a b) (+ a b)) (add 3 4))',
  '(begin (define (fact n) (if (< n 1) 1 (* n (fact (- n 1))))) (fact 5))',

  // cond: clause walk, else, no-else fallthrough, empty, and a recursive use
  // that exercises GC re-entry through a cond-rewritten branch.
  "(cond ((< 1 2) 'a) (else 'b))",
  "(cond ((= 1 2) 'a) ((= 3 3) 'b) (else 'c))",
  "(cond ((= 1 2) 'a) ((= 3 4) 'b))",
  '(cond)',
  "(begin (define (sgn n) (cond ((< n 0) -1) ((< 0 n) 1) (else 0))) (cons (sgn -7) (cons (sgn 0) (cons (sgn 9) nil))))",
  '(begin (define (len l) (cond ((null? l) 0) (else (+ 1 (len (cdr l)))))) (len (quote (a b c d e))))',

  // ---- PR1: primitive validation ------------------------------------------
  // arity errors on primitives
  '(+)', '(+ 1)', '(-)', '(- 1)', '(*)',
  '(cons)', '(cons 1)', '(cons 1 2 3)',
  '(car)', '(cdr 1 2)',
  '(=)', '(= 1)', '(< 1)',
  '(null?)', '(null? 1 2)',
  // type errors on primitives
  "(+ 'a 1)", "(- 1 'a)", "(* 'a 'b)",
  '(car 5)', '(car nil)', '(cdr 5)',
  "(< 'a 'b)",
  // = stays polymorphic identity (metacircular evaluator needs symbol compare)
  "(= 'a 'a)", "(= 'a 'b)", '(= nil nil)',
  // division and modulo (PR1 added /, mod)
  '(/ 6 2)', '(/ 7 2)', '(/ -7 2)',
  '(/ 1 0)', '(/)', '(/ 5)',
  '(mod 7 3)', '(mod -7 3)', '(mod 1 0)', '(mod 5)',
  // 30-bit overflow trap on arithmetic
  '(+ 536870900 100)',
  '(- -536870900 100)',
  '(* 100000 100000)',
  '(* 23000 23000)',         // fits (5.29e8 < 5.37e8)
  '(+ 536870910 1)',         // FIX_MAX boundary OK
  '(+ 536870911 1)',
  '(/ -536870912 -1)',       // FIX_MIN / -1 = FIX_MAX+1

  // ---- PR2: mutation (set! / set-car! / set-cdr!) -------------------------
  '(begin (define x 5) (set! x 10) x)',
  '(begin (define x 5) (set! x (+ x 1)) x)',
  '(set! y 10)',                                      // unbound set! → error
  '(set!)', '(set! x)',
  '(begin (define x 5) (set! x 1 2))',                // over-arity → error
  '(set! 5 10)',                                      // non-symbol → error
  '(begin (define c (cons 1 2)) (set-car! c 9) c)',
  '(begin (define c (cons 1 2)) (set-cdr! c 9) c)',
  '(begin (define c (cons 1 (cons 2 nil))) (set-car! (cdr c) 99) c)',
  '(set-car! 5 9)', '(set-cdr! nil 9)',               // type errors
  '(set-car!)', '(set-car! (cons 1 2))',              // arity errors
  '(set-car! (cons 1 2) 9 10)',
  // killer test: mutation visible through a closure with lexical state
  "(begin (define counter (let ((n 0)) (lambda () (begin (set! n (+ n 1)) n)))) (counter) (counter) (counter))",
  '(begin (define n 0) (define inc (lambda () (begin (set! n (+ n 1)) n))) (inc) (inc) (inc) n)',
  '(begin (define c (cons 1 2)) (set-car! c 9))',    // set-car! returns new value
  '(begin (define x 1) (set! x 42))',                 // set! returns new value

  // ── number? / symbol? predicates (post-Tier-B; metacircular-eval prep) ──
  '(number? 42)', '(number? -1)', '(number? 0)',
  '(number? nil)', '(number? t)', "(number? 'a)",
  "(number? '(1 2 3))",
  '(number?)', '(number? 1 2)',                       // arity errors
  "(symbol? 'a)", "(symbol? 'foo)",
  '(symbol? nil)', '(symbol? t)',
  '(symbol? 5)', "(symbol? '(a b))",
  '(symbol?)', "(symbol? 'a 'b)",                     // arity errors
  // composes with other predicates — used by the upcoming metacircular eval
  "(if (number? 5) 'num 'sym)",
  "(if (symbol? 'x) 'sym 'num)",
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
