// harness/parity_strings.mjs — string parity, bytecode_gc only.
//
// EXP1 (gap_closure_plan.md): strings are implemented on bytecode_gc and
// deliberately NOT on the other seven engines. This breaks the all-engine
// parity invariant, so string programs live here — gated to bytecode_gc.
// The plan accepted the breakage upfront in exchange for testing whether
// adding a type-tagged variable-length heap raises V8's mark-sweep tax on
// gc() (it doesn't; see FINDINGS H6 — falsified in the lower window).
//
// This suite checks the engine's string semantics produce expected, stable
// answers — not cross-engine agreement, since there's nothing to compare
// against. It plays a similar role to test_bc.mjs for the bytecode VM:
// a single-engine correctness gate.
//
//   node harness/parity_strings.mjs

import fs from 'fs';

const ENGINE = 'bytecode_gc.wasm';

// [source, expected output]
const PROGRAMS = [
  // literals + round-trip via print
  ['"hello"',                                 '"hello"'],
  ['""',                                       '""'],
  ['"a b c"',                                  '"a b c"'],
  // escapes
  ['"\\""',                                    '"\\""'],
  ['"\\n"',                                    '"\\n"'],
  ['"\\t"',                                    '"\\t"'],
  ['"\\\\"',                                   '"\\\\"'],

  // string?
  ['(string? "hi")',                           't'],
  ['(string? "")',                             't'],
  ['(string? 5)',                              '()'],
  ['(string? nil)',                            '()'],
  ['(string? (quote sym))',                    '()'],
  ['(string? (cons 1 2))',                     '()'],

  // string-length
  ['(string-length "hello")',                  '5'],
  ['(string-length "")',                       '0'],
  ['(string-length "\\n\\t")',                 '2'],

  // string-ref (returns the character as a fixnum byte value)
  ['(string-ref "abc" 0)',                     '97'],
  ['(string-ref "abc" 1)',                     '98'],
  ['(string-ref "abc" 2)',                     '99'],

  // string=?
  ['(string=? "abc" "abc")',                   't'],
  ['(string=? "abc" "abd")',                   '()'],
  ['(string=? "abc" "ab")',                    '()'],
  ['(string=? "" "")',                         't'],

  // string-append
  ['(string-append "foo" "bar")',              '"foobar"'],
  ['(string-append "" "baz")',                 '"baz"'],
  ['(string-append "a" "")',                   '"a"'],
  ['(string-length (string-append "hi" " there"))', '8'],
  ['(string=? (string-append "ab" "cd") "abcd")',   't'],

  // arity and type errors
  ['(string?)',                                '<error>'],
  ['(string? "a" "b")',                        '<error>'],
  ['(string-length)',                          '<error>'],
  ['(string-length 5)',                        '<error>'],
  ['(string-length nil)',                      '<error>'],
  ['(string-ref "abc" -1)',                    '<error>'],
  ['(string-ref "abc" 3)',                     '<error>'],
  ['(string-ref 5 0)',                         '<error>'],
  ['(string=? "a" 1)',                         '<error>'],
  ['(string=? "a")',                           '<error>'],
  ['(string-append "a" 1)',                    '<error>'],
  ['(string-append "a")',                      '<error>'],

  // strings survive GC pressure: 50k iterations each allocate temporary
  // strings via append. cells go through several gc() cycles; the strheap
  // walk runs every time. Program completes correctly.
  ['(begin (define f (lambda (n) (if (= n 0) (quote done) (begin (string-append "a" "b") (f (- n 1)))))) (f 50000))', 'done'],

  // strings as constants survive — quoted-data root walk reaches them
  // through the code stream's OP_CONST operands.
  ['(begin (define greet (lambda () "hello")) (greet))', '"hello"'],
  ['(begin (define x "kept") x)',              '"kept"'],

  // string used in higher-order context
  ['(begin (define id (lambda (x) x)) (id "round-trip"))', '"round-trip"'],
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
  const run = await load(ENGINE);
  let pass = 0, fail = 0;
  for (const [src, expected] of PROGRAMS) {
    const got = run(src);
    if (got === expected) pass++;
    else {
      fail++;
      console.log(`FAIL  ${JSON.stringify(src).slice(0, 70)}`);
      console.log(`      expected ${JSON.stringify(expected)}`);
      console.log(`      got      ${JSON.stringify(got)}`);
    }
  }
  console.log(`\n${pass}/${pass + fail} string programs pass on ${ENGINE}`);
  if (fail) process.exit(1);
};

main().catch(e => { console.error(e); process.exit(1); });
