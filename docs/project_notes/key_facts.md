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

## Interactive / I/O surface (current state)

Baseline as of 2026-06-08, for the terminal/game roadmap
(`terminal_game_roadmap.md`, ADR-003). Verified by grep across `engines/`,
`standalone/`, `web/`, `harness/`:

- **Host ABI is one-shot, stateless, zero-imports.** Write source into
  `inbuf` → `eval_source(len)` → read printed result from `outbuf`.
  `eval_source` calls `init()` **every call** — no persistent session.
- **No input primitive.** Nothing reads a key/char mid-eval. The `stdin`
  handling in `standalone/cli.mjs` / `harness/lisp-cli.mjs` only feeds *source*
  to eval, not interactive input.
- **No terminal surface.** No ANSI / cursor / tty primitives anywhere ("cursor"
  hits in the tree are CSS and the compiler's code-emit pointer, not a
  terminal cursor).
- **No bitwise primitives** (`and`/`or`/`xor`/shifts). An LCG PRNG is
  expressible (`* + mod`); xorshift/PCG are not.
- **Web showcase evals once per click** (`web/template.html`,
  `web/refresh-tiny-lisp-vm.sh`) — no session/history persistence.
- **Strings exist only in `bytecode_gc`**, and its side-heap reclamation is
  **incomplete** — that's why `standalone/` lifted strings out. This is the
  named blocker (B4) for a per-frame-allocating game.
