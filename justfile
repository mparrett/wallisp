# wallisp task runner. `just <recipe>`; `just` alone lists recipes.
# Test recipes fail fast — any suite that breaks fails the run.

# List recipes
default:
    @just --list

# Build all engines to wasm (needs a clang with the wasm32 target + wasm-ld;
# on macOS that's Homebrew LLVM, not Apple's /usr/bin/clang)
build:
    bash build.sh

# Also build the native binaries (Apple/system clang is fine for these)
build-native:
    bash build.sh --native

# Core suites — run on the checked-in *.wasm, no build step needed
test:
    node harness/parity.mjs
    node harness/parity_strings.mjs
    node harness/parity_callcc.mjs
    node harness/test_bc.mjs
    node harness/test_session.mjs
    node standalone/test.mjs

# Every suite, including the ones that need the native CLI and the futamura build
test-all: build-native test
    bash tests/reader_sugar.sh
    node harness/test_futamura_regressions.mjs

# Cross-engine benchmark (builds the big-arena variants first)
bench: build
    node harness/bench.mjs
