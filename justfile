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

# The harness/ suites run on the checked-in *.wasm with no build step.
# standalone/test.mjs is the exception: it loads standalone/wallisp.wasm, which
# is gitignored. CI runs the no-build subset (.github/workflows/test.yml).
# Core suites (standalone/test.mjs needs `bash standalone/build.sh` first)
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

# Rebuilds via build.sh, byte-compares against the tracked modules, then restores
# the tree. Byte-identity is toolchain-specific — read the note at the top of the
# script before treating a mismatch as a defect. Run before a release and after
# any toolchain bump.
# Check the tracked *.wasm still rebuild byte-for-byte
verify-artifacts:
    bash tests/verify_artifacts.sh

# Cross-engine benchmark (builds the big-arena variants first)
bench: build
    node harness/bench.mjs
