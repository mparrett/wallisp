#!/usr/bin/env bash
# verify_artifacts.sh — are the tracked *.wasm still reproducible from source?
#
# The nine engine modules at the repo root are checked in so the harnesses and
# the CLI run without a toolchain (see DEV.md). That convenience is only honest
# while the bytes still match what the compiler actually produces from
# engines/*.c. This rebuilds through build.sh and byte-compares.
#
# Run it before a release and after any toolchain bump.
#
#   bash tests/verify_artifacts.sh          # or: just verify-artifacts
#
# WHAT A MISMATCH MEANS: byte-identity is toolchain-specific. A different clang
# version legitimately produces different — still correct — output; measured
# 2026-07-28, clang 19 and 21 each differ from all nine artifacts that clang
# 22.1.6 reproduces exactly. So a mismatch means "built by a different compiler
# than the one on this machine," not necessarily "broken." Check your clang
# against the toolchain of record in DEV.md before concluding anything.
#
# This is deliberately NOT in CI: pinning the exact toolchain there isn't
# currently possible (no container image for the version of record), and a
# byte check that goes red on every LLVM bump trains people to ignore it.
# build.sh's zero-imports assertion is the part that DOES belong in CI.

set -euo pipefail
cd "$(dirname "$0")/.."

# Root-level tracked modules only — those are exactly what build.sh emits.
# web/bytecode_gc.wasm is also tracked (it predates the .gitignore entry, which
# can't untrack it) but comes from web/build-standalone.sh, so it isn't in scope.
MODULES=$(git ls-files '*.wasm' | grep -v '/')
[ -n "$MODULES" ] || { echo "error: no tracked root *.wasm found" >&2; exit 1; }

# build.sh writes over the tracked modules in place, so refuse to run against a
# dirty tree — otherwise a failed comparison would destroy uncommitted bytes.
if ! git diff --quiet -- $MODULES; then
  echo "error: tracked *.wasm have uncommitted changes." >&2
  echo "  Commit or stash them first; this script overwrites them to rebuild." >&2
  exit 1
fi

# Snapshot, then restore unconditionally — the working tree must look untouched
# whether the comparison passes, fails, or the build dies partway through.
SNAP=$(mktemp -d)
restore() { for m in $MODULES; do [ -f "$SNAP/$m" ] && cp "$SNAP/$m" "$m"; done; rm -rf "$SNAP"; }
trap restore EXIT
for m in $MODULES; do cp "$m" "$SNAP/$m"; done

echo "toolchain: $(clang --version | head -1)"
echo "rebuilding via build.sh ..."
bash build.sh >/dev/null

fail=0
for m in $MODULES; do
  if cmp -s "$m" "$SNAP/$m"; then
    echo "  = $m"
  else
    echo "  ! $m  differs (tracked $(wc -c <"$SNAP/$m" | tr -d ' ') bytes, rebuilt $(wc -c <"$m" | tr -d ' ') bytes)"
    fail=1
  fi
done

n=$(echo "$MODULES" | wc -w | tr -d ' ')
if [ "$fail" = 0 ]; then
  echo "all $n tracked modules reproduce byte-for-byte"
else
  echo "MISMATCH — see the note at the top of this script before treating it as a defect." >&2
fi
exit $fail
