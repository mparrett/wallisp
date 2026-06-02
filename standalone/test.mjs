#!/usr/bin/env node
// test.mjs — conformance suite that defines wallisp's language contract.
//
// Run: node test.mjs   (after ./build.sh has produced wallisp.wasm)
//
// Each entry is [source, expected-printed-result]. The suite is focused, not
// exhaustive — it pins down the supported surface; anything outside it is
// undefined.

import fs from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, 'wallisp.wasm');

const bytes = fs.readFileSync(WASM);
const { instance } = await WebAssembly.instantiate(bytes, {});
const ex = instance.exports;
const mem = () => new Uint8Array(ex.memory.buffer);
function run(src) {
  const data = new TextEncoder().encode(src);
  mem().set(data, ex.input_ptr());
  const n = ex.eval_source(data.length);
  return new TextDecoder().decode(mem().subarray(ex.output_ptr(), ex.output_ptr() + n));
}

const tests = [
  // Arithmetic + comparison
  ["(+ 1 2)", "3"],
  ["(+ 1 2 3 4 5)", "15"],
  ["(- 10 3)", "7"],
  ["(- 5 8)", "-3"],
  ["(* 6 7)", "42"],
  ["(/ 22 7)", "3"],
  ["(mod 22 7)", "1"],
  ["(= 3 3)", "t"],
  ["(< 4 4)", "()"],
  ["(< 3 4)", "t"],

  // Lists
  ["(cons 1 2)", "(1 . 2)"],
  ["(cons 1 (cons 2 (cons 3 nil)))", "(1 2 3)"],
  ["(car '(a b c))", "a"],
  ["(cdr '(a b c))", "(b c)"],
  ["'(1 2 3)", "(1 2 3)"],
  ["(null? nil)", "t"],
  ["(null? '(1))", "()"],
  ["(pair? '(1 2))", "t"],
  ["(list? nil)", "t"],

  // Special forms
  ["(if (< 1 2) 'yes 'no)", "yes"],
  ["(if (< 2 1) 'yes 'no)", "no"],
  ["(let ((x 5) (y 7)) (+ x y))", "12"],
  ["(begin 1 2 3)", "3"],
  ["(cond ((< 1 2) 'a) (else 'b))", "a"],
  ["(cond ((= 1 2) 'a) ((= 3 4) 'b))", "()"],
  ["(cond)", "()"],

  // Lambdas and closures
  ["((lambda (x) (* x x)) 9)", "81"],
  ["(begin (define add (lambda (a) (lambda (b) (+ a b)))) ((add 3) 4))", "7"],
  ["(begin (define (sq x) (* x x)) (sq 9))", "81"],

  // Recursion
  ["(begin (define (fact n) (if (< n 1) 1 (* n (fact (- n 1))))) (fact 5))", "120"],
  ["(begin (define (len l) (if (null? l) 0 (+ 1 (len (cdr l))))) (len '(a b c d)))", "4"],

  // Mutation
  ["(begin (define x 1) (set! x 42) x)", "42"],
  ["(begin (define p (cons 1 2)) (set-car! p 9) p)", "(9 . 2)"],
  ["(begin (define p (cons 1 2)) (set-cdr! p 9) p)", "(1 . 9)"],

  // Comments + whitespace
  ["; comment line\n(+ 2 40) ; trailing", "42"],

  // Primitive rebinding still works through the inline fast path
  ["(begin (define + (lambda (a b) 99)) (+ 1 2))", "99"],

  // Arity is checked
  ["((lambda (x y) x) 1)", "<error>"],
  ["((lambda (x) x) 1 2)", "<error>"],
  ["(+ 1)", "<error>"],

  // Types are checked
  ["(+ 'a 1)", "<error>"],
  ["(< 'a 1)", "<error>"],
  ["(car 5)", "<error>"],

  // Division-by-zero and overflow are errors
  ["(/ 1 0)", "<error>"],
  ["(mod 1 0)", "<error>"],
  ["(* 536870912 2)", "<error>"],

  // TCO: a non-trivial tail loop completes in a small arena without blowing
  // the call stack or the cell arena.
  ["(begin (define (loop n) (if (= n 0) 'done (loop (- n 1)))) (loop 100000))", "done"],
];

let pass = 0, fail = 0;
for (const [src, want] of tests) {
  const got = run(src);
  const ok = got === want;
  if (ok) pass++; else fail++;
  const label = JSON.stringify(src).slice(0, 60).padEnd(62);
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${label} => ${JSON.stringify(got)}${ok ? '' : '   expected ' + JSON.stringify(want)}`);
}
console.log(`\n${pass}/${pass + fail} passed`);
process.exit(fail === 0 ? 0 : 1);
