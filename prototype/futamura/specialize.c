// specialize.c — Lisp → C residualizer (Futamura projection #1 in code).
//
// Reads a Lisp source on stdin consisting of one or more top-level forms in
// either form:
//   (define name (lambda (a b ...) body))
//   (define (name a b ...) body)
// and emits a freestanding C residual on stdout that matches the engines'
// `eval_source` ABI. The *last-defined* function is the entry point: its
// arity determines how many ASCII integers `eval_source` parses from inbuf.
//
// Build:  cc -O2 -o specialize prototype/futamura/specialize.c
// Use:    ./specialize < fib.lisp > residual_fib_gen.c
//         clang --target=wasm32 ... -O2 -o residual_fib_gen.wasm residual_fib_gen.c
//
// Scope of v1: handles `if`, `+ - * < > = mod`, named function calls,
// integer literals, parameter refs, top-level defines. Emits the *tagged*
// 30-bit-fixnum representation. Out of scope: closures over locals, `let`,
// `cond`, `begin` inside a body (only at top level), `lambda` returned as a
// value, cons/car/cdr (no allocator in the residual).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef unsigned int u32;
typedef int          i32;

/* ---- value tagging (matches engines/lisp.c) -------------------------------*/
#define TAG_MASK  3u
#define TAG_FIX   0u
#define TAG_CONS  1u
#define TAG_SYM   2u
#define TAG_SPEC  3u
#define mkfix(n)   ((u32)(((i32)(n)) << 2) | TAG_FIX)
#define fixval(v)  (((i32)(v)) >> 2)
#define mkcons(i)  (((u32)(i) << 2) | TAG_CONS)
#define considx(v) ((v) >> 2)
#define mksym(i)   (((u32)(i) << 2) | TAG_SYM)
#define symidx(v)  ((v) >> 2)
#define mkspec(i)  (((u32)(i) << 2) | TAG_SPEC)
#define tagof(v)   ((v) & TAG_MASK)
#define is_fix(v)  (tagof(v)==TAG_FIX)
#define is_cons(v) (tagof(v)==TAG_CONS)
#define is_sym(v)  (tagof(v)==TAG_SYM)
enum { SP_NIL=0, SP_T };
#define NIL   mkspec(SP_NIL)
#define is_nil(v) ((v)==NIL)

/* ---- arenas / reader (cribbed from engines/lisp.c) ------------------------*/
#define MAX_CELLS  131072
#define MAX_SYMS   1024
#define SYM_CHARS  32
typedef struct { u32 car, cdr; } Cell;
static Cell cells[MAX_CELLS];
static u32  cell_top = 0;
static char symname[MAX_SYMS][SYM_CHARS];
static u32  symlen[MAX_SYMS];
static u32  sym_top = 0;
static const char *rp, *rend;

static u32 cons(u32 a, u32 d){
  u32 i = cell_top++;
  if(cell_top > MAX_CELLS){ fprintf(stderr, "cells exhausted\n"); exit(1); }
  cells[i].car = a; cells[i].cdr = d;
  return mkcons(i);
}
static u32 car(u32 v){ return is_cons(v) ? cells[considx(v)].car : NIL; }
static u32 cdr(u32 v){ return is_cons(v) ? cells[considx(v)].cdr : NIL; }

static u32 intern(const char* s, u32 len){
  if(len > SYM_CHARS) len = SYM_CHARS;
  for(u32 i = 0; i < sym_top; i++)
    if(symlen[i] == len && memcmp(symname[i], s, len) == 0) return mksym(i);
  u32 i = sym_top++;
  memcpy(symname[i], s, len);
  symlen[i] = len;
  return mksym(i);
}
static const char* symstr(u32 v){
  // returns a null-terminated copy (static buffer; one call at a time).
  static char buf[SYM_CHARS + 1];
  u32 i = symidx(v);
  memcpy(buf, symname[i], symlen[i]);
  buf[symlen[i]] = 0;
  return buf;
}

