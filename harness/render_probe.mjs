#!/usr/bin/env node
// render_probe.mjs — render slice demo + the string-heap leak, before and after
// the per-frame region reset.
//
//   node harness/render_probe.mjs
//
// Over bytecode_gc.wasm's persistent session:
//   1. Capability — render the coin game as a raw text grid via (display ...).
//   2. Leak — drive frame-building turns with NO reset; strheap grows ~128
//      bytes/turn, gc_count never moves, exhausts the 1 MB heap (~8200 turns).
//   3. Fix — bracket each frame with (strheap-mark)/(strheap-reset base): the
//      heap stays FLAT across 100k frames. base is captured AFTER the defs, so
//      the render functions' string literals (below base) are preserved while
//      per-frame scratch (above base) is dropped in O(1).
//
// Drivers recurse inside ONE eval (flat via TCO) so per-turn work doesn't
// recompile — isolating strheap behaviour from the code[]-append limit.

import fs from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, '..', 'bytecode_gc.wasm');
const STR_HEAP_BYTES = 1048576; // mirror of STR_HEAP_BYTES in bytecode_gc.c

// Coin-collector + grid renderer. `base` is captured AFTER every defn, so the
// "@"/"o"/"."/"\n" literals are below it. drive-leak builds a frame per turn and
// keeps it; drive-region drops back to base each turn.
const DEFS = `
(define (rng s) (mod (* (+ s 1) 75) 65537))
(define (mk a b c d) (cons a (cons b (cons c (cons d ())))))
(define (s-pos st) (car st))
(define (s-coin st) (car (cdr st)))
(define (s-score st) (car (cdr (cdr st))))
(define (s-seed st) (car (cdr (cdr (cdr st)))))
(define (clamp p) (cond ((< p 0) 0) ((< 9 p) 9) (else p)))
(define (respawn np score ns) (mk np (mod ns 10) score ns))
(define (place st np) (if (= np (s-coin st)) (respawn np (+ (s-score st) 1) (rng (s-seed st))) (mk np (s-coin st) (s-score st) (s-seed st))))
(define (step st mv) (place st (clamp (+ (s-pos st) mv))))
(define (cell i p c) (cond ((= i p) "@") ((= i c) "o") (else ".")))
(define (row i w p c) (if (= i w) "" (string-append (cell i p c) (row (+ i 1) w p c))))
(define (frame st) (string-append (row 0 10 (s-pos st) (s-coin st)) "\n"))
(define st (mk 0 (mod 12345 10) 0 12345))
(define (view mv) (begin (set! st (step st mv)) (display (frame st))))
(define base (strheap-mark))
(define (drive-leak n mv) (if (= n 0) st (begin (set! st (step st mv)) (begin (frame st) (drive-leak (- n 1) mv)))))
(define (drive-region n mv) (if (= n 0) st (begin (set! st (step st mv)) (begin (frame st) (begin (strheap-reset base) (drive-region (- n 1) mv))))))
`;

async function main() {
  const { instance: { exports: ex } } = await WebAssembly.instantiate(fs.readFileSync(WASM), {});
  const enc = new TextEncoder(), dec = new TextDecoder();
  const evl = (s) => {
    const d = enc.encode(s);
    if (d.length > 8192) throw new Error(`program too large (${d.length} > 8192)`);
    new Uint8Array(ex.memory.buffer, ex.input_ptr(), d.length).set(d);
    const n = ex.eval_persistent(d.length);
    return dec.decode(new Uint8Array(ex.memory.buffer, ex.output_ptr(), n));
  };
  const setup = () => { ex.reset_session(); evl(DEFS); };

  // 1. Capability: render a few turns as a grid.
  setup();
  console.log('=== rendered grid (@ player, o coin) — right ×6, left ×3 ===');
  for (const mv of [1, 1, 1, 1, 1, 1, -1, -1, -1]) process.stdout.write(evl(`(view ${mv})`));
  console.log(`score: ${evl('(s-score st)')}`);

  // 2. Leak: drive frame-building turns with no reset.
  setup();
  console.log('\n=== WITHOUT region reset: strheap leaks ===');
  console.log('  turns   strheap_used   gc_count');
  const K = 200;
  let turns = 0;
  while (turns < 9000) {
    const out = evl(`(drive-leak ${K} 1)`);
    turns += K;
    if (out === '<error>') { console.log(`  ${String(turns).padStart(5)}    exhausted -> <error> (1 MB heap full)`); break; }
    if (turns % 2000 === 0) console.log(`  ${String(turns).padStart(5)}    ${String(ex.strheap_used()).padStart(10)}    ${ex.gc_count()}`);
  }

  // 3. Fix: bracket each frame with mark/reset.
  setup();
  console.log('\n=== WITH region reset: strheap stays flat ===');
  const base = parseInt(evl('base'), 10);
  console.log(`  base (literals below) = ${base} bytes`);
  console.log('  turns   strheap_used   gc_count');
  for (let t = 20000; t <= 100000; t += 20000) {
    evl(`(drive-region 20000 1)`);
    console.log(`  ${String(t).padStart(6)}    ${String(ex.strheap_used()).padStart(10)}    ${ex.gc_count()}`);
  }
  const finalUsed = ex.strheap_used();
  console.log(`\nresult: 100k frames rendered, strheap_used=${finalUsed} (== base ${base}: ${finalUsed === base}),`);
  console.log(`        zero growth. Per-frame region reset fixes the B4 leak for transient frames.`);
}

main().catch(e => { console.error('render_probe:', e.message); process.exit(1); });
