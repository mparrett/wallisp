#!/usr/bin/env bash
# build.sh — compile the Lisp VM and emit a single self-contained HTML file.
#
#   ./build.sh            # builds lisp.wasm + lisp-standalone.html
#   ./build.sh --no-html  # just the .wasm
#
# Requires: clang with the wasm32 target + wasm-ld (LLVM 12+), and python3
# for the inject step (sed/awk choke on 16KB of base64 with regex specials).

set -euo pipefail
cd "$(dirname "$0")"

SRC=lisp.c
WASM=lisp.wasm
TEMPLATE=template.html
OUT=lisp-standalone.html
ARENA_MEM=33554432   # initial linear memory (bytes); arena size lives in lisp.c (MAX_CELLS)

# --- preflight -------------------------------------------------------------
command -v clang   >/dev/null || { echo "error: clang not found" >&2; exit 1; }
clang --print-targets 2>/dev/null | grep -q wasm32 \
  || { echo "error: this clang has no wasm32 target" >&2; exit 1; }

# --- compile ---------------------------------------------------------------
echo "→ compiling $SRC → $WASM"
clang --target=wasm32 -nostdlib -Oz \
  -Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined \
  -Wl,--initial-memory=$ARENA_MEM \
  -o "$WASM" "$SRC"
echo "  $(wc -c < "$WASM") bytes"

[ "${1:-}" = "--no-html" ] && { echo "done (wasm only)."; exit 0; }

# --- inject into the single-file template ----------------------------------
command -v python3 >/dev/null || { echo "error: python3 needed for inject" >&2; exit 1; }
command -v base64  >/dev/null || { echo "error: base64 needed" >&2; exit 1; }

echo "→ embedding $WASM into $OUT"
B64=$(base64 -w0 "$WASM" 2>/dev/null || base64 "$WASM" | tr -d '\n')  # -w0 is GNU; BSD fallback
WASM_B64="$B64" python3 - "$TEMPLATE" "$OUT" <<'PY'
import os, sys
template, out = sys.argv[1], sys.argv[2]
html = open(template).read()
token = "@@WASM_B64@@"
if token not in html:
    sys.exit(f"error: placeholder {token} not in {template}")
html = html.replace(token, os.environ["WASM_B64"])
open(out, "w").write(html)
print(f"  wrote {out} ({len(html)} bytes)")
PY

# --- verify the embedded module is intact ----------------------------------
if command -v node >/dev/null; then
  echo "→ verifying embedded module runs"
  node -e '
    const fs=require("fs");
    const html=fs.readFileSync(process.argv[1],"utf8");
    const m=html.match(/const WASM_B64 = "([^"]+)"/);
    if(!m){console.error("  no embedded wasm found");process.exit(1);}
    const bytes=Buffer.from(m[1],"base64");
    WebAssembly.instantiate(bytes,{}).then(({instance})=>{
      const ex=instance.exports,mem=ex.memory;
      const e=new TextEncoder().encode("(+ 1 2)");
      new Uint8Array(mem.buffer,ex.input_ptr(),e.length).set(e);
      const n=ex.eval_source(e.length);
      const r=new TextDecoder().decode(new Uint8Array(mem.buffer,ex.output_ptr(),n));
      if(r==="3"){console.log("  ok: (+ 1 2) => 3");}
      else{console.error("  FAIL: got "+r);process.exit(1);}
    }).catch(e=>{console.error(e);process.exit(1);});
  ' "$OUT"
else
  echo "  (node not found — skipping verify; output not checked)"
fi

echo "done."
