#!/usr/bin/env bash
# Build every engine to wasm. Requires clang with the wasm32 target + wasm-ld.
# Output .wasm land at the repo root (where the harnesses expect them).
#
# Pass --native to ALSO build native binaries (no wasm, no V8) for direct
# engine-vs-engine measurement without the JIT/runtime layer. Native bench/CLI
# use Apple/system clang and need no Homebrew toolchain.
set -e
cd "$(dirname "$0")"

WANT_NATIVE=0
[ "${1:-}" = "--native" ] && WANT_NATIVE=1

# -fno-builtin on all engines: keeps modules zero-imports (the project's headline
# property). Without it, newer clang (LLVM 20+) synthesizes calls to strlen for
# reader patterns and memset for large arena zero-init, both of which would become
# undefined env.* imports in a freestanding wasm32 build.
FLAGS="--target=wasm32 -nostdlib -fno-builtin -Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined"
MEM="-Wl,--initial-memory=33554432"      # 32 MB — fits the default 131072 / 262144-cell arenas

echo "engines (default arenas):"
clang $FLAGS $MEM -O2              -o lisp.wasm             engines/lisp.c
clang $FLAGS $MEM -O2              -o lisp_trampoline.wasm  engines/lisp_trampoline.c  # explicit while(TRUE) trampoline (H1 verification)
clang $FLAGS $MEM -O2              -o lisp_gc.wasm          engines/lisp_gc.c          # tree-walker + mark-sweep GC (H4)
clang $FLAGS $MEM -O2              -o lisp_region.wasm      engines/lisp_region.c      # tree-walker + region-drop GC (H2 zero floor)
clang $FLAGS $MEM -O2 -mtail-call  -o cek.wasm          engines/cek.c          # CEK uses wasm tail calls
clang $FLAGS $MEM -O2 -mtail-call  -o cek_gc.wasm       engines/cek_gc.c       # CEK + mark-sweep GC (H4)
clang $FLAGS $MEM -O2              -o bytecode.wasm     engines/bytecode.c
clang $FLAGS $MEM -O2              -o bytecode_gc.wasm  engines/bytecode_gc.c  # GC build ships its own memset
echo "  -> lisp.wasm lisp_trampoline.wasm lisp_gc.wasm lisp_region.wasm cek.wasm cek_gc.wasm bytecode.wasm bytecode_gc.wasm"

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
mkbig engines/lisp.c            lisp_big.wasm
mkbig engines/lisp_trampoline.c lisp_trampoline_big.wasm  # for direct A/B vs lisp_big at matching arena
mkbig engines/lisp_region.c     lisp_region_big.wasm
mkbig engines/cek.c             cek_big.wasm      "-mtail-call"
mkbig engines/bytecode.c        bytecode_big.wasm
echo "  -> lisp_big.wasm lisp_trampoline_big.wasm lisp_region_big.wasm cek_big.wasm bytecode_big.wasm"

if [ "$WANT_NATIVE" = "1" ]; then
  echo "native binaries (no wasm, no JIT — direct C measurement):"
  NFLAGS="-O2 -I. -Wno-unknown-attributes -Wno-ignored-attributes -Wno-macro-redefined"
  # Bench uses a big-arena engine variant (matches the wasm *_big.wasm builds, so
  # native vs wasm timings are apples-to-apples and no-GC engines don't bail on
  # heavy benchmarks). CLI uses the default-arena engine for fast load + small
  # one-off evals — same as the wasm CLI's bytecode_gc.wasm default.
  mknat () { # engine_name engine_src
    sed 's/define MAX_CELLS.*/define MAX_CELLS 16000000/' engines/$2.c > /tmp/_${1}_big.c
    clang $NFLAGS -DENGINE_NAME='"'$1'"' -DENGINE_SRC='"/tmp/_'$1'_big.c"' \
          -o native_bench_$1 native/bench.c
    clang $NFLAGS -DENGINE_SRC='"engines/'$2'.c"' \
          -o native_cli_$1 native/main.c
  }
  # -mtail-call is wasm-only; native musttail works on ARM64/x86 without it.
  mknat lisp             lisp
  mknat lisp_trampoline  lisp_trampoline
  mknat lisp_gc          lisp_gc
  mknat lisp_region      lisp_region
  mknat cek              cek
  mknat cek_gc           cek_gc
  mknat bytecode         bytecode
  mknat bytecode_gc      bytecode_gc
  echo "  -> native_bench_{lisp,lisp_trampoline,lisp_gc,lisp_region,cek,cek_gc,bytecode,bytecode_gc} + native_cli_*"

  # Hand-written reference points: same five benchmarks in pure C, no engine.
  # Sets the bottom-of-stack number — what -O2 native compute costs for these
  # algorithms with no interpreter in the way.
  clang -O2 -o native_bench_baseline baselines/bench.c
  echo "  -> native_bench_baseline (hand-written C reference)"
fi
echo "done."
