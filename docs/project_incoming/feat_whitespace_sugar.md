---
status: open
created: 2026-06-02
updated: 2026-06-02
---
# feat: whitespace/indentation sugar (parked)

Design analysis on layering whitespace-significant syntax onto wallisp —
`def NAME args: BODY`, `if C: T else: E`, indentation→nesting, dot-chain
threading. **Not a concrete plan**; a framework for deciding where each
kind of sugar belongs in the `chars → tokens → s-expr → eval` pipeline.

Key takeaway: structural relabelings (def/if/indent) round-trip cleanly;
argument-moving sugar (threading, infix-precedence) is one-way without
provenance tags. Prior art: SRFI 49 (i-expressions), SRFI 110 (sweet),
SRFI 119 (wisp).

Cost of acting on this in wallisp: reader goes from paren-counter to
indent-stack + Pratt (roughly doubles its size); threading needs either
a macro system (wallisp has none) or a hardcoded `->` special form.
Crosses the line from "measurement study" into "Lisp dialect design"
— worth doing as its own thing if reversible sugar is the goal.

---

The clean way to think about all of this: you have a pipeline, `chars → tokens → s-expressions → eval`, and every transformation you're describing is a choice of *which stage to insert it at*. That choice decides both what's possible and whether it reverses. Your three sketches actually live at two different stages, and threading lives at a third, which is why they feel related but slippery.

**Reader-level (chars/tokens → s-expr).** This is where your paren-free and `def fact n:` forms belong. The reader is the only thing that gets to decide what counts as valid input, so anything that changes *delimiters, layout, or infix* has to happen here. The catch is that the moment you make whitespace significant, your reader stops being a paren-counter and becomes a real parser: you need an indentation stack emitting INDENT/DEDENT tokens (exactly like Python's lexer), and if you allow infix like `n < 2` *and* `a + b * c`, you need a precedence parser — Pratt or shunting-yard — sitting in the middle. Your specific desugar rules are mostly clean:

- indentation → nesting (INDENT opens a group, DEDENT closes it)
- `def NAME args…: BODY` → `(define NAME (lambda (args…) BODY))` (rule: everything between the name and the colon is the param list — that resolves your `def X n` vs `def X (n)` question)
- `if C: T else: E` → `(if C T E)`
- juxtaposition = application, parens = grouping

The one troublemaker is infix-with-precedence. Everything else is a structural relabeling; `a + b * c` is the only place you're actually *deciding* tree shape from flat text.

Worth knowing this is solved prior art you can crib from rather than reinvent: **I-expressions** (SRFI 49) do pure indentation→paren nesting, **sweet-expressions** (SRFI 110) add infix `{a + b}` and `f(x)` call notation on top, and **wisp** (SRFI 119, used in Guile) is another whitespace-to-Lisp reader. They're explicitly designed so the indented form and the paren form denote the same data, which is precisely the reversible mapping you're chasing.

**Macro-level (s-expr → s-expr, before eval).** This is where threading lives, and it's a different animal. The reader still parses `(-> someval (sub 2) (mul 3))` as an ordinary nested list — no new syntax at all — and then a macro rewrites that *tree* into `(mul (sub someval 2) 3)` before eval ever sees it. Thread-first inserts the accumulator as argument 1 of each step; thread-last (`->>`) inserts it last. The implementation is a fold: seed value, then for each step splice the accumulated form into position 1 (or n). Four lines.

So your `someval .sub 2 .mul 3` is actually *two* layers stacked, and you get to choose how much each does:

- a **reader** change that turns the dotted chain into `(-> someval (sub 2) (mul 3))` (or straight into the nested call), plus
- the **threading macro** that does the nesting

The nice factoring is: let the reader do the minimal job of recognizing the chain and emitting a `->` form, then let the macro own the actual rewrite. That keeps the rearrangement logic in one tiny, testable place that's itself just a Lisp function.

**The part you're actually after — reversibility.** This is the sharp distinction: *delimiter/layout sugar round-trips; argument-moving sugar doesn't.* `(if c t e) ↔ if c: t else: e` reverses cleanly because nothing moves — same arity, same children, different brackets, so the reverse is just a pretty-printer walking the tree and re-emitting. But threading and infix-precedence *relocate arguments*, and that's lossy going backward: `(g (f x a) b)` could have come from `(-> x (f a) (g b))` or just been written that way — you can't recover the pipeline form because "which argument was the threaded one?" isn't recorded in the result. Same with infix: once `a + b * c` becomes `(+ a (* b c))`, the precedence info is baked in and there's nothing to invert ambiguously.

So the practical answer: split your sugar into the two buckets. The structural relabelings (def, if, let, indentation) you can write as a pair of mutually-inverse recursive walks over the same tree shape — genuinely reversible. The rearranging ones (threading, infix) are one-way unless you keep provenance, i.e. tag the rewritten nodes with where they came from so the unparser can undo them. For a toy, I'd make the reversible bucket a real round-trip and treat threading as expand-only.