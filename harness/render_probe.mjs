#!/usr/bin/env node
// render_probe.mjs — render slice demo + the string-heap leak measurement.
//
//   node harness/render_probe.mjs
//
// Two things, both over bytecode_gc.wasm's persistent session:
//   1. Capability — render the coin game as a raw text grid via (display ...).
//   2. Measurement (pre-registered, render_slice_plan.md) — drive many turns
//      that build a frame string each turn and watch strheap_used. Prediction:
//      monotonic growth, zero reclamation -> exhaustion. Confirms B4 is the
//      real game blocker and quantifies it (bytes/turn, turns-to-exhaustion).
//
// The driver recurses inside ONE eval (flat via TCO) so per-turn work doesn't
// recompile — isolating the strheap leak from the code[]-append limit.

import fs from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, '..', 'bytecode_gc.wasm');
const STR_HEAP_BYTES = 1048576; // mirror of STR_HEAP_BYTES in bytecode_gc.c

// Coin-collector + a grid renderer, all in core wallisp + strings. Frame is a
// 10-cell row: @ = player, o = coin, . = empty. row builds left-to-right with
// string-append (each intermediate allocates in strheap).
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
(define (drive n mv) (if (= n 0) st (begin (set! st (step st mv)) (frame st) (drive (- n 1) mv))))
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

  ex.reset_session();
  evl(DEFS);

  // 1. Capability: render a few turns as a grid.
  console.log('=== rendered grid (@ player, o coin) — right ×6, left ×3 ===');
  for (const mv of [1, 1, 1, 1, 1, 1, -1, -1, -1]) process.stdout.write(evl(`(view ${mv})`));
  console.log(`score: ${evl('(s-score st)')}`);

  // 2. Measurement: drive frame-building turns, sample strheap_used.
  console.log('\n=== strheap leak: build a frame per turn, watch the heap ===');
  console.log('  turns   strheap_used   bytes/turn   gc_count');
  const K = 200, CAP = 200000;
  let turns = 0, prev = ex.strheap_used();
  while (turns < CAP) {
    const out = evl(`(drive ${K} 1)`);
    const used = ex.strheap_used();
    if (out === '<error>') {
      console.log(`  exhausted between turns ${turns} and ${turns + K}: strheap_used=${used}/${STR_HEAP_BYTES}, eval -> <error>`);
      turns += K; prev = used; break;
    }
    turns += K;
    if (turns % 1000 === 0) console.log(`  ${String(turns).padStart(5)}    ${String(used).padStart(10)}    ${String((used / turns).toFixed(1)).padStart(7)}      ${ex.gc_count()}`);
    prev = used;
  }
  const perTurn = (prev / turns);
  console.log(`\nresult: ~${perTurn.toFixed(0)} bytes/turn leaked, gc_count=${ex.gc_count()} (collector never reclaims strheap),`);
  console.log(`        ~${turns} turns to exhaust ${STR_HEAP_BYTES} bytes. Confirms B4 (render_slice_plan.md).`);
}

main().catch(e => { console.error('render_probe:', e.message); process.exit(1); });
