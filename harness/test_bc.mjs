import fs from 'fs';

async function loadEngine(wasmName) {
  const bytes = fs.readFileSync(new URL(`../${wasmName}`, import.meta.url));
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  const mem = ex.memory;
  return (src) => {
    const enc = new TextEncoder().encode(src);
    new Uint8Array(mem.buffer, ex.input_ptr(), enc.length).set(enc);
    const outLen = ex.eval_source(enc.length);
    return new TextDecoder().decode(new Uint8Array(mem.buffer, ex.output_ptr(), outLen));
  };
}

const tests = [
  ["(+ 1 2)", "3"],
  ["(* 6 7)", "42"],
  ["(- 10 3 )", "7"],
  ["(cons 1 (cons 2 (cons 3 nil)))", "(1 2 3)"],
  ["(car (cons 9 8))", "9"],
  ["(cdr '(a b c))", "(b c)"],
  ["'(1 2 3)", "(1 2 3)"],
  ["(if (< 1 2) 'yes 'no)", "yes"],
  ["(if (< 2 1) 'yes 'no)", "no"],
  ["(let ((x 5) (y 7)) (+ x y))", "12"],
  ["((lambda (x) (* x x)) 9)", "81"],
  // closures capture env:
  ["(begin (define add (lambda (a) (lambda (b) (+ a b)))) ((add 3) 4))", "7"],
  // recursion via define + if:
  ["(begin (define fact (lambda (n) (if (< n 1) 1 (* n (fact (- n 1)))))) (fact 5))", "120"],
  ["(begin (define len (lambda (l) (if (null? l) 0 (+ 1 (len (cdr l)))))) (len '(a b c d)))", "4"],
  ["(null? nil)", "t"],
  ["(null? '(1))", "()"],
  ["(= 3 3)", "t"],
  ["(pair? '(1 2))", "t"],
  // comment handling:
  ["; comment line\n(+ 2 40) ; trailing", "42"],
  // inline fast-path edges (PR_ADD/PR_SUB/PR_MUL/PR_EQ/PR_LT via 2-arg calls):
  ["(- 5 8)", "-3"],
  ["(= 4 4)", "t"],
  ["(< 4 4)", "()"],
  // CRITICAL: rebinding a primitive must still take effect through the inline path.
  // A bug that switched on a compile-time id would answer 3 and break redefinition.
  ["(begin (define + (lambda (a b) 99)) (+ 1 2))", "99"],

  // arity check: under- and over-supply must error, not silently NIL-pad or drop.
  ["((lambda (x y) x) 1)",       "<error>"],
  ["((lambda (x) x) 1 2)",       "<error>"],
  ["((lambda (x y) (+ x y)) 1 2)", "3"],
  // tail-call arity is checked too:
  ["(begin (define f (lambda (n) (if (= n 0) 'done (f)))) (f 3))", "<error>"],

  // define-form shorthand: (define (name args...) body)
  ["(begin (define (sq x) (* x x)) (sq 9))", "81"],
  ["(begin (define (add a b) (+ a b)) (add 3 4))", "7"],
  ["(begin (define (fact n) (if (< n 1) 1 (* n (fact (- n 1))))) (fact 5))", "120"],

  // cond: clause walk, else fallback, no-else fallthrough.
  ["(cond ((< 1 2) 'a) (else 'b))",                    "a"],
  ["(cond ((= 1 2) 'a) ((= 3 3) 'b) (else 'c))",       "b"],
  ["(cond ((= 1 2) 'a) ((= 3 4) 'b))",                 "()"],
  ["(cond)",                                            "()"],
  ["(begin (define (sgn n) (cond ((< n 0) -1) ((< 0 n) 1) (else 0))) (cons (sgn -7) (cons (sgn 0) (cons (sgn 9) nil))))", "(-1 0 1)"],
];

async function main() {
  let total = 0, totalFail = 0;
  for (const wasmName of ['bytecode.wasm', 'bytecode_gc.wasm']) {
    console.log(`\n=== ${wasmName} ===`);
    const run = await loadEngine(wasmName);
    let pass = 0, fail = 0;
    for (const [src, want] of tests) {
      const got = run(src);
      const ok = got === want;
      if (ok) pass++; else fail++;
      console.log(`${ok ? "PASS" : "FAIL"}  ${JSON.stringify(src).slice(0,52).padEnd(54)} => ${JSON.stringify(got)}${ok?"":"   expected "+JSON.stringify(want)}`);
    }
    console.log(`${pass}/${pass+fail} passed`);
    total += pass + fail; totalFail += fail;

    // fibonacci as a heavier check
    const fib = "(begin (define fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))) (fib 20))";
    console.log(`fib(20) = ${run(fib)} (expect 6765)`);
  }
  console.log(`\nOVERALL: ${total-totalFail}/${total} passed`);
  if (totalFail) process.exit(1);
}
main().catch(e => { console.error(e); process.exit(1); });
