#!/usr/bin/env bash
# build-game.sh — generate the self-contained web/game.html from its template by
# inlining the current bytecode_gc.wasm (base64) and examples/coin2d.lisp. Run
# whenever the engine or the game source changes:
#
#   bash web/build-game.sh
#
# The xterm.js library is loaded from a CDN; everything else is embedded, so the
# output opens straight from file:// (or any static host).
set -euo pipefail
cd "$(dirname "$0")/.."

TEMPLATE=web/game-template.html
OUT=web/game.html
WASM=bytecode_gc.wasm
LISP=examples/coin2d.lisp

[ -f "$TEMPLATE" ] || { echo "error: $TEMPLATE not found" >&2; exit 1; }
[ -f "$WASM" ]     || { echo "error: $WASM not found (run build.sh)" >&2; exit 1; }
[ -f "$LISP" ]     || { echo "error: $LISP not found" >&2; exit 1; }

B64=$(base64 -w0 "$WASM" 2>/dev/null || base64 -i "$WASM" | tr -d '\n')

WASM_B64="$B64" python3 - "$TEMPLATE" "$OUT" "$LISP" <<'PY'
import os, sys, json
tpl, out, lisp = sys.argv[1], sys.argv[2], sys.argv[3]
html = open(tpl).read()
html = html.replace("@@WASM_B64@@", os.environ["WASM_B64"])
html = html.replace("@@GAME_LISP@@", json.dumps(open(lisp).read()))
open(out, "w").write(html)
print(f"wrote {out} ({len(html)} bytes)")
PY

# Smoke-test: the embedded wasm must expose the unbounded-play exports and a
# compiled (tick) must rerun and render a frame.
node - "$OUT" <<'NODE'
import('fs').then(async ({ default: fs }) => {
  const html = fs.readFileSync(process.argv[2], 'utf8');
  const b64 = html.match(/const WASM_B64 = "([^"]+)"/)[1];
  const lisp = JSON.parse(html.match(/const GAME_LISP = (".*?");\n/s)[1]);
  const { instance: { exports: ex } } = await WebAssembly.instantiate(Buffer.from(b64, 'base64'), {});
  for (const f of ['rerun', 'input_slots_ptr', 'last_entry', 'eval_persistent'])
    if (typeof ex[f] !== 'function') throw new Error('missing export ' + f);
  const enc = new TextEncoder(), dec = new TextDecoder();
  const evalP = s => { const b = enc.encode(s); new Uint8Array(ex.memory.buffer, ex.input_ptr(), b.length).set(b); return dec.decode(new Uint8Array(ex.memory.buffer, ex.output_ptr(), ex.eval_persistent(b.length))); };
  ex.reset_session(); evalP(lisp); evalP('(tick)');
  const entry = ex.last_entry();
  const slots = new Int32Array(ex.memory.buffer, ex.input_slots_ptr(), 8); slots[0] = 1; slots[1] = 0;
  const frame = dec.decode(new Uint8Array(ex.memory.buffer, ex.output_ptr(), ex.rerun(entry)));
  if (!frame.includes('@') || !frame.includes('score:')) throw new Error('frame did not render: ' + JSON.stringify(frame));
  console.log('  smoke ok: rerun renders a frame with @ and score');
});
NODE
echo "done."
