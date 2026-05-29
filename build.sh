#!/usr/bin/env bash
# Build every engine to wasm. Requires clang with the wasm32 target + wasm-ld.
# Output .wasm land at the repo root (where the harnesses expect them).
set -e
cd "$(dirname "$0")"

FLAGS="--target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined"
MEM="-Wl,--initial-memory=33554432"      # 32 MB — fits the default 131072 / 262144-cell arenas

echo "engines (default arenas):"
clang $FLAGS $MEM -O2              -o lisp.wasm         engines/lisp.c
clang $FLAGS $MEM -O2 -mtail-call  -o cek.wasm          engines/cek.c          # CEK uses wasm tail calls
clang $FLAGS $MEM -O2              -o bytecode.wasm     engines/bytecode.c
clang $FLAGS $MEM -O2 -fno-builtin -o bytecode_gc.wasm  engines/bytecode_gc.c  # GC build ships its own memset
echo "  -> lisp.wasm cek.wasm bytecode.wasm bytecode_gc.wasm"

echo "prototype line (no TCO / no GC — the optimization ladder):"
clang $FLAGS $MEM -O2 -o bc_base.wasm   prototype/bc_base.c     # + instruction counter
clang $FLAGS $MEM -O2 -o bc_inline.wasm prototype/bc_inline.c   # + runtime-inlined prims
clang $FLAGS $MEM -O2 -o bc_super.wasm  prototype/bc_super.c    # + compile-time superinstructions
echo "  -> bc_base.wasm bc_inline.wasm bc_super.wasm"

# Big-arena variants for harness/bench.mjs. The no-GC engines exhaust a small arena
# on heavy benchmarks (fib(24) wants ~1.8M cells), so bench needs roomy builds.
echo "bench variants (16M-cell arenas):"
BIGMEM="-Wl,--initial-memory=268435456"   # 256 MB (CEK allocates most; large arenas keep all 3 engines comparable)
mkbig () { sed 's/define MAX_CELLS.*/define MAX_CELLS 16000000/' "$1" > /tmp/_big.c; clang $FLAGS $BIGMEM $3 -O2 -o "$2" /tmp/_big.c; }
mkbig engines/lisp.c     lisp_big.wasm
mkbig engines/cek.c      cek_big.wasm      "-mtail-call"
mkbig engines/bytecode.c bytecode_big.wasm
echo "  -> lisp_big.wasm cek_big.wasm bytecode_big.wasm"
echo "done."
