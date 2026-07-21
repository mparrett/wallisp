# Pointers for a TUI-game-in-homegrown-Lisp project

**Source:** lessons distilled from working on **xsofy** (a roguelike game) and **let-go** (the Clojure-dialect VM on Go bytecode it runs on). Both are nooga's projects; we've been contributing fork patches + an integration workspace.

**Audience:** sibling team building TUI games in C or Rust, with a homegrown Lisp on top. Assumes they're early enough to bake architectural choices in.

**Bias of these pointers:** things that aren't obvious until you've shipped a TUI game and watched it bend in production. Skip the obvious; surface the tuition we paid.

---

## Architecture (most leverage, hardest to retrofit)

### 1. Make every world mutator a pure function `(state, action) → state` from day 1, and log the action stream.

This is the single highest-leverage move. We watched xsofy's `#67 widen the replay contract: route menu item-actions through dispatch` get written specifically because earlier code mutated the world inline from menu handlers, bypassing dispatch — which made replays silently diverge.

If they bake this in from line one, they get for free:

- Replay (deterministic re-execution of a recorded run)
- Deterministic regression tests
- "Give me your seed and action log" bug reports
- Time-travel debugging (rewind to action N)

**Caveat:** you must enforce it ruthlessly. One inline mutator that bypasses dispatch silently rots replays. Even "small" actions (inventory item use, equip, drop, enchant) belong on the action log.

### 2. RNG is a value you thread, not ambient state.

xsofy's `#62 reproducible runs` reworked title/quest/scheme generation into `[value, rng']` pair returns. Looks Haskell-ish at first; pays for itself the moment you want "seed N reproduces this exact world."

Tests like `(= "Chasms of Infinity" (title-from-seed 12345))` are trivially cheap to write and catch determinism regressions in CI long before users do.

The threading discipline also helps you reason about *where* randomness enters. Ambient `(rand)` calls are a debugging nightmare; a threaded RNG is a value you can inspect.

### 3. Wire-format your action log carefully and treat it as ABI.

Replay codes (xsofy's `xsofy/replay.lg`) are user-shareable byte strings. Renaming an action keyword breaks every existing code.

Choose one:

- **Version the wire format aggressively** — explicit version byte, bump it whenever the action vocabulary changes, refuse old codes by default
- **Commit to "action keywords are forever" from day 1** — and add a linter that warns on rename

They will pick wrong if not warned. The wrong default (silently breaking codes from previous releases) is the lazy path.

---

## Infrastructure (cheap to skip, expensive to bolt on)

### 4. Headless-safe rendering is a first-class platform, not a degraded one.

Their TUI library will return `null` for terminal size during CI, during a resize, or in a worker context. We hit this exactly: `term/size` returned nil and the whole render path destructured to garbage.

The fix (`term-dims` helper with a sensible fallback) is four lines, but they should write it **before the first CI run, not after a blank-screen mystery.**

```clojure
;; xsofy's pattern (Clojure-flavored):
(defn term-dims []
  (or (term/size) [game-width game-height]))
```

Translate to their language; the principle stands. Anywhere you read terminal dimensions, route through the safe helper.

### 5. Pin the runtime version in CI; run a `@latest` matrix cell as a drift smoke.

Concretely:

```yaml
matrix:
  - letgo: v1.9.0    # blocking gate
  - letgo: latest    # informational drift smoke
