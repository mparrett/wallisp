#!/usr/bin/env bash
# Build the specializer, then use it to (re)generate the residuals from the
# .lisp sources, then compile those residuals to wasm. Produces:
#   build/specialize                   the host-side residualizer
#   build/residual_fib_gen.c           generated fib residual (Lisp -> C)
#   build/residual_tak_gen.c           generated tak residual
#   $REPO_ROOT/residual_*_gen.wasm     wasm residuals (lands at repo root, like the engines)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"
mkdir -p "$BUILD"

cc -O2 -Wall -o "$BUILD/specialize" "$HERE/specialize.c"

"$BUILD/specialize" < "$HERE/fib.lisp" > "$BUILD/residual_fib_gen.c"
"$BUILD/specialize" < "$HERE/tak.lisp" > "$BUILD/residual_tak_gen.c"

# Same flags as build.sh.
WASMFLAGS=(--target=wasm32 -nostdlib -fno-builtin -Wl,--no-entry
           -Wl,--export-dynamic -Wl,--allow-undefined
           -Wl,--initial-memory=33554432 -O2)

export PATH="$(brew --prefix llvm)/bin:$(brew --prefix lld)/bin:$PATH"
clang "${WASMFLAGS[@]}" -o "$REPO_ROOT/residual_fib_gen.wasm" "$BUILD/residual_fib_gen.c"
clang "${WASMFLAGS[@]}" -o "$REPO_ROOT/residual_tak_gen.wasm" "$BUILD/residual_tak_gen.c"

# Hand-written reference variants (rep-specialized i32 — what a smart PE
# with type-flow analysis would emit). Kept around so the bench can show
# the rep-specialization headroom over the tagged gen residual.
clang "${WASMFLAGS[@]}" -o "$REPO_ROOT/residual_fib_untagged.wasm" "$HERE/residual_fib_untagged.c"
clang "${WASMFLAGS[@]}" -o "$REPO_ROOT/residual_tak_untagged.wasm" "$HERE/residual_tak_untagged.c"

echo "built:"
echo "  $BUILD/specialize"
echo "  $REPO_ROOT/residual_fib_gen.wasm $REPO_ROOT/residual_tak_gen.wasm"
echo "  $REPO_ROOT/residual_fib_untagged.wasm $REPO_ROOT/residual_tak_untagged.wasm"
