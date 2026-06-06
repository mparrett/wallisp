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

#define _GNU_SOURCE
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
  if(*rp == '\''){
    rp++;
    u32 q = intern("quote", 5);
    u32 e = read_expr();
    return cons(q, cons(e, NIL));
  }
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
typedef enum { BV_FIX, BV_RUNTIME, BV_LAMBDA, BV_NIL, BV_CONS } BVKind;
typedef struct {
  u32 name;
  BVKind k;
  i32 fix;
  int lam_idx;
  int cons_idx;
  int runtime_id;
} Binding;
#define ENV_MAX 128
static Binding env_stack[ENV_MAX];
static int env_top = 0;
static int next_runtime_id = 1;
static int current_emit_fn = -1;

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

// Spec-time value: a fixnum literal, a closure index, nil, a cons-cell index,
// or "couldn't fold."
typedef enum { SV_FAIL, SV_FIX, SV_LAMBDA, SV_NIL, SV_CONS } SVKind;
typedef struct { SVKind k; i32 fix; int lam_idx; int cons_idx; } SVal;
static SVal sv_fail(void){ SVal v = {SV_FAIL,0,0,0}; return v; }
static SVal sv_fix(i32 x){ SVal v = {SV_FIX,x,0,0}; return v; }
static SVal sv_lam(int i){ SVal v = {SV_LAMBDA,0,i,0}; return v; }
static SVal sv_nil(void){ SVal v = {SV_NIL,0,0,0}; return v; }
static SVal sv_cons(int i){ SVal v = {SV_CONS,0,0,i}; return v; }

typedef struct {
  SVal car;
  SVal cdr;
  int residual_idx;
} CtCons;
#define CT_CONS_MAX 65536
static CtCons ct_cons[CT_CONS_MAX];
static int n_ct_cons = 0;

static int make_ct_cons(SVal a, SVal d){
  if(n_ct_cons >= CT_CONS_MAX){ fprintf(stderr, "specialize: too many comptime cons cells\n"); exit(1); }
  int idx = n_ct_cons++;
  ct_cons[idx].car = a;
  ct_cons[idx].cdr = d;
  ct_cons[idx].residual_idx = -1;
  return idx;
}

// Top-level comptime values: `(define NAME EXPR)` where EXPR isn't a
// `(lambda ...)` form. The RHS is evaluated at spec time and the resulting
// SVal (typically SV_LAMBDA) is stored here, looked up later by symbol.
typedef struct { const char *name; SVal v; } CompDef;
#define COMPDEFS_MAX 64
static CompDef comp_defs[COMPDEFS_MAX];
static int n_comp_defs = 0;

// Fuel: bound inlining work per spec-time evaluation. Prevents recursive
// functions like fib from inlining unboundedly when called with a comptime arg.
#define EVAL_CT_FUEL 250000
static int eval_ct_fuel = EVAL_CT_FUEL;

static int env_lookup(u32 sym, Binding *out){
  for(int i = env_top - 1; i >= 0; i--)
    if(env_stack[i].name == sym){ *out = env_stack[i]; return 1; }
  return 0;
}

static int runtime_binding_is_active(int runtime_id){
  for(int i = env_top - 1; i >= 0; i--)
    if(env_stack[i].k == BV_RUNTIME && env_stack[i].runtime_id == runtime_id) return 1;
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
  if(env_top >= ENV_MAX){ fprintf(stderr, "specialize: env_stack overflow\n"); exit(1); }
  Binding b = { name, BV_RUNTIME, 0, 0, 0, -1 };
  if(v.k == SV_FIX){ b.k = BV_FIX; b.fix = v.fix; }
  else if(v.k == SV_LAMBDA){ b.k = BV_LAMBDA; b.lam_idx = v.lam_idx; }
  else if(v.k == SV_NIL){ b.k = BV_NIL; }
  else if(v.k == SV_CONS){ b.k = BV_CONS; b.cons_idx = v.cons_idx; }
  env_stack[env_top++] = b;
}

static void push_runtime_binding(u32 name){
  if(env_top >= ENV_MAX){ fprintf(stderr, "specialize: env_stack overflow\n"); exit(1); }
  Binding b = {name, BV_RUNTIME, 0, 0, 0, next_runtime_id++};
  env_stack[env_top++] = b;
}