static void skipws(void){
  while(rp < rend){
    char c = *rp;
    if(c==' '||c=='\t'||c=='\n'||c=='\r'){ rp++; }
    else if(c==';'){ while(rp<rend && *rp!='\n') rp++; }
    else break;
  }
}
static int is_delim(char c){
  return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='('||c==')'||c==';'||c==0;
}
static u32 read_expr(void);
static u32 read_list(void){
  u32 first = NIL, last = NIL;
  for(;;){
    skipws();
    if(rp >= rend) return first;
    if(*rp == ')'){ rp++; return first; }
    u32 e = read_expr();
    u32 link = cons(e, NIL);
    if(is_nil(first)){ first = link; last = link; }
    else { cells[considx(last)].cdr = link; last = link; }
  }
}
static u32 read_atom(void){
  const char* s = rp;
  while(rp < rend && !is_delim(*rp)) rp++;
  u32 len = (u32)(rp - s);
  int neg = 0; const char* p = s; u32 n = len;
  if(n > 0 && (*p == '-' || *p == '+')){ neg = (*p == '-'); p++; n--; }
  if(n > 0){
    int isnum = 1; i32 v = 0;
    for(u32 i = 0; i < n; i++){ char c = p[i]; if(c < '0' || c > '9'){ isnum = 0; break; } v = v*10 + (c - '0'); }
    if(isnum) return mkfix(neg ? -v : v);
  }
  return intern(s, len);
}
static u32 read_expr(void){
  skipws();
  if(rp >= rend) return NIL;
  if(*rp == '('){ rp++; return read_list(); }
  return read_atom();
}

/* ---- recorded functions ---------------------------------------------------*/
typedef struct {
  const char *name;     // C-mangled name (Lisp `-`/`?`/`!` → `_`)
  u32 params[8];        // interned symbol values
  int arity;
  u32 body;             // AST node (cons or atom)
  int comptime_only;    // body is a `(lambda ...)`: no runtime C function emitted
} Fn;
static Fn fns[32];
static int nfns = 0;

/* ---- spec-time values + closures (v2-extended) ----------------------------*/
// Closures are first-class comptime values. A closure is (params, body,
// captured-env). At beta-reduce time we restore the captured env on top of
// the current one, then push fresh param bindings.
typedef enum { BV_FIX, BV_RUNTIME, BV_LAMBDA } BVKind;
typedef struct { u32 name; BVKind k; i32 fix; int lam_idx; } Binding;
#define ENV_MAX 128
static Binding env_stack[ENV_MAX];
static int env_top = 0;

#define CAP_MAX 32
typedef struct {
  u32 params;            // cons-list of param symbols (AST)
  u32 body;              // body expression (AST)
  Binding cap[CAP_MAX];  // env snapshot at creation
  int n_cap;
} Closure;
#define CLOSURES_MAX 256
static Closure closures[CLOSURES_MAX];
static int n_closures = 0;

// Spec-time value: a fixnum literal, a closure index, or "couldn't fold."
typedef enum { SV_FAIL, SV_FIX, SV_LAMBDA } SVKind;
typedef struct { SVKind k; i32 fix; int lam_idx; } SVal;
static SVal sv_fail(void){ SVal v = {SV_FAIL,0,0}; return v; }
static SVal sv_fix(i32 x){ SVal v = {SV_FIX,x,0}; return v; }
static SVal sv_lam(int i){ SVal v = {SV_LAMBDA,0,i}; return v; }

// Fuel: bound inlining work per spec-time evaluation. Prevents recursive
// functions like fib from inlining unboundedly when called with a comptime arg.
#define EVAL_CT_FUEL 10000
static int eval_ct_fuel = EVAL_CT_FUEL;

static int env_lookup(u32 sym, Binding *out){
  for(int i = env_top - 1; i >= 0; i--)
    if(env_stack[i].name == sym){ *out = env_stack[i]; return 1; }
  return 0;
}

static int make_closure(u32 params, u32 body){
  if(n_closures >= CLOSURES_MAX){ fprintf(stderr, "specialize: too many closures\n"); exit(1); }
  if(env_top > CAP_MAX){ fprintf(stderr, "specialize: closure capture too large (%d)\n", env_top); exit(1); }
  int idx = n_closures++;
  Closure *c = &closures[idx];
  c->params = params; c->body = body; c->n_cap = env_top;
  for(int i = 0; i < env_top; i++) c->cap[i] = env_stack[i];
  return idx;
}

// Push an SVal as a Binding (for closure params / let RHSs).
static void push_binding(u32 name, SVal v){
  Binding b = { name,
                v.k == SV_FIX ? BV_FIX : (v.k == SV_LAMBDA ? BV_LAMBDA : BV_RUNTIME),
                v.k == SV_FIX ? v.fix : 0,
                v.k == SV_LAMBDA ? v.lam_idx : 0 };
  env_stack[env_top++] = b;
}

