#!/usr/bin/env bash
# harness/disasm.sh — build the disasm variant of bytecode_gc.
#
# The bytecode VM compiles each input form into a flat opcode stream before
# running. This variant dumps that stream (addresses + opcode names +
# decoded operands) instead of executing, so you can see exactly what the
# compiler emits for any given source program. Builds to disasm.wasm at
# the repo root.
#
#   bash harness/disasm.sh
#   node harness/disasm.mjs baselines/metacircular.lisp
#
# See FINDINGS.md "Bytecode shape — what the metacircular compiles to" and
# docs/project_notes/bytecode_disasm.md for the analysis.

set -e
cd "$(dirname "$0")/.."

# Needs Homebrew's clang for the wasm32 target (same as build.sh).
export PATH=/opt/homebrew/opt/llvm/bin:${PATH}

# Disasm-only build: flips the post-compile path from run() to "dump bytecode
# to outbuf and return". OUTCAP bumped because a 4KB buffer can't hold the
# disassembly of anything non-trivial.
FLAGS="--target=wasm32 -nostdlib -fno-builtin -Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined -Wl,--initial-memory=33554432"
clang $FLAGS -O2 -DDISASM_ONLY -DOUTCAP=262144 -o disasm.wasm engines/bytecode_gc.c
echo "  -> disasm.wasm ($(stat -f%z disasm.wasm) bytes)"