static void check_closure_runtime_captures(Closure *c){
  for(int i = 0; i < c->n_cap; i++){
    Binding *b = &c->cap[i];
    if(b->k != BV_RUNTIME) continue;
    if(current_emit_fn < 0 || b->runtime_id < 0 || !runtime_binding_is_active(b->runtime_id)){
      fprintf(stderr, "specialize: closure captured runtime binding `%s` after its C scope ended\n", symstr(b->name));
      exit(1);
    }
  }
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

// Compare a Lisp symbol (with on-the-fly mangling) to an already-mangled
// C name. Used by both find_define and find_comp_def.
static int sym_matches_mangled(u32 sym, const char *mangled){
  u32 i = symidx(sym);
  if(strlen(mangled) != symlen[i]) return 0;
  for(u32 j = 0; j < symlen[i]; j++){
    if(c_mangle_char(symname[i][j]) != mangled[j]) return 0;
  }
  return 1;
}

// Look up a top-level define by symbol.
static Fn* find_define(u32 sym){
  if(!is_sym(sym)) return NULL;
  for(int k = 0; k < nfns; k++)
    if(sym_matches_mangled(sym, fns[k].name)) return &fns[k];
  return NULL;
}

// Look up a top-level comptime value (`(define NAME EXPR)`) by symbol.
static CompDef* find_comp_def(u32 sym){
  if(!is_sym(sym)) return NULL;
  for(int k = 0; k < n_comp_defs; k++)
    if(sym_matches_mangled(sym, comp_defs[k].name)) return &comp_defs[k];
  return NULL;
}

static void reject_mangled_top_level_collision(u32 sym){
  for(int k = 0; k < nfns; k++){
    if(sym_matches_mangled(sym, fns[k].name)){
      fprintf(stderr, "specialize: top-level name `%s` collides with an existing C identifier after mangling\n", symstr(sym));
      exit(1);
    }
  }
  for(int k = 0; k < n_comp_defs; k++){
    if(sym_matches_mangled(sym, comp_defs[k].name)){
      fprintf(stderr, "specialize: top-level name `%s` collides with an existing C identifier after mangling\n", symstr(sym));
      exit(1);
    }
  }
}

static int expr_can_yield_closure(u32 node){
  if(is_sym(node)){
    CompDef *cd = find_comp_def(node);
    return cd && cd->v.k == SV_LAMBDA;
  }
  if(!is_cons(node)) return 0;

  u32 head = car(node), args = cdr(node);
  if(sym_eq(head, "lambda")) return 1;
  if(sym_eq(head, "if")){
    u32 t = car(cdr(args)), e = car(cdr(cdr(args)));
    return expr_can_yield_closure(t) || expr_can_yield_closure(e);
  }
  if(sym_eq(head, "let")){
    return expr_can_yield_closure(car(cdr(args)));
  }
  if(is_sym(head)){
    Fn *f = find_define(head);
    return f && f->comptime_only;
  }
  return 0;
}

static int expr_can_yield_cons(u32 node){
  if(is_sym(node)){
    CompDef *cd = find_comp_def(node);
    return cd && cd->v.k == SV_CONS;
  }
  if(!is_cons(node)) return 0;

  u32 head = car(node), args = cdr(node);
  if(sym_eq(head, "cons") || sym_eq(head, "quote")) return 1;
  if(sym_eq(head, "if")){
    u32 t = car(cdr(args)), e = car(cdr(cdr(args)));
    return expr_can_yield_cons(t) || expr_can_yield_cons(e);
  }
  if(sym_eq(head, "let")){
    return expr_can_yield_cons(car(cdr(args)));
  }
  if(is_sym(head)){
    Fn *f = find_define(head);
    return f && f->comptime_only;
  }
  return 0;
}

static int expr_uses_cons_primitive(u32 node){
  if(!is_cons(node)) return 0;
  u32 head = car(node), args = cdr(node);
  if(sym_eq(head, "cons") || sym_eq(head, "car") || sym_eq(head, "cdr")) return 1;
  if(sym_eq(head, "quote") && is_cons(car(args))) return 1;
  if(is_cons(head) && expr_uses_cons_primitive(head)) return 1;
  for(u32 p = args; is_cons(p); p = cdr(p))
    if(expr_uses_cons_primitive(car(p))) return 1;
  return 0;
}

static SVal quote_to_sval(u32 node){
  if(is_nil(node)) return sv_nil();
  if(is_fix(node)) return sv_fix(fixval(node));
  if(is_cons(node)){
    SVal a = quote_to_sval(car(node));
    SVal d = quote_to_sval(cdr(node));
    if(a.k == SV_FAIL || d.k == SV_FAIL) return sv_fail();
    return sv_cons(make_ct_cons(a, d));
  }
  return sv_fail();
}

static void fail_runtime_cons(const char *prim){
  fprintf(stderr, "specialize: runtime %s not yet supported (B-v3 v1 is comptime-only)\n", prim);
  exit(1);
}

// Try to evaluate `node` to a comptime SVal (fixnum or closure) using the
// current env. Returns SV_FAIL if any subexpression hits runtime or fuel runs
// out. The call site uses the result to decide whether to substitute inline
// (SV_FIX/SV_LAMBDA) or emit a runtime C expression (SV_FAIL).
static SVal eval_ct(u32 node){
  if(--eval_ct_fuel < 0) return sv_fail();

  if(is_fix(node)) return sv_fix(fixval(node));
  if(is_nil(node)) return sv_nil();

  if(is_sym(node)){
    Binding b;
    if(env_lookup(node, &b)){
      if(b.k == BV_FIX)    return sv_fix(b.fix);
      if(b.k == BV_LAMBDA) return sv_lam(b.lam_idx);
      if(b.k == BV_NIL)    return sv_nil();
      if(b.k == BV_CONS)   return sv_cons(b.cons_idx);
      return sv_fail();  // BV_RUNTIME — not comptime
    }
    CompDef *cd = find_comp_def(node);
    if(cd) return cd->v;
    return sv_fail();    // unbound — usually a function parameter (runtime)
  }

  if(!is_cons(node)) return sv_fail();
  u32 head = car(node), args = cdr(node);

  // (quote DATA) — turn quoted proper lists into comptime cons trees.
  if(sym_eq(head, "quote")){
    return quote_to_sval(car(args));
  }

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

  if(sym_eq(head, "cons")){
    SVal a = eval_ct(car(args));
    SVal d = eval_ct(car(cdr(args)));
    if(a.k == SV_FAIL || d.k == SV_FAIL) fail_runtime_cons("cons");
    return sv_cons(make_ct_cons(a, d));
  }

  if(sym_eq(head, "car") || sym_eq(head, "cdr")){
    SVal v = eval_ct(car(args));
    if(v.k == SV_FAIL) fail_runtime_cons(sym_eq(head, "car") ? "car" : "cdr");
    if(v.k != SV_CONS) return sv_fail();
    CtCons *c = &ct_cons[v.cons_idx];
    return sym_eq(head, "car") ? c->car : c->cdr;
  }

  if(sym_eq(head, "null?")){
    SVal v = eval_ct(car(args));
    if(v.k == SV_NIL) return sv_fix(1);
    if(v.k == SV_CONS) return sv_fix(0);
    return sv_fail();
  }

  if(sym_eq(head, "pair?")){
    SVal v = eval_ct(car(args));
    if(v.k == SV_NIL) return sv_fix(0);
    if(v.k == SV_CONS) return sv_fix(1);
    return sv_fail();
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
// if comptime (SV_FIX/SV_LAMBDA/SV_NIL/SV_CONS), push the binding inline.
// Otherwise emit a runtime C local and push BV_RUNTIME. If any binding is runtime, the whole
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
      if(v.k == SV_FIX || v.k == SV_LAMBDA || v.k == SV_NIL || v.k == SV_CONS){
        push_binding(name, v);
      } else {
        if(expr_can_yield_closure(rhs)){
          fprintf(stderr, "specialize: closure-valued let binding `%s` depends on runtime data\n", symstr(name));
          exit(1);
        }
        if(expr_can_yield_cons(rhs)){
          fprintf(stderr, "specialize: cons-valued let binding `%s` depends on runtime data\n", symstr(name));
          exit(1);
        }
        fprintf(out, " u32 %s = ", symstr(name));
        emit_expr(out, rhs);
        fputs(";", out);
        push_runtime_binding(name);
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
  check_closure_runtime_captures(c);
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

static int residual_cons_order[CT_CONS_MAX];
static int n_residual_cons = 0;

static int materialize_cons_idx(int ct_idx){
  CtCons *c = &ct_cons[ct_idx];
  if(c->residual_idx >= 0) return c->residual_idx;
  int idx = n_residual_cons++;
  c->residual_idx = idx;
  residual_cons_order[idx] = ct_idx;
  if(c->car.k == SV_CONS) materialize_cons_idx(c->car.cons_idx);
  if(c->cdr.k == SV_CONS) materialize_cons_idx(c->cdr.cons_idx);
  return idx;
}

static void emit_sval(FILE *out, SVal v){
  if(v.k == SV_FIX){ fprintf(out, "mkfix(%d)", v.fix); return; }
  if(v.k == SV_NIL){ fputs("NIL", out); return; }
  if(v.k == SV_CONS){
    fprintf(out, "mkcons(%d)", materialize_cons_idx(v.cons_idx));
    return;
  }
  if(v.k == SV_LAMBDA){
    fprintf(stderr, "specialize: closure escaped to a runtime value position\n");
    exit(1);
  }
  fprintf(stderr, "specialize: internal error: tried to emit failed comptime value\n");
  exit(1);
}

static void emit_sval_initializer(FILE *out, SVal v){
  if(v.k == SV_FIX){ fprintf(out, "mkfix(%d)", v.fix); return; }
  if(v.k == SV_NIL){ fputs("NIL", out); return; }
  if(v.k == SV_CONS){
    fprintf(out, "mkcons(%d)", materialize_cons_idx(v.cons_idx));
    return;
  }
  fprintf(stderr, "specialize: internal error: non-data value in comptime cons pool\n");
  exit(1);
}

static void emit_expr(FILE *out, u32 node){
  if(is_fix(node)){
    fprintf(out, "mkfix(%d)", fixval(node));
    return;
  }
  if(is_nil(node)){
    fputs("NIL", out);
    return;
  }
  if(is_sym(node)){
    // Check the spec-time env first. BV_FIX → substitute literal. BV_LAMBDA
    // → error (closures can't appear as runtime values). BV_RUNTIME or
    // unbound → emit the name (a C local or function parameter), or substitute
    // a top-level comptime value if one is registered.
    Binding b;
    if(env_lookup(node, &b)){
      if(b.k == BV_FIX){ fprintf(out, "mkfix(%d)", b.fix); return; }
      if(b.k == BV_LAMBDA){
        fprintf(stderr, "specialize: closure `%s` escaped to a runtime value position\n", symstr(node));
        exit(1);
      }
      if(b.k == BV_NIL){ fputs("NIL", out); return; }
      if(b.k == BV_CONS){ emit_sval(out, sv_cons(b.cons_idx)); return; }
    }
    CompDef *cd = find_comp_def(node);
    if(cd){
      if(cd->v.k == SV_FIX){ fprintf(out, "mkfix(%d)", cd->v.fix); return; }
      if(cd->v.k == SV_LAMBDA){
        fprintf(stderr, "specialize: closure-valued top-level `%s` escaped to a runtime value position\n", symstr(node));
        exit(1);
      }
      if(cd->v.k == SV_NIL || cd->v.k == SV_CONS){ emit_sval(out, cd->v); return; }
    }
    fputs(symstr(node), out);
    return;
  }
  if(!is_cons(node)){
    fprintf(stderr, "specialize: unsupported atom (tag=%u)\n", tagof(node));
    exit(1);
  }
  u32 head = car(node), args = cdr(node);

  if(sym_eq(head, "lambda")){
    fprintf(stderr, "specialize: lambda escaped to a runtime value position\n");
    exit(1);
  }

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

  if(sym_eq(head, "cons")){
    eval_ct_fuel = EVAL_CT_FUEL;
    SVal v = eval_ct(node);
    emit_sval(out, v);
    return;
  }

  if(sym_eq(head, "car") || sym_eq(head, "cdr")){
    eval_ct_fuel = EVAL_CT_FUEL;
    SVal v = eval_ct(node);
    if(v.k == SV_FAIL) fail_runtime_cons(sym_eq(head, "car") ? "car" : "cdr");
    emit_sval(out, v);
    return;
  }

  if(sym_eq(head, "quote")){
    eval_ct_fuel = EVAL_CT_FUEL;
    SVal v = eval_ct(node);
    emit_sval(out, v);
    return;
  }

  if(sym_eq(head, "null?") || sym_eq(head, "pair?")){
    eval_ct_fuel = EVAL_CT_FUEL;
    SVal v = eval_ct(node);
    if(v.k == SV_FIX){ emit_sval(out, v); return; }
    fputs("mkfix(", out);
    fputs(sym_eq(head, "null?") ? "is_nil(" : "is_cons(", out);
    emit_expr(out, car(args));
    fputs("))", out);
    return;
  }

  // Symbol head: either a closure bound via let/param (BV_LAMBDA →
  // beta-reduce at emit time), a top-level comptime closure value, a named
  // top-level function, or unknown.
  if(is_sym(head)){
    Binding b;
    if(env_lookup(head, &b) && b.k == BV_LAMBDA){
      beta_reduce_emit(out, &closures[b.lam_idx], args);
      return;
    }
    CompDef *cd = find_comp_def(head);
    if(cd && cd->v.k == SV_LAMBDA){
      beta_reduce_emit(out, &closures[cd->v.lam_idx], args);
      return;
    }
    // Try spec-time inline: if every arg is comptime AND the body folds to
    // a comptime fixnum, emit the literal. Don't inline to a closure here —
    // closures aren't emittable as values.
    eval_ct_fuel = EVAL_CT_FUEL;
    SVal v = eval_ct(node);
    if(v.k == SV_FIX || v.k == SV_NIL || v.k == SV_CONS){ emit_sval(out, v); return; }
    Fn *f = find_define(head);
    if(f && f->comptime_only){
      if(expr_can_yield_closure(f->body))
        fprintf(stderr, "specialize: comptime-only closure-valued function `%s` called with runtime arguments\n", symstr(head));
      else
        fprintf(stderr, "specialize: comptime-only cons-dependent function `%s` called with runtime arguments\n", symstr(head));
      exit(1);
    }
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
  if(is_cons(head) && is_sym(car(head))){
    Fn *f = find_define(car(head));
    if(f && f->comptime_only){
      fprintf(stderr, "specialize: comptime-only closure-valued function `%s` called with runtime arguments\n", symstr(car(head)));
      exit(1);
    }
  }

  fprintf(stderr, "specialize: unsupported expression head (not a symbol, lambda, or comptime closure)\n");
  exit(1);
}

/* ---- top-level parsing of defines ----------------------------------------*/
// Three accepted shapes:
//   (define (NAME ARGS...) BODY)         — function form, runtime C function
//   (define NAME (lambda (ARGS) BODY))   — equivalent to above, runtime C function
//   (define NAME EXPR)                   — comptime value; eval EXPR at spec time,
//                                          store as CompDef. EXPR must fold
//                                          (SV_FIX/SV_LAMBDA/SV_NIL/SV_CONS);
//                                          SV_FAIL hard-errors.
static void record_define(u32 form){
  if(!is_cons(form) || !sym_eq(car(form), "define")){
    fprintf(stderr, "specialize: top-level forms must be `define`\n");
    exit(1);
  }
  u32 head = car(cdr(form));    // either NAME or (NAME ARGS...)
  u32 rest = cdr(cdr(form));    // remaining: either (LAMBDA), (BODY), or (EXPR)

  if(is_cons(head)){
    // (define (NAME ARGS...) BODY)
    reject_mangled_top_level_collision(car(head));
    Fn *f = &fns[nfns++];
    f->name = name_of(car(head));
    f->arity = 0;
    for(u32 p = cdr(head); is_cons(p); p = cdr(p)) f->params[f->arity++] = car(p);
    f->body = car(rest);
    f->comptime_only = 0;
    f->comptime_only = expr_can_yield_closure(f->body) || expr_can_yield_cons(f->body) || expr_uses_cons_primitive(f->body);
    return;
  }

  if(!is_sym(head)){
    fprintf(stderr, "specialize: malformed define\n");
    exit(1);
  }

  u32 val_expr = car(rest);

  if(is_cons(val_expr) && sym_eq(car(val_expr), "lambda")){
    // (define NAME (lambda (ARGS) BODY))
    reject_mangled_top_level_collision(head);
    Fn *f = &fns[nfns++];
    f->name = name_of(head);
    u32 params = car(cdr(val_expr));
    f->arity = 0;
    for(u32 p = params; is_cons(p); p = cdr(p)) f->params[f->arity++] = car(p);
    f->body = car(cdr(cdr(val_expr)));
    f->comptime_only = 0;
    f->comptime_only = expr_can_yield_closure(f->body) || expr_can_yield_cons(f->body) || expr_uses_cons_primitive(f->body);
    return;
  }

  // (define NAME EXPR) — comptime value binding.
  reject_mangled_top_level_collision(head);
  eval_ct_fuel = EVAL_CT_FUEL;
  SVal v = eval_ct(val_expr);
  if(v.k == SV_FAIL){
    fprintf(stderr, "specialize: top-level (define %s ...) RHS doesn't fold at spec time\n", symstr(head));
    exit(1);
  }
  if(n_comp_defs >= COMPDEFS_MAX){
    fprintf(stderr, "specialize: too many top-level comptime defines\n");
    exit(1);
  }
  CompDef *cd = &comp_defs[n_comp_defs++];
  cd->name = name_of(head);
  cd->v = v;
}

/* ---- residual file emitter -----------------------------------------------*/
static void emit_residual(FILE *out){
  int entry_i = nfns - 1;
  while(entry_i >= 0 && fns[entry_i].comptime_only) entry_i--;
  if(entry_i < 0 && nfns > 0) entry_i = nfns - 1;
  if(entry_i < 0){
    fprintf(stderr, "specialize: no runtime-callable entry point (all defines are comptime-only lambdas)\n");
    exit(1);
  }

  char *body_buf = NULL;
  size_t body_len = 0;
  FILE *body_out = open_memstream(&body_buf, &body_len);
  if(!body_out){ fprintf(stderr, "specialize: open_memstream failed\n"); exit(1); }

  // Forward declarations for all RUNTIME-callable functions (skip
  // comptime-only helpers unless they are the forced entry used to produce a
  // precise runtime-cons diagnostic).
  for(int i = 0; i < nfns; i++){
    if(fns[i].comptime_only && i != entry_i) continue;
    fprintf(body_out, "static u32 %s(", fns[i].name);
    for(int j = 0; j < fns[i].arity; j++){
      if(j) fputs(", ", body_out);
      fputs("u32 ", body_out);
    }
    fputs(");\n", body_out);
  }
  fputc('\n', body_out);

  // Function definitions (skip comptime-only helpers — they exist only as
  // spec-time helpers — unless this is the forced entry described above).
  for(int i = 0; i < nfns; i++){
    Fn *f = &fns[i];
    if(f->comptime_only && i != entry_i) continue;
    fprintf(body_out, "static u32 %s(", f->name);
    for(int j = 0; j < f->arity; j++){
      if(j) fputs(", ", body_out);
      fprintf(body_out, "u32 %s", symstr(f->params[j]));
    }
    fputs("){\n  return ", body_out);
    int saved = env_top;
    current_emit_fn = i;
    for(int j = 0; j < f->arity; j++) push_runtime_binding(f->params[j]);
    emit_expr(body_out, f->body);
    env_top = saved;
    current_emit_fn = -1;
    fputs(";\n}\n\n", body_out);
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
    body_out);
  for(int j = 0; j < entry->arity; j++){
    fprintf(body_out, "  u32 a%d = parse_int(len, &i);\n", j);
  }
  fprintf(body_out, "  u32 r = %s(", entry->name);
  for(int j = 0; j < entry->arity; j++){
    if(j) fputs(", ", body_out);
    fprintf(body_out, "a%d", j);
  }
  fputs(
    ");\n"
    "  outlen=0;\n"
    "  emitint(fixval(r));\n"
    "  return outlen;\n"
    "}\n", body_out);

  fclose(body_out);

  fputs(
    "// GENERATED by prototype/futamura/specialize.c — do not edit by hand.\n"
    "typedef unsigned int u32;\n"
    "typedef int          i32;\n"
    "#define TAG_MASK  3u\n"
    "#define TAG_FIX   0u\n"
    "#define TAG_CONS  1u\n"
    "#define TAG_SPEC  3u\n"
    "#define mkfix(n)   ((u32)(((i32)(n)) << 2) | TAG_FIX)\n"
    "#define fixval(v)  (((i32)(v)) >> 2)\n"
    "#define mkcons(i)  (((u32)(i) << 2) | TAG_CONS)\n"
    "#define considx(v) ((v) >> 2)\n"
    "#define mkspec(i)  (((u32)(i) << 2) | TAG_SPEC)\n"
    "#define tagof(v)   ((v) & TAG_MASK)\n"
    "#define is_cons(v) (tagof(v)==TAG_CONS)\n"
    "enum { SP_NIL=0, SP_T };\n"
    "#define NIL   mkspec(SP_NIL)\n"
    "#define is_nil(v) ((v)==NIL)\n"
    "typedef struct { u32 car, cdr; } Cell;\n\n",
    out);

  if(n_residual_cons > 0){
    fprintf(out, "static const Cell ct_pool[%d] = {\n", n_residual_cons);
    for(int i = 0; i < n_residual_cons; i++){
      CtCons *c = &ct_cons[residual_cons_order[i]];
      fputs("  { ", out);
      emit_sval_initializer(out, c->car);
      fputs(", ", out);
      emit_sval_initializer(out, c->cdr);
      fputs(" },\n", out);
    }
    fputs("};\n\n", out);
  }

  fwrite(body_buf, 1, body_len, out);
  free(body_buf);
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