// C-mangle a Lisp identifier char: replace `-`, `?`, `!` with `_`.
static char c_mangle_char(char c){
  return (c == '-' || c == '?' || c == '!') ? '_' : c;
}

static const char* name_of(u32 sym){
  // dup into a fresh malloc, applying C-name mangling so identifiers like
  // `make-adder` become `make_adder` (hyphens are minus signs in C).
  u32 i = symidx(sym);
  char *s = malloc(symlen[i] + 1);
  for(u32 k = 0; k < symlen[i]; k++) s[k] = c_mangle_char(symname[i][k]);
  s[symlen[i]] = 0;
  return s;
}

/* ---- C emitter ------------------------------------------------------------*/
// One named binary primitive: source op -> C operator, returns u32 tagged.
typedef struct { const char *lisp; const char *c_int_op; int returns_bool; } BinOp;
static BinOp BINOPS[] = {
  {"+",   "+", 0},
  {"-",   "-", 0},
  {"*",   "*", 0},
  {"<",   "<", 1},
  {">",   ">", 1},
  {"=",  "==", 1},
  {"mod", "%", 0},
  {NULL, NULL, 0},
};
static BinOp* find_binop(u32 sym){
  if(!is_sym(sym)) return NULL;
  const char *s = symstr(sym);
  for(int i = 0; BINOPS[i].lisp; i++)
    if(strcmp(BINOPS[i].lisp, s) == 0) return &BINOPS[i];
  return NULL;
}

static int sym_eq(u32 v, const char *s){
  if(!is_sym(v)) return 0;
  u32 i = symidx(v);
  return symlen[i] == strlen(s) && memcmp(symname[i], s, symlen[i]) == 0;
}

// Forward decl — used by eval_ct.
static void emit_expr(FILE *out, u32 node);

// Look up a top-level define by symbol. fns[].name is the mangled C name; the
// Lisp symbol may contain `-`/`?`/`!` so we mangle on the fly while comparing.
static Fn* find_define(u32 sym){
  if(!is_sym(sym)) return NULL;
  u32 i = symidx(sym);
  for(int k = 0; k < nfns; k++){
    if(strlen(fns[k].name) != symlen[i]) continue;
    int match = 1;
    for(u32 j = 0; j < symlen[i]; j++){
      if(c_mangle_char(symname[i][j]) != fns[k].name[j]){ match = 0; break; }
    }
    if(match) return &fns[k];
  }
  return NULL;
}

