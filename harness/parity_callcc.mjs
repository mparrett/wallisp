// harness/parity_callcc.mjs — call/cc parity, CEK engines only.
//
// EXP2 (gap-closure plan): call/cc is implemented on the two CEK engines and
// deliberately NOT bound on the other six. Programs that reference call/cc
// error on the non-CEK engines, so they can't live in the unified parity
// suite — they go here. We assert cek.wasm and cek_gc.wasm produce identical
// output on each program. The gc engine sees orders of magnitude more
// allocation pressure on the same programs; agreement is the test that the
// GC walker reaches captured continuations properly.
//
//   node harness/parity_callcc.mjs

import fs from 'fs';

const ENGINES = ['cek.wasm', 'cek_gc.wasm'];

const PROGRAMS = [
  // identity uses — k is captured but never invoked
  '(call/cc (lambda (k) 42))',
  '(call/cc (lambda (k) (+ 1 2)))',
  '(+ 10 (call/cc (lambda (k) 5)))',

  // invoke captured k synchronously — abandons the call site's continuation
  '(call/cc (lambda (k) (k 42)))',
  '(+ 100 (call/cc (lambda (k) (k 7))))',                       // → 107
  '(+ 100 (call/cc (lambda (k) (begin (k 7) (k 99)))))',         // first invoke wins → 107

  // continuation invoked nested inside the call/cc lambda
  '(* 2 (call/cc (lambda (k) (+ 3 (k 10)))))',                   // → 20 (+3 is discarded)

  // continuation as a first-class value: stored, later invoked
  '(begin (define escape nil) (+ 1 (call/cc (lambda (k) (begin (set! escape k) 10)))))',
  '(begin (define escape nil) (define x (call/cc (lambda (k) (begin (set! escape k) 0)))) (if (= x 0) (escape 99) x))',

  // arity errors on call/cc itself
  '(call/cc)',
  '(call/cc (lambda (k) 1) (lambda (k) 2))',
  // call/cc applied to a non-function value: the cont is constructed, then
  // applying a number errors at the apply step — same as any (5 1).
  '(call/cc 5)',

  // early-exit from a list walk
  `(begin
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
     (find1 (quote (1 2 3 4 5 6 7 8 9 10)) (lambda (x) (= x 7))))`,

  // call/cc nested inside call/cc
  '(call/cc (lambda (k1) (call/cc (lambda (k2) (+ (k1 10) 99)))))',  // → 10

  // continuation invoked twice via stored reference (set!-loop with a counter)
  `(begin
     (define k0 nil)
     (define n 0)
     (define x (call/cc (lambda (k) (begin (set! k0 k) 0))))
     (set! n (+ n 1))
     (if (< n 3) (k0 n) n))`,                                    // → 3 (n=1→2→3, then exits)

  // printer
  '(call/cc (lambda (k) k))',                                   // → <continuation>
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
      console.log(`  ${refName.padEnd(14)} => ${JSON.stringify(expected)}`);
      for (const { name, got } of diffs) {
        console.log(`  ${name.padEnd(14)} => ${JSON.stringify(got)}`);
      }
    }
  }
  console.log(`\n${programs - failures}/${programs} programs agree across CEK engines (call/cc)`);
  if (failures) process.exit(1);
};

main().catch(e => { console.error(e); process.exit(1); });
