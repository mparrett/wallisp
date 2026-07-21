#!/usr/bin/env bash
# build.sh — compile wallisp.c to freestanding wasm32 (zero imports).
#
#   ./build.sh           # builds wallisp.wasm at -Oz
#   ./build.sh --check   # also runs node test.mjs against the build

set -euo pipefail
cd "$(dirname "$0")"

SRC=wallisp.c
WASM=wallisp.wasm
MEM=33554432   # initial linear memory; the cell-arena size lives in the C source

command -v clang >/dev/null || { echo "error: clang not found" >&2; exit 1; }
clang --print-targets 2>/dev/null | grep -q wasm32 \
  || { echo "error: this clang has no wasm32 target (install brew llvm + lld)" >&2; exit 1; }

# -fno-builtin: wallisp.c defines its own memset/memcpy; without this clang
# rewrites the bodies into calls to themselves (undefined-symbol at link).
clang --target=wasm32 -nostdlib -fno-builtin -Oz \
  -Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined \
  -Wl,--initial-memory=$MEM \
  -o "$WASM" "$SRC"
echo "$WASM: $(wc -c < "$WASM") bytes"

if [ "${1:-}" = "--check" ]; then
  command -v node >/dev/null || { echo "error: node not found — --check requested but can't run" >&2; exit 1; }
  node test.mjs
fi