// Try to evaluate `node` to a comptime SVal (fixnum or closure) using the
// current env. Returns SV_FAIL if any subexpression hits runtime or fuel runs
// out. The call site uses the result to decide whether to substitute inline
// (SV_FIX/SV_LAMBDA) or emit a runtime C expression (SV_FAIL).
static SVal eval_ct(u32 node){
  if(--eval_ct_fuel < 0) return sv_fail();

  if(is_fix(node)) return sv_fix(fixval(node));

  if(is_sym(node)){
    Binding b;
    if(env_lookup(node, &b)){
      if(b.k == BV_FIX)    return sv_fix(b.fix);
      if(b.k == BV_LAMBDA) return sv_lam(b.lam_idx);
      return sv_fail();  // BV_RUNTIME — not comptime
    }
    return sv_fail();    // unbound — usually a function parameter (runtime)
  }

  if(!is_cons(node)) return sv_fail();
  u32 head = car(node), args = cdr(node);

  // (lambda (params) body) — create a closure value capturing current env.
  if(sym_eq(head, "lambda")){
    u32 params = car(args), body = car(cdr(args));
    return sv_lam(make_closure(params, body));
  }

  // (if c t e) — fold only if condition is comptime.
  if(sym_eq(head, "if")){
    SVal sc = eval_ct(car(args));
    if(sc.k != SV_FIX) return sv_fail();
    return sc.fix ? eval_ct(car(cdr(args))) : eval_ct(car(cdr(cdr(args))));
  }

  // (let ((x V) ...) body) — bind each comptime RHS, recurse on body.
  if(sym_eq(head, "let")){
    u32 bindings = car(args), body = car(cdr(args));
    int saved = env_top;
    for(u32 p = bindings; is_cons(p); p = cdr(p)){
      u32 b = car(p);
      SVal v = eval_ct(car(cdr(b)));
      if(v.k == SV_FAIL){ env_top = saved; return sv_fail(); }
      push_binding(car(b), v);
    }
    SVal r = eval_ct(body);
    env_top = saved;
    return r;
  }

  // Binary primitive — fold if both operands comptime.
  BinOp *bo = find_binop(head);
  if(bo){
    if(!is_cons(args) || !is_cons(cdr(args))) return sv_fail();
    SVal sa = eval_ct(car(args)), sb = eval_ct(car(cdr(args)));
    if(sa.k != SV_FIX || sb.k != SV_FIX) return sv_fail();
    i32 a = sa.fix, b = sb.fix;
    const char *op = bo->c_int_op;
    if(op[0]=='+' && op[1]==0) return sv_fix(a + b);
    if(op[0]=='-' && op[1]==0) return sv_fix(a - b);
    if(op[0]=='*' && op[1]==0) return sv_fix(a * b);
    if(op[0]=='<' && op[1]==0) return sv_fix(a < b);
    if(op[0]=='>' && op[1]==0) return sv_fix(a > b);
    if(op[0]=='=' && op[1]=='=') return sv_fix(a == b);
    if(op[0]=='%' && op[1]==0){ if(b==0) return sv_fail(); return sv_fix(a % b); }
    return sv_fail();
  }

  // Inline lambda head: ((lambda (params) body) args...)
  // Build closure, beta-reduce.
  if(is_cons(head) && sym_eq(car(head), "lambda")){
    u32 params = car(cdr(head)), body = car(cdr(cdr(head)));
    // Eval args first
    SVal vals[8]; int n = 0;
    for(u32 p = args; is_cons(p); p = cdr(p)){
      if(n >= 8) return sv_fail();
      vals[n] = eval_ct(car(p));
      if(vals[n].k == SV_FAIL) return sv_fail();
      n++;
    }
    int saved = env_top;
    u32 p = params; int i = 0;
    for(; is_cons(p) && i < n; p = cdr(p), i++) push_binding(car(p), vals[i]);
    SVal r = eval_ct(body);
    env_top = saved;
    return r;
  }

  // Named function call — try to inline (only if all args are comptime).
  if(is_sym(head)){
    Fn *f = find_define(head);
    if(!f) return sv_fail();
    SVal vals[8]; int n = 0;
    for(u32 p = args; is_cons(p); p = cdr(p)){
      if(n >= 8) return sv_fail();
      vals[n] = eval_ct(car(p));
      if(vals[n].k == SV_FAIL) return sv_fail();
      n++;
    }
    if(n != f->arity) return sv_fail();
    int saved = env_top;
    for(int i = 0; i < n; i++) push_binding(f->params[i], vals[i]);
    SVal r = eval_ct(f->body);
    env_top = saved;
    return r;
  }

  // Head is a more complex expression — try to eval it; if it yields a
  // closure, beta-reduce. This covers e.g. ((make-adder 5) n) when n is
  // runtime (the call still fails comptime because n is runtime; emit_expr
  // handles the runtime-args case there).
  SVal head_v = eval_ct(head);
  if(head_v.k == SV_LAMBDA){
    Closure *c = &closures[head_v.lam_idx];
    SVal vals[8]; int n = 0;
    for(u32 p = args; is_cons(p); p = cdr(p)){
      if(n >= 8) return sv_fail();
      vals[n] = eval_ct(car(p));
      if(vals[n].k == SV_FAIL) return sv_fail();
      n++;
    }
    int saved = env_top;
    for(int i = 0; i < c->n_cap; i++) env_stack[env_top++] = c->cap[i];
    u32 p = c->params; int i = 0;
    for(; is_cons(p) && i < n; p = cdr(p), i++) push_binding(car(p), vals[i]);
    SVal r = eval_ct(c->body);
    env_top = saved;
    return r;
  }

  return sv_fail();
}

