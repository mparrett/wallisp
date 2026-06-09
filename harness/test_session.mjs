#!/usr/bin/env node
// test_session.mjs — persistent-session contract for bytecode_gc.wasm
// (Milestone A). Asserts that eval_persistent keeps state across calls, that
// closures defined on earlier lines stay valid (the cp-append invariant), that
// reset_session clears it, and that eval_source is unchanged and stateless.
//
//   node harness/test_session.mjs

import fs from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, '..', 'bytecode_gc.wasm');

function makeEval(ex) {
  const enc = new TextEncoder(), dec = new TextDecoder();
  return (fn, src) => {
    const d = enc.encode(src);
    new Uint8Array(ex.memory.buffer, ex.input_ptr(), d.length).set(d);
    const n = ex[fn](d.length);
    return dec.decode(new Uint8Array(ex.memory.buffer, ex.output_ptr(), n));
  };
}

let pass = 0, fail = 0;
function check(name, got, exp) {
  if (got === exp) { pass++; }
  else { fail++; console.error(`FAIL  ${name}: got ${JSON.stringify(got)} want ${JSON.stringify(exp)}`); }
}

async function main() {
  const { instance: { exports: ex } } = await WebAssembly.instantiate(fs.readFileSync(WASM), {});
  const evl = makeEval(ex);

  // A fresh instance must expose the session exports.
  for (const f of ['reset_session', 'eval_persistent', 'eval_source'])
    check(`export ${f}`, typeof ex[f], 'function');

  // --- persistent session: values and closures survive across calls ---
  ex.reset_session();
  check('define x', evl('eval_persistent', '(define x 5)'), 'x');
  check('read x later', evl('eval_persistent', '(+ x 1)'), '6');
  // closure capturing a global, *defined on its own line*, then called on a
  // later line — this is the case the cp-append fix exists for (a naive
  // cp=0-per-call reset clobbers f's body and this returns <error>).
  check('define f', evl('eval_persistent', '(define (f n) (* n x))'), 'f');
  check('call f', evl('eval_persistent', '(f 10)'), '50');
  // closure that calls another earlier closure
  check('define g', evl('eval_persistent', '(define (g a) (f (+ a 1)))'), 'g');
  check('call g', evl('eval_persistent', '(g 9)'), '50');
  // self-recursion defined on one line, exercised on the next
  check('define fib', evl('eval_persistent', '(define (fib n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))'), 'fib');
  check('call fib', evl('eval_persistent', '(fib 12)'), '144');
  // redefinition takes effect
  check('redefine x', evl('eval_persistent', '(define x 100)'), 'x');
  check('f sees new x', evl('eval_persistent', '(f 2)'), '200');

  // --- display: raw frame output (render slice) ---
  ex.reset_session();
  check('display raw, no quotes', evl('eval_persistent', '(display "hi")'), 'hi');
  check('display suppresses value echo', evl('eval_persistent', '(begin (display "x") 42)'), 'x');
  check('display newline passes through', evl('eval_persistent', '(display (string-append "a" "\\nb"))'), 'a\nb');
  check('non-string display errors', evl('eval_persistent', '(display 5)'), '<error>');
  check('plain value still echoes', evl('eval_persistent', '(+ 1 2)'), '3');
  check('normal string still quoted', evl('eval_persistent', '(string-append "a" "b")'), '"ab"');

  // --- strheap region drop: (strheap-mark) / (strheap-reset m) ---
  ex.reset_session();
  check('strheap-mark is a fixnum', evl('eval_persistent', '(number? (strheap-mark))'), 't');
  // reset reclaims: used returns to the mark after allocating + dropping
  evl('eval_persistent', '(define m (strheap-mark))');
  const at = ex.strheap_used();
  evl('eval_persistent', '(string-append "aaaa" "bbbb")');
  check('alloc grows strheap', ex.strheap_used() > at, true);
  evl('eval_persistent', '(strheap-reset m)');
  check('reset reclaims to mark', ex.strheap_used(), at);
  // the correctness trap: a literal allocated BELOW the mark survives a reset
  // followed by reallocation over the dropped region.
  ex.reset_session();
  evl('eval_persistent', '(define (tag) "HELLO")');     // literal below the mark
  evl('eval_persistent', '(define base (strheap-mark))');
  evl('eval_persistent', '(string-append "junk" "junk2")');
  evl('eval_persistent', '(strheap-reset base)');
  evl('eval_persistent', '(string-append "over" "write")'); // realloc over dropped region
  check('literal below mark survives reset', evl('eval_persistent', '(tag)'), '"HELLO"');
  // guard rails
  check('strheap-reset rejects non-fixnum', evl('eval_persistent', '(strheap-reset (quote a))'), '<error>');
  check('strheap-reset rejects past top', evl('eval_persistent', '(strheap-reset 9999999)'), '<error>');

  // --- reset_session clears the session ---
  ex.reset_session();
  check('x gone after reset', evl('eval_persistent', 'x'), '<error>');

  // --- eval_source is unchanged: stateless, one-shot ---
  check('eval_source value', evl('eval_source', '(+ 2 3)'), '5');
  check('eval_source define', evl('eval_source', '(define y 9)'), 'y');
  check('eval_source no carry', evl('eval_source', 'y'), '<error>');
  // multi-form program in a single eval_source still works (unchanged path)
  check('eval_source multiform', evl('eval_source', '(define z 7) (* z z)'), '49');

  console.log(`${pass} passed, ${fail} failed`);
  process.exit(fail ? 1 : 0);
}

main().catch(e => { console.error('test_session:', e.message); process.exit(1); });
