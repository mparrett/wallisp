// harness/parity.mjs — cross-engine semantic parity, with pinned values.
//
// All nine engines export the same wasm ABI and claim to implement the same
// language. bench.mjs cross-checks results at runtime (it flags ⚠ DISAGREE),
// but it's a benchmark, not a failing test. This is the failing test, and it
// asserts two independent things for every program:
//
//   1. VALUE      — the reference engine produces the pinned expected output.
//   2. AGREEMENT  — every other engine matches the reference.
//
// Agreement alone is not enough: a change that broke all nine engines the same
// way — a shared reader.h regression, say — would agree perfectly and pass. So
// the value is pinned independently of the consensus. Exits 1 on either.
//
// HOW MUCH THE PINS ACTUALLY ASSERT. For the 93 value-bearing programs, a lot:
// the exact printed result. For the 48 that expect `<error>`, much less. Every
// engine prints one opaque token for every failure — arity, type, unbound
// symbol, divide-by-zero, 30-bit overflow, arena exhaustion, parse failure all
// collapse to `<error>` at a single print site. Pinning it asserts "this still
// fails", not "this still fails for the right reason". A program that stopped
// parsing would be indistinguishable from one correctly rejecting its
// arguments. Differentiated errors would fix that; until then, don't read a
// green run as validating the error paths.
//
// The pinned values were captured from the suite's own output and reviewed by
// hand, so they characterise intended behaviour rather than proving it — the
// engines are the only specification there is. Two pins are worth knowing
// before you "fix" them:
//
//   * `(car nil)` is `<error>`, not `()`. Deliberate: an explicit `!is_cons`
//     guard in each engine's PR_CAR.
//   * `(mod -7 3)` is `-1` and `(/ -7 2)` is `-3`. These are bare C `%` and `/`
//     with only a zero-divisor guard, i.e. R7RS `remainder`/`truncate` rather
//     than `modulo`/`floor`, despite the name `mod`. Inherited from C, not
//     argued for anywhere in the sources — pinned so that changing it has to be
//     a deliberate edit here rather than a silent drift.
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
// a known answer — every entry now carries both: the tree-walker (lisp.wasm)
// is the reference the other eight must agree with, and its own output is
// checked against the pinned value.
const PROGRAMS = [
  // arithmetic
  ['(+ 1 2 3)', "6"],
  ['(- 100 7 3)', "90"],
  ['(* 2 3 4)', "24"],
  ['(* 6 7)', "42"],
  ['(+ (* 2 3) (- 10 4))', "12"],
  ['(= 0 0)', "t"],
  ['(= 0 1)', "()"],
  ['(< 3 5)', "t"],
  ['(< 5 3)', "()"],

  // booleans / nil
  ['(if (< 1 2) 1 2)', "1"],
  ['(if (= 1 2) 1 2)', "2"],
  ['(null? nil)', "t"],
  ['(null? (quote ()))', "t"],
  ['(null? (quote (1)))', "()"],
  ['(pair? (quote (1 2)))', "t"],
  ['(pair? nil)', "()"],

  // cons / car / cdr
  ['(cons 1 2)', "(1 . 2)"],
  ['(cons 1 (cons 2 (cons 3 nil)))', "(1 2 3)"],
  ['(car (quote (a b c)))', "a"],
  ['(cdr (quote (a b c)))', "(b c)"],
  ['(car (cons 9 8))', "9"],
  ['(cdr (cons 9 8))', "8"],

  // quote
  ['(quote a)', "a"],
  ['(quote (1 2 3))', "(1 2 3)"],
  ['(quote ((1 2) (3 4)))', "((1 2) (3 4))"],

  // let
  ['(let ((x 5)) x)', "5"],
  ['(let ((x 5) (y 7)) (+ x y))', "12"],
  ['(let ((x 1)) (let ((y 2)) (+ x y)))', "3"],

  // reader sugar: fn(a, b) ≡ (fn a b), from the shared reader.h. Covered on the
  // wasm engines here — the native-only reader_sugar.sh doesn't exercise these.
  ['(begin (define add (lambda (x y) (+ x y))) add(2, 3))', "5"],
  ['(begin (define add (lambda (x y) (+ x y))) add(add(1, 2), 3))', "6"],
  ['(begin (define sq (lambda (x) (* x x))) sq(4))', "16"],
  ['(begin (define k (lambda () 42)) k())', "42"],
  ['car((quote (7 8 9)))', "7"],

  // lambda + closures
  ['((lambda (x) (* x x)) 9)', "81"],
  ['((lambda (x y) (+ x y)) 3 4)', "7"],
  ['(begin (define add (lambda (a) (lambda (b) (+ a b)))) ((add 10) 32))', "42"],
  ['(begin (define make-counter (lambda (n) (lambda () (+ n 1)))) ((make-counter 41)))', "42"],

  // recursion (non-tail)
  ['(begin (define fact (lambda (n) (if (< n 1) 1 (* n (fact (- n 1)))))) (fact 5))', "120"],
  ['(begin (define fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))) (fib 10))', "55"],
  ['(begin (define len (lambda (l) (if (null? l) 0 (+ 1 (len (cdr l)))))) (len (quote (a b c d e))))', "5"],

  // recursion (tail)
  ['(begin (define loop (lambda (i a) (if (= i 0) a (loop (- i 1) (+ a i))))) (loop 100 0))', "5050"],
  ['(begin (define cd (lambda (n) (if (= n 0) (quote done) (cd (- n 1))))) (cd 500))', "done"],

  // mutual recursion
  ['(begin (define ev (lambda (n) (if (= n 0) (quote t) (od (- n 1))))) (define od (lambda (n) (if (= n 0) (quote ()) (ev (- n 1))))) (ev 12))', "t"],
  ['(begin (define ev (lambda (n) (if (= n 0) (quote t) (od (- n 1))))) (define od (lambda (n) (if (= n 0) (quote ()) (ev (- n 1))))) (od 7))', "t"],

  // list reverse + sum (small)
  ['(begin (define ap (lambda (a b) (if (null? a) b (cons (car a) (ap (cdr a) b))))) (define rv (lambda (l) (if (null? l) nil (ap (rv (cdr l)) (cons (car l) nil))))) (rv (quote (1 2 3 4 5))))', "(5 4 3 2 1)"],
  ['(begin (define sm (lambda (l) (if (null? l) 0 (+ (car l) (sm (cdr l)))))) (sm (quote (1 2 3 4 5 6 7 8 9 10))))', "55"],

  // primitive rebinding — must NOT be silently bypassed by any inline-prim path
  ['(begin (define + (lambda (a b) 99)) (+ 1 2))', "99"],
  ['(begin (define * (lambda (a b) 0)) (* 7 8))', "0"],

  // comments
  ['; comment\n(+ 2 40) ; trailing', "42"],

  // arity check: under- and over-supply error; exact match still works.
  ['((lambda (x y) x) 1)', "<error>"],
  ['((lambda (x) x) 1 2)', "<error>"],
  ['((lambda (x y) (+ x y)) 1 2)', "3"],

  // define-form shorthand: (define (name args...) body)
  ['(begin (define (sq x) (* x x)) (sq 9))', "81"],
  ['(begin (define (add a b) (+ a b)) (add 3 4))', "7"],
  ['(begin (define (fact n) (if (< n 1) 1 (* n (fact (- n 1))))) (fact 5))', "120"],

  // cond: clause walk, else, no-else fallthrough, empty, and a recursive use
  // that exercises GC re-entry through a cond-rewritten branch.
  ["(cond ((< 1 2) 'a) (else 'b))", "a"],
  ["(cond ((= 1 2) 'a) ((= 3 3) 'b) (else 'c))", "b"],
  ["(cond ((= 1 2) 'a) ((= 3 4) 'b))", "()"],
  ['(cond)', "()"],
  ["(begin (define (sgn n) (cond ((< n 0) -1) ((< 0 n) 1) (else 0))) (cons (sgn -7) (cons (sgn 0) (cons (sgn 9) nil))))", "(-1 0 1)"],
  ['(begin (define (len l) (cond ((null? l) 0) (else (+ 1 (len (cdr l)))))) (len (quote (a b c d e))))', "5"],

  // ---- PR1: primitive validation ------------------------------------------
  // arity errors on primitives
  ['(+)', "<error>"],
  ['(+ 1)', "<error>"],
  ['(-)', "<error>"],
  ['(- 1)', "<error>"],
  ['(*)', "<error>"],
  ['(cons)', "<error>"],
  ['(cons 1)', "<error>"],
  ['(cons 1 2 3)', "<error>"],
  ['(car)', "<error>"],
  ['(cdr 1 2)', "<error>"],
  ['(=)', "<error>"],
  ['(= 1)', "<error>"],
  ['(< 1)', "<error>"],
  ['(null?)', "<error>"],
  ['(null? 1 2)', "<error>"],
  // type errors on primitives
  ["(+ 'a 1)", "<error>"],
  ["(- 1 'a)", "<error>"],
  ["(* 'a 'b)", "<error>"],
  ['(car 5)', "<error>"],
  ['(car nil)', "<error>"],
  ['(cdr 5)', "<error>"],
  ["(< 'a 'b)", "<error>"],
  // = stays polymorphic identity (metacircular evaluator needs symbol compare)
  ["(= 'a 'a)", "t"],
  ["(= 'a 'b)", "()"],
  ['(= nil nil)', "t"],
  // division and modulo (PR1 added /, mod)
  ['(/ 6 2)', "3"],
  ['(/ 7 2)', "3"],
  ['(/ -7 2)', "-3"],
  ['(/ 1 0)', "<error>"],
  ['(/)', "<error>"],
  ['(/ 5)', "<error>"],
  ['(mod 7 3)', "1"],
  ['(mod -7 3)', "-1"],
  ['(mod 1 0)', "<error>"],
  ['(mod 5)', "<error>"],
  // 30-bit overflow trap on arithmetic
  ['(+ 536870900 100)', "<error>"],
  ['(- -536870900 100)', "<error>"],
  ['(* 100000 100000)', "<error>"],
  ['(* 23000 23000)', "529000000"],
  ['(+ 536870910 1)', "536870911"],
  ['(+ 536870911 1)', "<error>"],
  ['(/ -536870912 -1)', "<error>"],

  // ---- PR2: mutation (set! / set-car! / set-cdr!) -------------------------
  ['(begin (define x 5) (set! x 10) x)', "10"],
  ['(begin (define x 5) (set! x (+ x 1)) x)', "6"],
  ['(set! y 10)', "<error>"],
  ['(set!)', "<error>"],
  ['(set! x)', "<error>"],
  ['(begin (define x 5) (set! x 1 2))', "<error>"],
  ['(set! 5 10)', "<error>"],
  ['(begin (define c (cons 1 2)) (set-car! c 9) c)', "(9 . 2)"],
  ['(begin (define c (cons 1 2)) (set-cdr! c 9) c)', "(1 . 9)"],
  ['(begin (define c (cons 1 (cons 2 nil))) (set-car! (cdr c) 99) c)', "(1 99)"],
  ['(set-car! 5 9)', "<error>"],
  ['(set-cdr! nil 9)', "<error>"],
  ['(set-car!)', "<error>"],
  ['(set-car! (cons 1 2))', "<error>"],
  ['(set-car! (cons 1 2) 9 10)', "<error>"],
  // killer test: mutation visible through a closure with lexical state
  ["(begin (define counter (let ((n 0)) (lambda () (begin (set! n (+ n 1)) n)))) (counter) (counter) (counter))", "3"],
  ['(begin (define n 0) (define inc (lambda () (begin (set! n (+ n 1)) n))) (inc) (inc) (inc) n)', "3"],
  ['(begin (define c (cons 1 2)) (set-car! c 9))', "9"],
  ['(begin (define x 1) (set! x 42))', "42"],

  // ── number? / symbol? predicates (post-Tier-B; metacircular-eval prep) ──
  ['(number? 42)', "t"],
  ['(number? -1)', "t"],
  ['(number? 0)', "t"],
  ['(number? nil)', "()"],
  ['(number? t)', "()"],
  ["(number? 'a)", "()"],
  ["(number? '(1 2 3))", "()"],
  ['(number?)', "<error>"],
  ['(number? 1 2)', "<error>"],
  ["(symbol? 'a)", "t"],
  ["(symbol? 'foo)", "t"],
  ['(symbol? nil)', "()"],
  ['(symbol? t)', "()"],
  ['(symbol? 5)', "()"],
  ["(symbol? '(a b))", "()"],
  ['(symbol?)', "<error>"],
  ["(symbol? 'a 'b)", "<error>"],
  // composes with other predicates — used by the upcoming metacircular eval
  ["(if (number? 5) 'num 'sym)", "num"],
  ["(if (symbol? 'x) 'sym 'num)", "sym"],
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

  let wrong = 0, disagreed = 0;
  // Programs are printed truncated, and 15 of them exceed the width. Two differ
  // only past it AND share an expected value (the mutual-recursion pair), so the
  // index is printed to keep every failure line identifiable.
  const label = (i, src) => `[${String(i).padStart(3)}] ${JSON.stringify(src).slice(0, 70)}`;

  PROGRAMS.forEach(([src, expected], i) => {
    const ref = refRun(src);

    // (1) VALUE. Catches a regression that hits every engine at once, which
    // the agreement check below is structurally blind to.
    if (ref !== expected) {
      wrong++;
      console.log(`WRONG     ${label(i, src)}`);
      console.log(`  expected ${JSON.stringify(expected)}`);
      console.log(`  ${refName.padEnd(24)} => ${JSON.stringify(ref)}`);
    }

    // (2) AGREEMENT, against the reference's ACTUAL output rather than the
    // pinned one. A trade, not a free win: it keeps a shared regression to a
    // single WRONG instead of eight spurious DISAGREEs, at the cost of eight
    // spurious DISAGREEs when the reference ALONE regresses. Both schemes catch
    // both faults; this way optimises for the shared-regression case, which is
    // the one this suite exists to catch.
    const diffs = [];
    for (let j = 1; j < engines.length; j++) {
      const [name, run] = engines[j];
      const got = run(src);
      if (got !== ref) diffs.push({ name, got });
    }
    if (diffs.length) {
      disagreed++;
      console.log(`DISAGREE  ${label(i, src)}`);
      console.log(`  ${refName.padEnd(24)} => ${JSON.stringify(ref)}`);
      for (const { name, got } of diffs) {
        console.log(`  ${name.padEnd(24)} => ${JSON.stringify(got)}`);
      }
    }
  });

  const n = PROGRAMS.length;
  console.log(`\n${n - wrong}/${n} programs match their pinned value`);
  console.log(`${n - disagreed}/${n} programs agree across all ${engines.length} engines`);
  if (wrong || disagreed) process.exit(1);
};

main().catch(e => { console.error(e); process.exit(1); });