// Bind a parallel list of params and RHS expressions, then emit `body` with
// the extended env. Used by `let`, inline lambda application, and closure
// beta-reduce at emit time. For each (param, rhs): evaluate rhs at spec time;
// if comptime (SV_FIX/SV_LAMBDA), push the binding inline. Otherwise emit a
// runtime C local and push BV_RUNTIME. If any binding is runtime, the whole
// construct is wrapped in a statement expression `({ ... })`.
static void bind_and_emit_body(FILE *out, u32 params, u32 rhs_list, u32 body){
  int n_runtime = 0;
  {
    u32 p = params, r = rhs_list;
    for(; is_cons(p) && is_cons(r); p = cdr(p), r = cdr(r)){
      SVal v = eval_ct(car(r));
      if(v.k == SV_FAIL) n_runtime++;
    }
  }

  int opened = 0;
  if(n_runtime > 0){ fputs("({", out); opened = 1; }

  int pushed = 0;
  {
    u32 p = params, r = rhs_list;
    for(; is_cons(p) && is_cons(r); p = cdr(p), r = cdr(r)){
      u32 name = car(p), rhs = car(r);
      SVal v = eval_ct(rhs);
      if(v.k == SV_FIX || v.k == SV_LAMBDA){
        push_binding(name, v);
      } else {
        fprintf(out, " u32 %s = ", symstr(name));
        emit_expr(out, rhs);
        fputs(";", out);
        Binding b = {name, BV_RUNTIME, 0, 0};
        env_stack[env_top++] = b;
      }
      pushed++;
    }
  }

  if(opened) fputc(' ', out);
  emit_expr(out, body);
  if(opened) fputs("; })", out);

  env_top -= pushed;
}

// Beta-reduce a closure at C emit time. Restores captured env on top of the
// current env, then delegates to bind_and_emit_body to handle the param/arg
// binding (which may have a mix of comptime and runtime args).
static void beta_reduce_emit(FILE *out, Closure *c, u32 args){
  int saved = env_top;
  for(int i = 0; i < c->n_cap; i++){
    if(env_top >= ENV_MAX){ fprintf(stderr, "specialize: env_stack overflow\n"); exit(1); }
    env_stack[env_top++] = c->cap[i];
  }
  bind_and_emit_body(out, c->params, args, c->body);
  env_top = saved;
}

// Convenience: emit (fixval(EXPR)) where EXPR is the C for node.
static void emit_fixval(FILE *out, u32 node){
  fputs("fixval(", out);
  emit_expr(out, node);
  fputc(')', out);
}

static void emit_expr(FILE *out, u32 node){
  if(is_fix(node)){
    fprintf(out, "mkfix(%d)", fixval(node));
    return;
  }
  if(is_sym(node)){
    // Check the spec-time env first. BV_FIX → substitute literal. BV_LAMBDA
    // → error (closures can't appear as runtime values). BV_RUNTIME or
    // unbound → emit the name (a C local or function parameter).
    Binding b;
    if(env_lookup(node, &b)){
      if(b.k == BV_FIX){ fprintf(out, "mkfix(%d)", b.fix); return; }
      if(b.k == BV_LAMBDA){
        fprintf(stderr, "specialize: closure `%s` escaped to a runtime value position\n", symstr(node));
        exit(1);
      }
    }
    fputs(symstr(node), out);
    return;
  }
  if(!is_cons(node)){
    fprintf(stderr, "specialize: unsupported atom (tag=%u)\n", tagof(node));
    exit(1);
  }
  u32 head = car(node), args = cdr(node);

  // (if c t e)
  if(sym_eq(head, "if")){
    u32 c = car(args), t = car(cdr(args)), e = car(cdr(cdr(args)));
    fputc('(', out);
    fputs("fixval(", out); emit_expr(out, c); fputc(')', out);
    fputs(" ? ", out); emit_expr(out, t);
    fputs(" : ", out); emit_expr(out, e);
    fputc(')', out);
    return;
  }

  // (let ((x V) ...) body) — bind each (x, V) at spec time; substitute
  // comptime RHSs inline, emit runtime RHSs as C locals.
  if(sym_eq(head, "let")){
    u32 bindings = car(args);
    u32 body = car(cdr(args));
    // Reshape (((x V) (y W) ...) body) into parallel param/rhs lists for
    // bind_and_emit_body. Easiest: build the two lists in cons-cell arenas.
    u32 params = NIL, last_p = NIL;
    u32 rhss   = NIL, last_r = NIL;
    for(u32 p = bindings; is_cons(p); p = cdr(p)){
      u32 b = car(p);
      u32 lp = cons(car(b), NIL);
      u32 lr = cons(car(cdr(b)), NIL);
      if(is_nil(params)){ params = lp; last_p = lp; } else { cells[considx(last_p)].cdr = lp; last_p = lp; }
      if(is_nil(rhss)){   rhss = lr;   last_r = lr; } else { cells[considx(last_r)].cdr = lr; last_r = lr; }
    }
    bind_and_emit_body(out, params, rhss, body);
    return;
  }

  // Inline lambda application: ((lambda (params) body) args...)
  if(is_cons(head) && sym_eq(car(head), "lambda")){
    u32 params = car(cdr(head));
    u32 body   = car(cdr(cdr(head)));
    bind_and_emit_body(out, params, args, body);
    return;
  }

  // Binary primitive
  BinOp *bo = find_binop(head);
  if(bo){
    u32 a = car(args), b = car(cdr(args));
    fputs("mkfix(", out);
    emit_fixval(out, a);
    fprintf(out, " %s ", bo->c_int_op);
    emit_fixval(out, b);
    fputc(')', out);
    return;
  }

  // Symbol head: either a closure bound via let/param (BV_LAMBDA →
  // beta-reduce at emit time), a named top-level function, or unknown.
  if(is_sym(head)){
    Binding b;
    if(env_lookup(head, &b) && b.k == BV_LAMBDA){
      beta_reduce_emit(out, &closures[b.lam_idx], args);
      return;
    }
    // Try spec-time inline: if every arg is comptime AND the body folds to
    // a comptime fixnum, emit the literal. Don't inline to a closure here —
    // closures aren't emittable as values.
    eval_ct_fuel = EVAL_CT_FUEL;
    SVal v = eval_ct(node);
    if(v.k == SV_FIX){ fprintf(out, "mkfix(%d)", v.fix); return; }
    // Runtime call.
    fputs(symstr(head), out);
    fputc('(', out);
    int first = 1;
    for(u32 p = args; is_cons(p); p = cdr(p)){
      if(!first) fputs(", ", out);
      first = 0;
      emit_expr(out, car(p));
    }
    fputc(')', out);
    return;
  }

  // Non-symbol head: must evaluate to a closure at spec time.
  // Covers ((make-adder 5) n) and similar.
  eval_ct_fuel = EVAL_CT_FUEL;
  SVal hv = eval_ct(head);
  if(hv.k == SV_LAMBDA){
    beta_reduce_emit(out, &closures[hv.lam_idx], args);
    return;
  }

  fprintf(stderr, "specialize: unsupported expression head (not a symbol, lambda, or comptime closure)\n");
  exit(1);
}

