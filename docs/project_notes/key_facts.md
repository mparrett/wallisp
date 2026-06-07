# Key Facts

## Tracked wasm artifacts

The default-arena wasm binaries at the repo root (`lisp.wasm`,
`lisp_trampoline.wasm`, `lisp_gc.wasm`, `lisp_region.wasm`, `cek.wasm`,
`cek_gc.wasm`, `bytecode.wasm`, `bytecode_gc.wasm`) are **tracked
intentionally** so harnesses run without a build step. The .gitignore
spells out the why.

**Operational rule**: when you change engine source (anything that affects
the wasm output), rebuild via `./build.sh` and commit the refreshed wasm
files **in the same commit** as the source change. Otherwise the in-tree
binaries silently drift from the source.

This was missed during the shared-reader extraction (commits `3e3de3a`
Phase 1 and `24dae4f` Phase 2 left the wasm stale; `7b8c495` Phase 3
caught them up alongside the f-call sugar changes). Internally consistent
after Phase 3, but Phase 1/2 alone would have rebuilt to different bytes
than what was committed.

The bench-variant `*_big.wasm` files and prototype `bc_*.wasm` files are
gitignored — those are pure build artifacts.
