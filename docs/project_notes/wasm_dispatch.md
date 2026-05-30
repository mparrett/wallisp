# The dispatch loop, at the wasm layer

Companion to `bytecode_disasm.md`. That doc looks at what the bytecode VM
compiles user programs into — the *inner* IR. This one looks at what
clang compiles the bytecode VM itself into — the **wasm** that V8 then
JITs.

Why both views matter: when we ask "is the metacircular slow because of
LOADG?" — that question lives at the *outer* layer. The answer requires
knowing what a `LOADG` opcode *costs at runtime*, which is determined
by how the dispatch loop's LOADG arm is shaped in wasm, and how V8
specializes it.

## How to use

```bash
brew install wabt                                       # one-time
wasm2wat bytecode_gc.wasm -o /tmp/bytecode_gc.wat
grep -n 'br_table' /tmp/bytecode_gc.wat                 # find the dispatch switches
```

The big VM dispatch is the `br_table` with 12 targets — one arm per
opcode. The 12 opcodes are CONST, LOADL, LOADG, DEFG, POP, JMP, JFALSE,
CLOSURE, CALL, TAILCALL, RET, HALT (see the enum at
`engines/bytecode_gc.c:178`).

## Reproducing the snapshot

The wat excerpt isn't committed (line numbers shift across clang
versions). To capture the dispatch block next to this file, find the
largest `br_table` (the 12-arm one) and grab ~90 lines around it:

```bash
wasm2wat bytecode_gc.wasm > /tmp/bytecode_gc.wat
# Locate the br_table with the most targets — that's the VM dispatch.
LINE=$(awk '/br_table/{if(NF>max){max=NF;ln=NR}} END{print ln}' /tmp/bytecode_gc.wat)
sed -n "$((LINE-15)),$((LINE+75))p" /tmp/bytecode_gc.wat \
  > docs/project_notes/wasm_dispatch.excerpt.wat
```

Pattern-based, not line-anchored, so it survives clang updates and
unrelated engine edits. The file is gitignored.

## What we found

clang at `-O2` compiles the run loop's `switch(op){case OP_X: ...}` into
a single `br_table` (line 2410 in the current build) followed by one
per-opcode `block` arm. The full excerpt is at
`wasm_dispatch.excerpt.wat` next to this file. Sketch:

```wat
local.get 4                   ; opcode register
i32.load offset=2986112       ; load code[ip]
br_table 11 0 1 2 3 4 5 6 7 8 9 10 12   ; one target per opcode
```

That's the canonical "compute the index, branch to the matching arm"
pattern V8 specializes hardest. Each opcode arm is contiguous wasm
that V8 will inline-cache and tier up independently.

## What this means for the LOADG observation

`bytecode_disasm.md` notes that 32% of the metacircular bytecode is
`LOADG`. The question that view *can't* answer is: does that share
correspond to runtime cost?

Looking at the wasm: the LOADG arm is ~30-40 ops — read the operand
word, shift, load from the globals array, tag-check (the `i32.const 1
i32.and` is the tagged-fixnum LSB test), push to operand stack. Tight.
This is the same shape as the env-lookup falsification (FINDINGS.md
"Two surprises"): the operation is walked frequently, the bytecode count
shows it, but V8 already specializes the inlined arm into something
close to free, so optimizing it at the bytecode level shifts work to
compile time without reducing per-step runtime cost.

That's the substrate-amplification story stated as a tool: **the wasm
view shows when a frequently-emitted opcode is already a tight inlined
arm, vs when it's a costly out-of-line call**. The former is a sunk
cost V8 hides; the latter is a real lever.

For the metacircular, the dominant non-`LOADG` cost is more likely the
allocation rate (every closure call does an `extend`-shaped env
chain extension, building cons cells; the optimization-barrier-per-cons
mechanism from H4/H2 is what flips the GC tax sign in the
metacircular benchmark — see `index.html` §9 and FINDINGS.md
"Metacircular evaluator"). The disassembly was the thing that let us
*verify* the dispatch loop wasn't the bottleneck even though it looks
that way at first glance.