```

Catches "upstream silently re-tagged something" without breaking the build. Your CI shouldn't `cargo install --git=main` or its equivalent; that's how you wake up to mystery failures.

### 6. Render with dirty regions + a full-redraw fallback.

Brogue-style. Naive "repaint everything every tick" doesn't scale to mobile.

xsofy's render path has a camera/viewport system with dead-zone gating so most turns are FOV-delta renders. Build order:

1. Build the full-redraw path first (it's trivial)
2. Add the diff path on top, with a switch
3. If dirty render gets confused, fall back to full

Don't let "always correct but slow" and "fast but sometimes wrong" be a choice. Have both, fall back when needed. The cost of a stutter is much less than the cost of a stale rendered cell.

### 7. If they want browser/mobile reach: WASM + xterm.js + cross-origin isolation + SharedArrayBuffer for blocking input read.

That's the recipe. The COI handshake is fiddly (service worker fallback for hosts that don't set headers). Plan to ship a tiny `coi-serviceworker.js` alongside the bundle.

SAB is necessary because input reads block in TUI games, and main-thread `prompt()`-style APIs would deadlock the worker. The pattern: main thread writes UTF-8 key bytes into a `SharedArrayBuffer` slot; worker spins on `Atomics.wait` reading from it.

C/Rust to WASM: emscripten / wasm-bindgen respectively. Same SAB + COI dance applies regardless.

---

## Lessons we paid tuition on (so they don't have to)

### 8. Hot functions deserve native bindings before you reach for caches.

xsofy had ~1500 lines of pure-Lisp xxh3 hash code, plus a perf-cache layer wrapping it, plus invalidation gating to make the cache safe.

`#64` replaced the lot with ~5 lines of native interop and dropped 1486 LOC net. **The "snappy" feel we noticed on the rebuild was unwinding compensation, not adding optimization.**

When perf is bad in a dynamic-language game, **suspect a missing native binding before you write cache code.** C/Rust make this trivial — they should expose hot host primitives early. Hash functions, byte manipulation, anything in an inner loop.

### 9. Layer the host/template boundary carefully — anything you customize on top of the runtime's bundle template will fall off when upstream refactors.

We just lived through this. A `wasm.go → host.html + lg-host.js` upstream refactor silently dropped a shell-slots patch we had on the fork. We discovered it weeks later when the mobile shell stopped injecting.

Two options:

- **Upstream your generic mechanisms** so you don't carry a private fork
- **Accept that re-ports will be a recurring tax**, and write the re-port commits in a way that's easy to redo (small, well-commented, isolated)

A `--shell <path>` flag at the runtime level for "host page can swap in their own chrome" is a clean factoring. Generic enough that upstream might take the PR.

### 10. TUI games benefit from a shell wrapper for mobile — a separate HTML fragment the bundle fetches on boot and injects around the terminal.

Gives you touch buttons, D-pad, font scaling, status bar — all the things mobile expects but a terminal lacks.

Crucially: **keep it optional.** The bundle should work standalone on desktop with no shell at all. Empty-slot CSS handles graceful degradation:

```css
#shell-top:empty, #shell-bottom:empty { display: none; }
```

xsofy's shell.html is a single 42K HTML fragment with all the touch UI, fetched by the WASM bundle on boot. Desktop loads ignore it; mobile gets a real native-feeling app.

---

## One meta-pointer

Their homegrown Lisp probably has interop with their host language (C/Rust). **Make sure their TUI primitives go through that interop layer** — think `term/move-cursor`, `term/write`, `term/size` as Lisp-visible names backed by native impls — rather than reimplementing terminal sequences in Lisp.

- **C**: ncurses / termios are battle-tested. Wrap them.
- **Rust**: crossterm / termion. Same.

Wrapping costs less than reimplementing and gains decades of platform compatibility (true-color detection, alternate-screen mode, Windows ConPTY, mouse reporting, etc.).

---

## TL;DR — top 5 if they only read this far

1. **Pure-function dispatch + action log from day 1.** Replay, regression tests, debugging — all flow from this.
2. **Thread the RNG; never ambient.** Deterministic seeding is a feature.
3. **Native bindings for hot functions before caches.** Caches papering over slow hot loops are a rabbit hole.
4. **Headless-safe rendering as a property.** `term/size` can be nil; design for it.
5. **Pin the runtime in CI; drift-smoke `@latest`.** Don't let upstream drift bite you silently.

---

*Compiled 2026-06-08 from session experience on xsofy (nooga/xsofy) + let-go (nooga/let-go). Happy to elaborate on any pointer — we have receipts.*
