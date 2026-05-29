import fs from 'fs';

async function main() {
  const bytes = fs.readFileSync(new URL('../bytecode.wasm', import.meta.url));
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  const mem = ex.memory;

  function run(src) {
    const enc = new TextEncoder().encode(src);
    const inPtr = ex.input_ptr();
    new Uint8Array(mem.buffer, inPtr, enc.length).set(enc);
    const outLen = ex.eval_source(enc.length);
    const outPtr = ex.output_ptr();
    return new TextDecoder().decode(new Uint8Array(mem.buffer, outPtr, outLen));
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
  ];

  let pass = 0, fail = 0;
  for (const [src, want] of tests) {
    const got = run(src);
    const ok = got === want;
    if (ok) pass++; else fail++;
    console.log(`${ok ? "PASS" : "FAIL"}  ${JSON.stringify(src).slice(0,52).padEnd(54)} => ${JSON.stringify(got)}${ok?"":"   expected "+JSON.stringify(want)}`);
  }
  console.log(`\n${pass}/${pass+fail} passed`);

  // fibonacci as a heavier check
  const fib = "(begin (define fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))) (fib 20))";
  console.log("\nfib(20) =", run(fib), "(expect 6765)");
}
main().catch(e => { console.error(e); process.exit(1); });
