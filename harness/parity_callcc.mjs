// harness/parity_callcc.mjs — call/cc parity + pinned values, CEK engines only.
//
// EXP2 (gap-closure plan): call/cc is implemented on the two CEK engines and
// deliberately NOT bound on the other seven. Programs that reference call/cc
// error on the non-CEK engines, so they can't live in the unified parity
// suite — they go here.
//
// Like harness/parity.mjs, this asserts two independent things per program:
//
//   1. VALUE      — the reference engine (cek.wasm) produces the pinned output.
//   2. AGREEMENT  — cek_gc.wasm matches the reference.
//
// Agreement is the load-bearing half for GC correctness: the gc engine sees
// orders of magnitude more allocation pressure on the same programs, so
// agreement is the test that the GC walker reaches captured continuations
// properly. But agreement alone can't catch a change that breaks BOTH engines
// the same way — three of these expect `<error>`, and a regression that made
// call/cc simply stop working would agree perfectly. Hence the pinned values.
//
// The pins were captured from the suite's own output and cross-checked against
// the expected answers the programs already documented in their comments
// (107 for first-invoke-wins, 20 for the discarded `+3`, 10 for the nested
// escape, 3 for the set!-loop, <continuation> for the printer) — all matched.
//
//   node harness/parity_callcc.mjs

import { loadEngine } from './engine.mjs';

const ENGINES = ['cek.wasm', 'cek_gc.wasm'];

const PROGRAMS = [
  // identity uses — k is captured but never invoked
  ['(call/cc (lambda (k) 42))', "42"],
  ['(call/cc (lambda (k) (+ 1 2)))', "3"],
  ['(+ 10 (call/cc (lambda (k) 5)))', "15"],

  // invoke captured k synchronously — abandons the call site's continuation
  ['(call/cc (lambda (k) (k 42)))', "42"],
  ['(+ 100 (call/cc (lambda (k) (k 7))))', "107"],
  // first invoke wins; the second (k 99) is never reached
  ['(+ 100 (call/cc (lambda (k) (begin (k 7) (k 99)))))', "107"],

  // continuation invoked nested inside the call/cc lambda — the +3 is discarded
  ['(* 2 (call/cc (lambda (k) (+ 3 (k 10)))))', "20"],

  // continuation as a first-class value: stored, later invoked
  ['(begin (define escape nil) (+ 1 (call/cc (lambda (k) (begin (set! escape k) 10)))))', "11"],
  ['(begin (define escape nil) (define x (call/cc (lambda (k) (begin (set! escape k) 0)))) (if (= x 0) (escape 99) x))', "99"],

  // arity errors on call/cc itself
  ['(call/cc)', "<error>"],
  ['(call/cc (lambda (k) 1) (lambda (k) 2))', "<error>"],
  // call/cc applied to a non-function value: the cont is constructed, then
  // applying a number errors at the apply step — same as any (5 1).
  ['(call/cc 5)', "<error>"],

  // early-exit from a list walk
  [`(begin
     (define find1
       (lambda (l p)
         (call/cc (lambda (return)
           (begin
             (define iter
               (lambda (xs)
                 (cond ((null? xs) nil)
                       ((p (car xs)) (return (car xs)))
                       (else (iter (cdr xs))))))
             (iter l))))))
     (find1 (quote (1 2 3 4 5 6 7 8 9 10)) (lambda (x) (= x 7))))`, "7"],

  // call/cc nested inside call/cc — k1 escapes past k2, so the +99 is discarded
  ['(call/cc (lambda (k1) (call/cc (lambda (k2) (+ (k1 10) 99)))))', "10"],

  // continuation invoked twice via stored reference (set!-loop with a counter):
  // n goes 1 -> 2 -> 3, then the (< n 3) test fails and it exits
  [`(begin
     (define k0 nil)
     (define n 0)
     (define x (call/cc (lambda (k) (begin (set! k0 k) 0))))
     (set! n (+ n 1))
     (if (< n 3) (k0 n) n))`, "3"],

  // printer: a reified continuation has its own print form
  ['(call/cc (lambda (k) k))', "<continuation>"],
];

// Shared ABI handshake — see harness/engine.mjs.
const load = async (file) => (await loadEngine(file)).run;

const main = async () => {
  const engines = [];
  for (const f of ENGINES) engines.push([f, await load(f)]);
  const [refName, refRun] = engines[0];

  let wrong = 0, disagreed = 0;
  const label = (i, src) => `[${String(i).padStart(2)}] ${JSON.stringify(src).slice(0, 66)}`;

  PROGRAMS.forEach(([src, expected], i) => {
    const ref = refRun(src);

    // (1) VALUE. Catches a regression that breaks both engines at once, which
    // the agreement check below is structurally blind to.
    if (ref !== expected) {
      wrong++;
      console.log(`WRONG     ${label(i, src)}`);
      console.log(`  expected ${JSON.stringify(expected)}`);
      console.log(`  ${refName.padEnd(14)} => ${JSON.stringify(ref)}`);
    }

    // (2) AGREEMENT, against the reference's ACTUAL output rather than the
    // pinned one, so a shared regression reports as one WRONG instead of a
    // spurious DISAGREE alongside it.
    const diffs = [];
    for (let j = 1; j < engines.length; j++) {
      const [name, run] = engines[j];
      const got = run(src);
      if (got !== ref) diffs.push({ name, got });
    }
    if (diffs.length) {
      disagreed++;
      console.log(`DISAGREE  ${label(i, src)}`);
      console.log(`  ${refName.padEnd(14)} => ${JSON.stringify(ref)}`);
      for (const { name, got } of diffs) {
        console.log(`  ${name.padEnd(14)} => ${JSON.stringify(got)}`);
      }
    }
  });

  const n = PROGRAMS.length;
  console.log(`\n${n - wrong}/${n} call/cc programs match their pinned value`);
  console.log(`${n - disagreed}/${n} call/cc programs agree across CEK engines`);
  if (wrong || disagreed) process.exit(1);
};

main().catch(e => { console.error(e); process.exit(1); });
