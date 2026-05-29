---
status: open
assigned: claude-code
created: 2026-05-29
updated: 2026-05-29
---
# Feature: tiny Docker dev environment

## Summary

Bundle a minimal Docker image with the toolchain pinned (clang+wasm-ld
from a known LLVM version, node, wabt) so contributors don't have to
fight the macOS/Homebrew keg-only dance or chase clang regressions like
the LLVM-20 `strlen` builtin issue we just hit.

## Motivation

Smoke-testing on a fresh macOS this session required:
- `brew install llvm lld` (both keg-only, so PATH prefix needed)
- A `-fno-builtin` patch to `build.sh` because newer clang synthesized
  `env.strlen` / `env.memset` calls that broke instantiation
- Hunting through `llvm-objdump` to diagnose the import mismatch

A pinned Docker image avoids all of that for anyone who has Docker.

## Sketch

- Base: `debian:slim` or `alpine`
- Install: a specific LLVM version (e.g. 18, where the project was
  originally built and known to produce zero-imports without
  `-fno-builtin`), `node`, `wabt`
- Mount the repo, run `bash build.sh` and `node harness/*.mjs`
- Provide a `justfile` or `docker compose` one-liner

## Acceptance

- `docker run ... bash build.sh` produces all 10 `*.wasm` modules, all
  reporting **zero imports**
- `docker run ... node harness/test_bc.mjs` → 19/19
- `docker run ... node harness/bench.mjs` → all engines agree, no
  DISAGREE flags
- README.md gets a "use Docker if you don't want to install a toolchain"
  pointer
