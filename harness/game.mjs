#!/usr/bin/env node
// game.mjs — B5 host game-loop driver for the wallisp persistent session.
//
//   node harness/game.mjs [game.lisp]      # default: examples/coin2d.lisp
//
// The whole TUI-game loop, host-side, over a zero-imports wasm module:
//   keypress (host owns the keyboard)  ->  eval_persistent("(turn dx dy)")
//   the game renders a frame via (display ...) into outbuf  ->  host flushes
//   it to the terminal with a cursor-home so it redraws in place.
//
// No SharedArrayBuffer / cross-origin isolation and no engine input primitive:
// nothing blocks inside the wasm, the host drives each turn. xterm.js in a
// browser is the same loop with browser key events + an xterm write() instead
// of process.stdout (see terminal_game_roadmap.md).
//
// Runs interactively in a TTY (raw mode); with piped stdin it replays the piped
// bytes as keystrokes and prints each frame — so it is testable headlessly.
//
// UNBOUNDED: the game's (tick) is compiled ONCE; each frame the host pokes the
// action into input slots (read by (input i)) and re-runs the compiled tick via
// rerun() — no per-turn recompile, so code[] never fills. With the per-frame
// strheap reset and cons-arena GC, every per-tick growth vector is bounded
// (verified at 1e6 ticks). No SharedArrayBuffer, no blocking read.

import fs from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, '..', 'bytecode_gc.wasm');
const GAME = process.argv[2] || join(HERE, '..', 'examples', 'coin2d.lisp');
const HELP = '[ wasd / arrows: move   q: quit ]';

async function load() {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(WASM), {});
  return instance.exports;
}

function makeEval(ex) {
  const enc = new TextEncoder(), dec = new TextDecoder();
  return (src) => {
    const d = enc.encode(src);
    if (d.length > 8192) throw new Error(`source too large (${d.length} > 8192)`);
    new Uint8Array(ex.memory.buffer, ex.input_ptr(), d.length).set(d);
    const n = ex.eval_persistent(d.length);
    return dec.decode(new Uint8Array(ex.memory.buffer, ex.output_ptr(), n));
  };
}

// Decode a stdin chunk into logical keys (letters as-is; ESC[A/B/C/D arrows).
function keysFromBuffer(buf) {
  const keys = [];
  for (let i = 0; i < buf.length;) {
    if (buf[i] === 0x1b && buf[i + 1] === 0x5b && i + 2 < buf.length) {
      keys.push({ A: 'up', B: 'down', C: 'right', D: 'left' }[String.fromCharCode(buf[i + 2])] || 'esc');
      i += 3;
    } else { keys.push(String.fromCharCode(buf[i])); i += 1; }
  }
  return keys;
}
const MOVES = { up: [0, -1], w: [0, -1], down: [0, 1], s: [0, 1], left: [-1, 0], a: [-1, 0], right: [1, 0], d: [1, 0] };
const isQuit = (k) => k === 'q' || k === '\x03'; // q or Ctrl-C

async function main() {
  const ex = await load();
  const evl = makeEval(ex);
  const dec = new TextDecoder();
  ex.reset_session();
  if (evl(fs.readFileSync(GAME, 'utf8')) === '<error>') { // defines (tick), renders itself, owns `base`
    console.error(`error: ${GAME} failed to load (<error>) — syntax error or arena exhaustion in the game file.`);
    process.exit(1);
  }

  // Unbounded path: compile (tick) once, then drive it with rerun() + input slots.
  if (evl('(tick)') === '<error>') {
    console.error(`error: first (tick) returned <error> — ${GAME} defines no working (tick).`);
    process.exit(1);
  }
  const entry = ex.last_entry();
  const turn = (dx, dy) => {
    const slots = new Int32Array(ex.memory.buffer, ex.input_slots_ptr(), 8); // refetch: memory may move
    slots[0] = dx; slots[1] = dy;
    const n = ex.rerun(entry);
    return dec.decode(new Uint8Array(ex.memory.buffer, ex.output_ptr(), n));
  };

  if (!process.stdout.isTTY || !process.stdin.isTTY) {
    // Headless: replay piped bytes as keystrokes, print each frame.
    const chunks = [];
    for await (const c of process.stdin) chunks.push(c);
    const keys = keysFromBuffer(Buffer.concat(chunks));
    process.stdout.write('initial:\n' + turn(0, 0) + '\n');
    for (const k of keys) {
      if (isQuit(k)) { process.stdout.write('\n[quit]\n'); break; }
      const mv = MOVES[k];
      if (!mv) continue;
      const f = turn(...mv);
      process.stdout.write(`\nkey ${k}:\n${f}\n`);
      if (f === '<error>') { process.stdout.write('[tick error]\n'); break; }
    }
    return;
  }

  // Interactive TTY: alt screen, hidden cursor, raw keys, redraw in place.
  const enter = () => process.stdout.write('\x1b[?1049h\x1b[?25l\x1b[2J');
  const leave = () => process.stdout.write('\x1b[?25h\x1b[?1049l');
  const draw = (f) => process.stdout.write(`\x1b[H${f}\n${HELP}`);
  const quit = (msg) => { leave(); if (msg) process.stdout.write(msg + '\n'); process.exit(0); };

  enter();
  draw(turn(0, 0));
  process.stdin.setRawMode(true);
  process.stdin.resume();
  process.on('SIGINT', () => quit());
  process.stdin.on('data', (buf) => {
    for (const k of keysFromBuffer(buf)) {
      if (isQuit(k)) return quit();
      const mv = MOVES[k];
      if (!mv) continue;
      const f = turn(...mv);
      if (f === '<error>') return quit('tick returned <error>.');
      draw(f);
    }
  });
}

main().catch(e => { process.stdout.write('\x1b[?25h\x1b[?1049l'); console.error('game:', e.message); process.exit(1); });
