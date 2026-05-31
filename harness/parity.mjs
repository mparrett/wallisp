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
  'cek.wasm',
  'cek_gc.wasm',
  'bytecode.wasm',
  'bytecode_gc.wasm',
];

// PR1 (primitive validation + variadic +/-/*  + / + mod + 30-bit overflow
// trap) is being rolled out engine-by-engine; PR1a pilots on lisp.wasm.
// Engines in this set must match the expected output in PR1_PROGRAMS. The
// rest still produce older behaviour (e.g. (+ 1 2 3) -> 3 because they
// silently dropped trailing args, (+ 1) -> 1, division wasn't a prim).
// When all engines ship PR1, fold PR1_PROGRAMS back into PROGRAMS and
// delete this gate.
const SUPPORTS_PR1 = new Set([
  'lisp.wasm',
]);

// Programs cover: numeric arithmetic, list ops, quote, cond/if, let, lambda,
// closures, recursion (tail and non-tail), mutual recursion, primitive
// rebinding (must defeat any inline-prim shortcut), edge cases on car/cdr.
// Designed for "must match across engines," not for "must match a known
// answer" — we use the tree-walker (lisp.wasm) as the reference and require
// all seven others to agree with it.
const PROGRAMS = [
  // arithmetic — binary only at this layer; variadic +/-/* moved to
  // PR1_PROGRAMS below (semantics differ pre/post PR1).
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
];

// Programs whose expected output depends on PR1 semantics. Only checked
// against engines in SUPPORTS_PR1; non-PR1 engines produce older behaviour
// for these inputs (variadic +/-/* silently dropped trailing args; (+ 1)
// returned 1; / and mod weren't bound; arithmetic silently wrapped at
// 30 bits). Each entry is [source, expected_output]. Fold back into
// PROGRAMS as cross-checks once every engine ships PR1.
const PR1_PROGRAMS = [
  // variadic + / - / * (was binary-only on non-PR1 engines)
  ['(+ 1 2 3)',        '6'],
  ['(- 100 7 3)',      '90'],
  ['(* 2 3 4)',        '24'],
  // arity errors on primitives
  ['(+)',              '<error>'],
  ['(+ 1)',            '<error>'],
  ['(-)',              '<error>'],
  ['(- 1)',            '<error>'],
  ['(*)',              '<error>'],
  ['(cons)',           '<error>'],
  ['(cons 1)',         '<error>'],
  ['(cons 1 2 3)',     '<error>'],
  ['(car)',            '<error>'],
  ['(cdr 1 2)',        '<error>'],
  ['(=)',              '<error>'],
  ['(= 1)',            '<error>'],
  ['(< 1)',            '<error>'],
  ['(null?)',          '<error>'],
  ['(null? 1 2)',      '<error>'],
  // type errors on primitives
  ["(+ 'a 1)",         '<error>'],
  ["(- 1 'a)",         '<error>'],
  ["(* 'a 'b)",        '<error>'],
  ['(car 5)',          '<error>'],
  ['(car nil)',        '<error>'],
  ['(cdr 5)',          '<error>'],
  ["(< 'a 'b)",        '<error>'],
  // = stays polymorphic identity — metacircular evaluator depends on it
  ["(= 'a 'a)",        't'],
  ["(= 'a 'b)",        '()'],
  ['(= nil nil)',      't'],
  // new ops: / and mod, including divide-by-zero
  ['(/ 6 2)',          '3'],
  ['(/ 7 2)',          '3'],
  ['(/ -7 2)',         '-3'],   // C truncation toward zero
  ['(/ 1 0)',          '<error>'],
  ['(/)',              '<error>'],
  ['(/ 5)',            '<error>'],
  ['(mod 7 3)',        '1'],
  ['(mod -7 3)',       '-1'],   // C remainder semantics
  ['(mod 1 0)',        '<error>'],
  ['(mod 5)',          '<error>'],
  // 30-bit overflow trap
  ['(+ 536870900 100)',     '<error>'],
  ['(- -536870900 100)',    '<error>'],
  ['(* 100000 100000)',     '<error>'],
  ['(* 23000 23000)',       '529000000'],  // fits (5.29e8 < 5.37e8)
  ['(+ 536870910 1)',       '536870911'],  // FIX_MAX boundary OK
  ['(+ 536870911 1)',       '<error>'],
  ['(/ -536870912 -1)',     '<error>'],    // FIX_MIN / -1 = FIX_MAX+1
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

  // PR1: assert PR1-supporting engines match the expected output. During
  // rollout (PR1a/PR1b) this catches regressions in piloted engines without
  // requiring the others to keep up.
  const pr1Engines = engines.filter(([f]) => SUPPORTS_PR1.has(f));
  let pr1Failures = 0;
  console.log(`\nPR1 programs against ${pr1Engines.length} engine(s): ${pr1Engines.map(([f]) => f).join(', ')}`);
  for (const [src, want] of PR1_PROGRAMS) {
    for (const [name, run] of pr1Engines) {
      const got = run(src);
      if (got !== want) {
        pr1Failures++;
        console.log(`PR1 FAIL  ${name.padEnd(24)} ${JSON.stringify(src).slice(0, 50)}`);
        console.log(`  expected ${JSON.stringify(want)}`);
        console.log(`  got      ${JSON.stringify(got)}`);
      }
    }
  }
  console.log(`${PR1_PROGRAMS.length * pr1Engines.length - pr1Failures}/${PR1_PROGRAMS.length * pr1Engines.length} PR1 assertions passed`);

  if (failures || pr1Failures) process.exit(1);
};

main().catch(e => { console.error(e); process.exit(1); });
