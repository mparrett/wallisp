#!/usr/bin/env bash
# refresh-tiny-lisp-vm.sh — rebuild the three arena-tuned bytecode_gc variants
# embedded in tiny-lisp-vm.html. The HTML's BLOBS object is keyed by arena
# size (512, 8192, 65536); each blob is the current `engines/bytecode_gc.c`
# built with MAX_CELLS set to that key. Run this whenever the engine changes
# (e.g., after a PR adds primitives).
#
#   bash web/refresh-tiny-lisp-vm.sh
set -euo pipefail
cd "$(dirname "$0")/.."

HTML=web/tiny-lisp-vm.html
SRC=engines/bytecode_gc.c
[ -f "$HTML" ] || { echo "error: $HTML not found" >&2; exit 1; }
[ -f "$SRC"  ] || { echo "error: $SRC not found"  >&2; exit 1; }

command -v clang   >/dev/null || { echo "error: clang not found" >&2; exit 1; }
command -v python3 >/dev/null || { echo "error: python3 needed for injection" >&2; exit 1; }
clang --print-targets 2>/dev/null | grep -q wasm32 \
  || { echo "error: this clang has no wasm32 target" >&2; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

build_one () {  # arena -> echo b64
  local ARENA=$1
  local C=$WORK/bc_gc_${ARENA}.c
  local W=$WORK/bc_gc_${ARENA}.wasm
  sed "s/^#define MAX_CELLS .*/#define MAX_CELLS ${ARENA}/" "$SRC" > "$C"
  echo "→ building arena=${ARENA}" >&2
  clang --target=wasm32 -nostdlib -fno-builtin -Oz \
    -Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined \
    -Wl,--initial-memory=33554432 \
    -o "$W" "$C"
  echo "  $(wc -c < "$W") bytes wasm" >&2
  base64 -w0 "$W" 2>/dev/null || base64 -i "$W" | tr -d '\n'
}

B512=$(build_one 512)
B8192=$(build_one 8192)
B65536=$(build_one 65536)

# Inject into the HTML, replacing each <KEY>:"…" entry inside the BLOBS object.
echo "→ rewriting BLOBS in $HTML"
B512="$B512" B8192="$B8192" B65536="$B65536" python3 - "$HTML" <<'PY'
import os, re, sys
path = sys.argv[1]
html = open(path).read()
blobs = {512: os.environ["B512"], 8192: os.environ["B8192"], 65536: os.environ["B65536"]}
m = re.search(r"const BLOBS = \{(.*?)\};", html, re.S)
if not m: sys.exit("error: BLOBS object not found")
block = m.group(1)
for k, v in blobs.items():
    new_block, n = re.subn(rf'({k}\s*:\s*)"[^"]*"', f'\\1"{v}"', block, count=1)
    if n != 1: sys.exit(f"error: did not replace key {k} (replaced {n} times)")
    block = new_block
new_html = html.replace(m.group(0), f"const BLOBS = {{{block}}};")
open(path, "w").write(new_html)
print(f"  wrote {path} ({len(new_html)} bytes)")
PY

# Verify each refreshed blob actually loads and runs the new primitives.
echo "→ smoke-testing each blob"
node - "$HTML" <<'NODE'
import('fs').then(async ({default: fs}) => {
  const path = process.argv[2];
  const html = fs.readFileSync(path, 'utf8');
  const m = html.match(/const BLOBS = \{(.*?)\};/s);
  if (!m) { console.error('BLOBS not found'); process.exit(1); }
  const blobs = {};
  for (const e of m[1].matchAll(/(\d+)\s*:\s*"([^"]+)"/g)) blobs[e[1]] = e[2];
  const probes = ["(number? 5)", "(symbol? 'foo)", "(+ 1 2)"];
  for (const [k, b64] of Object.entries(blobs)) {
    const bytes = Buffer.from(b64, 'base64');
    const { instance } = await WebAssembly.instantiate(bytes, {});
    const ex = instance.exports, mem = ex.memory;
    const results = probes.map(src => {
      const e = new TextEncoder().encode(src);
      new Uint8Array(mem.buffer, ex.input_ptr(), e.length).set(e);
      const n = ex.eval_source(e.length);
      return new TextDecoder().decode(new Uint8Array(mem.buffer, ex.output_ptr(), n));
    });
    console.log(`  arena=${k}: ${probes.map((p,i) => p+'=>'+results[i]).join('  ')}`);
  }
});
NODE
echo "done."