/* ---- top-level parsing of defines ----------------------------------------*/
// Recognize (define NAME (lambda (ARGS) BODY)) and (define (NAME ARGS) BODY).
static void record_define(u32 form){
  if(!is_cons(form) || !sym_eq(car(form), "define")){
    fprintf(stderr, "specialize: top-level forms must be `define`\n");
    exit(1);
  }
  u32 head = car(cdr(form));    // either NAME or (NAME ARGS...)
  u32 rest = cdr(cdr(form));    // remaining: either (LAMBDA) or (BODY)
  Fn *f = &fns[nfns++];

  if(is_sym(head)){
    // (define NAME (lambda (ARGS) BODY))
    f->name = name_of(head);
    u32 lam = car(rest);
    if(!is_cons(lam) || !sym_eq(car(lam), "lambda")){
      fprintf(stderr, "specialize: `define NAME ...` body must be a lambda\n");
      exit(1);
    }
    u32 params = car(cdr(lam));
    f->arity = 0;
    for(u32 p = params; is_cons(p); p = cdr(p)) f->params[f->arity++] = car(p);
    f->body = car(cdr(cdr(lam)));
  } else if(is_cons(head)){
    // (define (NAME ARGS...) BODY)
    f->name = name_of(car(head));
    f->arity = 0;
    for(u32 p = cdr(head); is_cons(p); p = cdr(p)) f->params[f->arity++] = car(p);
    f->body = car(rest);
  } else {
    fprintf(stderr, "specialize: malformed define\n");
    exit(1);
  }

  // If the body is a bare `(lambda ...)` form, this define is a closure
  // factory — only meaningful at spec time. Don't emit a runtime C function
  // for it; the residual won't reference it.
  f->comptime_only = is_cons(f->body) && sym_eq(car(f->body), "lambda");
}

/* ---- residual file emitter -----------------------------------------------*/
static void emit_residual(FILE *out){
  fputs(
    "// GENERATED by prototype/futamura/specialize.c — do not edit by hand.\n"
    "typedef unsigned int u32;\n"
    "typedef int          i32;\n"
    "#define mkfix(n)  ((u32)(((i32)(n)) << 2))\n"
    "#define fixval(v) (((i32)(v)) >> 2)\n\n",
    out);

  // Forward declarations for all RUNTIME-callable functions (skip comptime-only).
  for(int i = 0; i < nfns; i++){
    if(fns[i].comptime_only) continue;
    fprintf(out, "static u32 %s(", fns[i].name);
    for(int j = 0; j < fns[i].arity; j++){
      if(j) fputs(", ", out);
      fputs("u32 ", out);
    }
    fputs(");\n", out);
  }
  fputc('\n', out);

  // Function definitions (skip comptime-only — they exist only as spec-time helpers).
  for(int i = 0; i < nfns; i++){
    Fn *f = &fns[i];
    if(f->comptime_only) continue;
    fprintf(out, "static u32 %s(", f->name);
    for(int j = 0; j < f->arity; j++){
      if(j) fputs(", ", out);
      fprintf(out, "u32 %s", symstr(f->params[j]));
    }
    fputs("){\n  return ", out);
    emit_expr(out, f->body);
    fputs(";\n}\n\n", out);
  }

  // ABI shim. Entry point = last RUNTIME-callable function (skip comptime-only).
  int entry_i = nfns - 1;
  while(entry_i >= 0 && fns[entry_i].comptime_only) entry_i--;
  if(entry_i < 0){
    fprintf(stderr, "specialize: no runtime-callable entry point (all defines are comptime-only lambdas)\n");
    exit(1);
  }
  Fn *entry = &fns[entry_i];
  fputs(
    "#define INCAP  8192\n"
    "#define OUTCAP 4096\n"
    "static char inbuf[INCAP];\n"
    "static char outbuf[OUTCAP];\n"
    "static u32  outlen;\n\n"
    "static void emit(char c){ if(outlen<OUTCAP) outbuf[outlen++]=c; }\n"
    "static void emitint(i32 n){\n"
    "  if(n<0){ emit('-'); n=-n; }\n"
    "  char tmp[12]; int k=0;\n"
    "  if(n==0){ emit('0'); return; }\n"
    "  while(n){ tmp[k++]='0'+(n%10); n/=10; }\n"
    "  while(k) emit(tmp[--k]);\n"
    "}\n"
    "static u32 parse_int(u32 len, u32* i_io){\n"
    "  u32 i=*i_io;\n"
    "  while(i<len && (inbuf[i]==' '||inbuf[i]=='\\n'||inbuf[i]=='\\t'||inbuf[i]=='\\r')) i++;\n"
    "  int neg=0; if(i<len && inbuf[i]=='-'){ neg=1; i++; }\n"
    "  i32 n=0;\n"
    "  while(i<len && inbuf[i]>='0' && inbuf[i]<='9'){ n=n*10+(inbuf[i]-'0'); i++; }\n"
    "  if(neg) n=-n;\n"
    "  *i_io=i;\n"
    "  return mkfix(n);\n"
    "}\n\n"
    "__attribute__((export_name(\"input_ptr\")))  char* input_ptr(void){ return inbuf; }\n"
    "__attribute__((export_name(\"output_ptr\"))) char* output_ptr(void){ return outbuf; }\n"
    "__attribute__((export_name(\"eval_source\")))\n"
    "u32 eval_source(u32 len){\n  u32 i=0;\n",
    out);
  for(int j = 0; j < entry->arity; j++){
    fprintf(out, "  u32 a%d = parse_int(len, &i);\n", j);
  }
  fprintf(out, "  u32 r = %s(", entry->name);
  for(int j = 0; j < entry->arity; j++){
    if(j) fputs(", ", out);
    fprintf(out, "a%d", j);
  }
  fputs(
    ");\n"
    "  outlen=0;\n"
    "  emitint(fixval(r));\n"
    "  return outlen;\n"
    "}\n", out);
}

/* ---- driver ---------------------------------------------------------------*/
int main(void){
  // Read all of stdin.
  static char buf[1 << 20];
  size_t got = fread(buf, 1, sizeof buf, stdin);
  rp = buf; rend = buf + got;

  // Read all top-level forms.
  for(;;){
    skipws();
    if(rp >= rend) break;
    u32 form = read_expr();
    if(is_nil(form)) break;
    record_define(form);
  }
  if(nfns == 0){
    fprintf(stderr, "specialize: no defines found\n");
    return 1;
  }
  emit_residual(stdout);
  return 0;
}
