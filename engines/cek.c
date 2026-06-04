// lisp.c — a tiny Lisp, freestanding, compiles to wasm32 (no libc).
//
// Design in one breath:
//   * Values are 32-bit tagged words. Low 2 bits = tag.
//       00 -> fixnum (30-bit signed, value = word >> 2)
//       01 -> cons cell  (upper 30 bits = index into cell arena)
//       10 -> symbol     (upper 30 bits = index into symbol table)
//       11 -> special    (NIL, T, primitive ids, error)
//   * Heap is a fixed bump arena of cons cells. No GC (see ARENA note).
//   * Environments are assoc-lists of (symbol . value), chained — lexical scope.
//   * The reader, evaluator, and a printer all live in this one file.
//
// Why these tradeoffs: tagged ints + cons-only heap is the smallest core that
// still runs recursive list programs. NaN-boxing buys nothing at 32-bit. A
// real GC is ~all the remaining complexity and none of the idea, so the arena
// is a constant you can later turn into a semispace collector.

typedef unsigned int   u32;
typedef int            i32;
typedef unsigned char  u8;

// ---- tag scheme ------------------------------------------------------------
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

// ---- special / singleton values -------------------------------------------
// id 0..N reserved. Primitives get ids too so they're first-class callables.
enum {
  SP_NIL=0, SP_T, SP_ERR, SP_UNBOUND,
  // primitives:
  PR_CONS, PR_CAR, PR_CDR, PR_ADD, PR_SUB, PR_MUL, PR_DIV, PR_MOD, PR_EQ, PR_LT,
  PR_NULLP, PR_PAIRP, PR_LISTQ, PR_NUMBERP, PR_SYMBOLP, PR_SETCAR, PR_SETCDR, PR_CALLCC,
  SP_COUNT
};
#define NIL      mkspec(SP_NIL)
#define TRUE     mkspec(SP_T)
#define ERR      mkspec(SP_ERR)
#define UNBOUND  mkspec(SP_UNBOUND)
#define is_nil(v) ((v)==NIL)
#define is_prim(v) (tagof(v)==TAG_SPEC && ((v)>>2) >= PR_CONS && ((v)>>2) < SP_COUNT)

// ---- arenas ----------------------------------------------------------------
// ARENA: bump-only. When `cell_top` hits MAX_CELLS we return ERR instead of
// scribbling. Swap this region for a semispace + forwarding to add GC.
#define MAX_CELLS   131072
typedef struct { u32 car, cdr; } Cell;
static Cell  cells[MAX_CELLS];
static u32   cell_top = 0;
static int   g_oom = 0;   // set when the arena is exhausted; checked each CEK step

#define MAX_SYMS    1024
#define SYM_CHARS   16
static char  symname[MAX_SYMS][SYM_CHARS];
static u32   symlen[MAX_SYMS];
static u32   sym_top = 0;

static u32 cons(u32 a, u32 d){
  if (cell_top >= MAX_CELLS) { g_oom=1; return ERR; }
  u32 i = cell_top++;
  cells[i].car = a; cells[i].cdr = d;
  return mkcons(i);
}
static u32 car(u32 v){ return is_cons(v) ? cells[considx(v)].car : NIL; }
static u32 cdr(u32 v){ return is_cons(v) ? cells[considx(v)].cdr : NIL; }

// intern a symbol by name
static int streq(const char*a,u32 la,const char*b,u32 lb){
  if(la!=lb) return 0; for(u32 i=0;i<la;i++) if(a[i]!=b[i]) return 0; return 1;
}
static u32 intern(const char* s, u32 len){
  if(len>SYM_CHARS) len=SYM_CHARS;
  for(u32 i=0;i<sym_top;i++)
    if(streq(symname[i],symlen[i],s,len)) return mksym(i);
  if(sym_top>=MAX_SYMS) return ERR;
  u32 i=sym_top++;
  for(u32 k=0;k<len;k++) symname[i][k]=s[k];
  symlen[i]=len;
  return mksym(i);
}

// cached symbols for special forms (filled in init)
static u32 s_quote,s_if,s_define,s_lambda,s_let,s_begin,s_cond,s_else,s_setbang;

// ---- reader ----------------------------------------------------------------
// Hand-rolled recursive descent over a source buffer. Supports ints (with
// leading '-'), symbols, lists, and 'quote shorthand. Good enough; no strings,
// no dotted pairs, no #t literals (we expose nil/t as symbols bound in env).
static const char* rp;
static const char* rend;

static void skipws(){
  while(rp<rend){
    char c=*rp;
    if(c==' '||c=='\t'||c=='\n'||c=='\r'){rp++;}
    else if(c==';'){ while(rp<rend && *rp!='\n') rp++; }
    else break;
  }
}
static int is_delim(char c){
  return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='('||c==')'||c==';'||c==0;
}
static u32 read_expr();

static u32 read_list(){
  u32 head=NIL, *tailp=&head; // we build via index since head may move? no—cons returns stable
  // Build with explicit append using a sentinel approach:
  u32 first=NIL; u32 last=NIL;
  for(;;){
    skipws();
    if(rp>=rend) return first;            // tolerate missing ')'
    if(*rp==')'){ rp++; return first; }
    u32 e=read_expr();
    u32 link=cons(e,NIL);
    if(link==ERR) return ERR;
    if(is_nil(first)){ first=link; last=link; }
    else { cells[considx(last)].cdr=link; last=link; }
  }
  (void)tailp;
}
static u32 read_atom(){
  const char* start=rp;
  while(rp<rend && !is_delim(*rp)) rp++;
  u32 len=(u32)(rp-start);
  // integer?
  int neg=0; const char* p=start; u32 n=len;
  if(n>0 && (*p=='-'||*p=='+')){ neg=(*p=='-'); p++; n--; }
  if(n>0){
    int isnum=1; i32 val=0;
    for(u32 i=0;i<n;i++){ char c=p[i]; if(c<'0'||c>'9'){isnum=0;break;} val=val*10+(c-'0'); }
    if(isnum) return mkfix(neg?-val:val);
  }
  return intern(start,len);
}
static u32 read_expr(){
  skipws();
  if(rp>=rend) return ERR;
  char c=*rp;
  if(c=='('){ rp++; return read_list(); }
  if(c=='\''){ rp++; u32 e=read_expr(); return cons(s_quote,cons(e,NIL)); }
  return read_atom();
}

// ---- environment -----------------------------------------------------------
// env is a list of frames; each frame is a list of (sym . val) cons cells.
// Flat assoc-list is simplest; lookups are O(n) but n is tiny for a demo.
static u32 env_lookup(u32 env, u32 sym){
  for(u32 f=env; is_cons(f); f=cdr(f)){
    u32 binding=car(f);
    if(is_cons(binding) && car(binding)==sym) return cdr(binding);
  }
  return UNBOUND;
}
static u32 env_define(u32 env, u32 sym, u32 val){
  return cons(cons(sym,val), env);
}
// PR2: walk env, mutate the binding cons's cdr in place. See engines/lisp.c.
static u32 env_set(u32 env, u32 sym, u32 val){
  for(u32 f=env; is_cons(f); f=cdr(f)){
    u32 binding=car(f);
    if(is_cons(binding) && car(binding)==sym){
      cells[considx(binding)].cdr = val;
      return val;
    }
  }
  return UNBOUND;
}
// Global define mutates IN PLACE by splicing after a stable sentinel head cell,
// so closures that captured the global env earlier still observe new bindings
// (this is what makes top-level recursion work). g_head is that sentinel.
static u32 g_head; // a cons whose cdr is the live global binding list
static void global_define(u32 sym, u32 val){
  u32 link=cons(cons(sym,val), cdr(g_head));
  cells[considx(g_head)].cdr = link;
}

// ---- closures --------------------------------------------------------------
// A lambda is represented as a cons-tagged list: (params body . env) but we
// need to distinguish it from data lists at apply time. We tag it by making
// the car a reserved symbol "%closure". Simple and works with the printer.
static u32 s_closure;
static u32 make_closure(u32 params, u32 body, u32 env){
  return cons(s_closure, cons(params, cons(body, cons(env, NIL))));
}
static int is_closure(u32 v){ return is_cons(v) && car(v)==s_closure; }

// EXP2: call/cc. A reified continuation is just a cons (%cont . K). On apply,
// we restore the captured K and return_val into it — the saved K already
// chains all the way back to K_HALT, so invoking it terminates the current
// computation and resumes wherever the call/cc was. No extra root: the cont
// keeps the K reachable through its cdr.
static u32 s_cont;
static u32 make_cont(u32 K){ return cons(s_cont, K); }
static int is_cont(u32 v){ return is_cons(v) && car(v)==s_cont; }
static u32 cont_k(u32 v){ return cdr(v); }

// PR1: see engines/lisp.c for the design notes. Inline arity check,
// operand type checks, 30-bit overflow trap, polymorphic = (metacircular
// eval depends on it for symbol identity). +/-/* variadic with ≥2 args.
#define FIX_MAX  ((i32) 0x1FFFFFFF)   //  536870911
#define FIX_MIN  (-(i32)0x20000000)   // -536870912
static int fits_fix(i32 v){ return v >= FIX_MIN && v <= FIX_MAX; }
static int fits_fix64(long long v){ return v >= FIX_MIN && v <= FIX_MAX; }

static u32 apply_prim(u32 prim, u32 args){
  u32 id=prim>>2;
  if(!is_cons(args)) return ERR;
  u32 a  = car(args);
  u32 d0 = cdr(args);

  switch(id){
    case PR_NULLP: if(!is_nil(d0)) return ERR; return is_nil(a)?TRUE:NIL;
    case PR_PAIRP: if(!is_nil(d0)) return ERR; return is_cons(a)?TRUE:NIL;
    case PR_LISTQ: if(!is_nil(d0)) return ERR; return (is_nil(a)||is_cons(a))?TRUE:NIL;
    case PR_NUMBERP: if(!is_nil(d0)) return ERR; return is_fix(a)?TRUE:NIL;
    case PR_SYMBOLP: if(!is_nil(d0)) return ERR; return is_sym(a)?TRUE:NIL;
    case PR_CAR:   if(!is_nil(d0) || !is_cons(a)) return ERR; return cells[considx(a)].car;
    case PR_CDR:   if(!is_nil(d0) || !is_cons(a)) return ERR; return cells[considx(a)].cdr;
  }

  if(!is_cons(d0)) return ERR;
  u32 b  = car(d0);
  u32 d1 = cdr(d0);

  switch(id){
    case PR_CONS:  if(!is_nil(d1)) return ERR; return cons(a,b);
    case PR_SETCAR: if(!is_nil(d1) || !is_cons(a)) return ERR;
                    cells[considx(a)].car = b; return b;
    case PR_SETCDR: if(!is_nil(d1) || !is_cons(a)) return ERR;
                    cells[considx(a)].cdr = b; return b;
    case PR_EQ:    if(!is_nil(d1)) return ERR; return (a==b)?TRUE:NIL;
    case PR_LT:    if(!is_nil(d1) || !is_fix(a) || !is_fix(b)) return ERR;
                   return (fixval(a)<fixval(b))?TRUE:NIL;
    case PR_DIV: {
      if(!is_nil(d1) || !is_fix(a) || !is_fix(b)) return ERR;
      i32 bv=fixval(b); if(bv==0) return ERR;
      i32 r=fixval(a)/bv; if(!fits_fix(r)) return ERR;
      return mkfix(r);
    }
    case PR_MOD: {
      if(!is_nil(d1) || !is_fix(a) || !is_fix(b)) return ERR;
      i32 bv=fixval(b); if(bv==0) return ERR;
      return mkfix(fixval(a)%bv);
    }
    case PR_ADD: case PR_SUB: case PR_MUL: {
      if(!is_fix(a) || !is_fix(b)) return ERR;
      i32 av=fixval(a), bv=fixval(b), r;
      if(id==PR_MUL){
        long long m=(long long)av*(long long)bv;
        if(!fits_fix64(m)) return ERR;
        r=(i32)m;
      } else {
        r=(id==PR_ADD)?(av+bv):(av-bv);
        if(!fits_fix(r)) return ERR;
      }
      if(is_nil(d1)) return mkfix(r);
      long long acc=r;
      for(u32 p=d1; is_cons(p); p=cdr(p)){
        u32 v=car(p); if(!is_fix(v)) return ERR;
        long long t = (id==PR_ADD) ? acc + (long long)fixval(v)
                    : (id==PR_SUB) ? acc - (long long)fixval(v)
                    :                acc * (long long)fixval(v);
        if(!fits_fix64(t)) return ERR;
        acc=t;
      }
      return mkfix((i32)acc);
    }
  }
  return ERR;
}

// ════════════════════════════════════════════════════════════════════════════
// WHAT THIS BOUGHT US (measured, not assumed — see FINDINGS.md):
//   • Deep NON-tail recursion: sum(10000) computes here; the tree-walker
//     overflows the C stack. This is CEK's one exclusive win.
//   • Proper tail calls: yes — but NOT a win over the tree-walker, because
//     clang -O2 already turns the tree-walker's `return eval(...)` self-calls
//     into a loop (tail-recursion elimination). countdown(1e6) works on BOTH.
//   • Speed: ~2.2x SLOWER than the tree-walker on fib (continuations are
//     5-6 cons cells each; heavy allocation, no GC).
//   • Long tail loops: exhausts the arena SOONER than the tree-walker, since
//     it allocates more garbage per step and nothing reclaims it.
// Net: a capability play for non-tail depth, not a speedup. Kept as a correct,
// working reference machine.
// ════════════════════════════════════════════════════════════════════════════
// CEK MACHINE
// Three registers C (control), E (environment), K (kontinuation). Two states,
// eval_expr and return_val, joined by GUARANTEED tail calls (musttail ->
// wasm return_call -> native jump). The C stack never grows; recursion depth
// lives in heap-allocated continuations instead. A normal `return` anywhere
// short-circuits straight to run(), because every frame is tail-replaced — so
// error propagation is free.
// ════════════════════════════════════════════════════════════════════════════

// Continuation kinds. A K is a cons list: (kind-fixnum . fields...). It lives in
// the same arena as everything else.
enum { K_HALT=0, K_IF, K_ARGS, K_DEFINE, K_SEQ, K_SETBANG };

static u32 g_khalt;                       // the singleton halt continuation
static u32 k_kind(u32 K){ return fixval(car(K)); }
static u32 kf(u32 K, int i){ u32 p=cdr(K); while(i--) p=cdr(p); return car(p); }

static u32 mklist4(u32 a,u32 b,u32 c,u32 d){ return cons(a,cons(b,cons(c,cons(d,NIL)))); }
static u32 mklist5(u32 a,u32 b,u32 c,u32 d,u32 e){ return cons(a,cons(b,cons(c,cons(d,cons(e,NIL))))); }

// K_IF:    (K_IF then else env next)
static u32 k_if(u32 thn,u32 els,u32 env,u32 next){
  return cons(mkfix(K_IF), mklist4(thn,els,env,next));
}
// K_ARGS:  (K_ARGS fn done todo env next)   fn=UNBOUND until operator evaluated
static u32 k_args(u32 fn,u32 done,u32 todo,u32 env,u32 next){
  return cons(mkfix(K_ARGS), mklist5(fn,done,todo,env,next));
}
// K_DEFINE:(K_DEFINE name next)
static u32 k_define(u32 name,u32 next){
  return cons(mkfix(K_DEFINE), cons(name,cons(next,NIL)));
}
// K_SEQ:   (K_SEQ rest env next)
static u32 k_seq(u32 rest,u32 env,u32 next){
  return cons(mkfix(K_SEQ), cons(rest,cons(env,cons(next,NIL))));
}
// K_SETBANG:(K_SETBANG sym env next) — when value V comes back, env_set(env,sym,V).
static u32 k_setbang(u32 sym,u32 env,u32 next){
  return cons(mkfix(K_SETBANG), cons(sym,cons(env,cons(next,NIL))));
}

static u32 reverse(u32 lst){
  u32 r=NIL; for(u32 p=lst; is_cons(p); p=cdr(p)) r=cons(car(p),r); return r;
}
static u32 bind_params(u32 fn, u32 args){
  u32 p=car(cdr(fn)), env=car(cdr(cdr(cdr(fn))));
  u32 a=args;
  for(; is_cons(p)&&is_cons(a); p=cdr(p),a=cdr(a))
    env=env_define(env,car(p),car(a));
  if(is_cons(p) || is_cons(a)) return ERR;            // arity mismatch
  return env;
}

// Both states share the (u32,u32,u32) signature — REQUIRED for musttail.
// return_val ignores its middle argument.
static u32 eval_expr(u32 C, u32 E, u32 K);
static u32 return_val(u32 V, u32 _unused, u32 K);

#define TAIL __attribute__((musttail)) return

static u32 eval_expr(u32 C, u32 E, u32 K){
  if(g_oom) return ERR;                             // arena exhausted -> clean error
  if(is_fix(C) || tagof(C)==TAG_SPEC)               // self-evaluating
    { TAIL return_val(C, 0, K); }
  if(is_sym(C)){                                    // variable reference
    u32 v = env_lookup(E, C);
    if(v==UNBOUND) return ERR;                      // short-circuits to run()
    TAIL return_val(v, 0, K);
  }
  u32 op = car(C);
  if(op==s_quote)  { TAIL return_val(car(cdr(C)), 0, K); }
  if(op==s_lambda) { TAIL return_val(make_closure(car(cdr(C)),car(cdr(cdr(C))),E), 0, K); }
  if(op==s_if){
    u32 k = k_if(car(cdr(cdr(C))), car(cdr(cdr(cdr(C)))), E, K);
    TAIL eval_expr(car(cdr(C)), E, k);              // evaluate the test
  }
  if(op==s_define){
    u32 head = car(cdr(C));
    if(is_cons(head)){
      // (define (f a b) body) -> (define f (lambda (a b) body))
      u32 name=car(head), params=cdr(head), body=car(cdr(cdr(C)));
      u32 lam=cons(s_lambda, cons(params, cons(body, NIL)));
      u32 d  =cons(s_define, cons(name,   cons(lam,  NIL)));
      TAIL eval_expr(d, E, K);
    }
    u32 k = k_define(head, K);
    TAIL eval_expr(car(cdr(cdr(C))), E, k);         // evaluate the value
  }
  if(op==s_setbang){
    // (set! sym expr) — strict arity 2. Queue K_SETBANG, evaluate value;
    // K_SETBANG's case in return_val performs the env_set + propagates value.
    u32 d1=cdr(C);
    if(!is_cons(d1)) return ERR;
    u32 d2=cdr(d1);
    if(!is_cons(d2) || !is_nil(cdr(d2))) return ERR;
    u32 sym=car(d1);
    if(!is_sym(sym)) return ERR;
    u32 k = k_setbang(sym, E, K);
    TAIL eval_expr(car(d2), E, k);
  }
  if(op==s_cond){
    // (cond (t1 e1) ... [(else en)]) -> (if t1 e1 (if t2 e2 ... en))
    u32 rev=NIL; for(u32 c=cdr(C); is_cons(c); c=cdr(c)) rev=cons(car(c),rev);
    u32 expanded=NIL;
    for(u32 c=rev; is_cons(c); c=cdr(c)){
      u32 cl=car(c); u32 test=car(cl), body=car(cdr(cl));
      if(test==s_else) expanded=body;
      else expanded=cons(s_if, cons(test, cons(body, cons(expanded, NIL))));
    }
    TAIL eval_expr(expanded, E, K);
  }
  if(op==s_begin){
    u32 body=cdr(C);
    if(!is_cons(body)) { TAIL return_val(NIL,0,K); }
    if(!is_cons(cdr(body))) { TAIL eval_expr(car(body), E, K); }  // last is tail
    TAIL eval_expr(car(body), E, k_seq(cdr(body), E, K));
  }
  if(op==s_let){
    // desugar (let ((a v)...) body) -> ((lambda (a...) body) v...), reuse apply
    u32 binds=car(cdr(C)), body=car(cdr(cdr(C)));
    u32 vars=NIL, vals=NIL;                         // built reversed — pairs stay aligned
    for(u32 b=binds; is_cons(b); b=cdr(b)){
      vars=cons(car(car(b)), vars);
      vals=cons(car(cdr(car(b))), vals);
    }
    u32 lam=cons(s_lambda, cons(vars, cons(body, NIL)));
    TAIL eval_expr(cons(lam, vals), E, K);
  }
  // application: evaluate operator first, remembering the unevaluated args
  TAIL eval_expr(op, E, k_args(UNBOUND, NIL, cdr(C), E, K));
}

static u32 return_val(u32 V, u32 _unused, u32 K){
  (void)_unused;
  if(g_oom) return ERR;                             // arena exhausted -> clean error
  if(V==ERR) return ERR;                            // propagate errors to run()
  switch(k_kind(K)){
    case K_HALT:
      return V;                                     // the ONE non-tail return

    case K_IF: {
      u32 branch = is_nil(V) ? kf(K,1) : kf(K,0);   // else : then
      TAIL eval_expr(branch, kf(K,2), kf(K,3));     // env, next  (tail position)
    }

    case K_DEFINE: {
      global_define(kf(K,0), V);                    // bind name -> value
      TAIL return_val(kf(K,0), 0, kf(K,1));         // define returns the name
    }

    case K_SETBANG: {
      u32 sym=kf(K,0), E=kf(K,1), next=kf(K,2);
      if(env_set(E, sym, V)==UNBOUND) return ERR;   // set! on unbound
      TAIL return_val(V, 0, next);                  // set! returns the value
    }

    case K_SEQ: {
      u32 rest=kf(K,0), E=kf(K,1), next=kf(K,2);
      if(is_cons(cdr(rest)))                        // more after this one
        { TAIL eval_expr(car(rest), E, k_seq(cdr(rest), E, next)); }
      TAIL eval_expr(car(rest), E, next);           // last expr: tail position
    }

    case K_ARGS: {
      u32 fn=kf(K,0), done=kf(K,1), todo=kf(K,2), E=kf(K,3), next=kf(K,4);
      if(fn==UNBOUND) fn=V;                          // first value = the operator
      else            done=cons(V, done);            // else accumulate an argument
      if(is_cons(todo))                              // still operands to evaluate?
        { TAIL eval_expr(car(todo), E, k_args(fn, done, cdr(todo), E, next)); }
      u32 args=reverse(done);
      // EXP2: (call/cc f) — apply f to a continuation value wrapping `next`.
      // Reduces to a normal apply after re-binding fn=f and args=(cont).
      if(fn==mkspec(PR_CALLCC)){
        if(!is_cons(args) || !is_nil(cdr(args))) return ERR;
        u32 f = car(args);
        u32 cont = make_cont(next);
        args = cons(cont, NIL);
        fn = f;
      }
      // EXP2: (k v) — invoking a reified continuation: restore captured K,
      // return v into it. The current `next` is discarded — that's the whole
      // point of first-class continuations.
      if(is_cont(fn)){
        if(!is_cons(args) || !is_nil(cdr(args))) return ERR;
        TAIL return_val(car(args), 0, cont_k(fn));
      }
      if(is_prim(fn)){
        u32 r=apply_prim(fn, args);
        if(r==ERR) return ERR;
        TAIL return_val(r, 0, next);
      }
      if(is_closure(fn)){
        u32 E2=bind_params(fn, args);
        if(E2==ERR) return ERR;                       // arity mismatch
        TAIL eval_expr(car(cdr(cdr(fn))), E2, next); // body, original next-K (tail!)
      }
      return ERR;                                    // applied a non-function
    }
  }
  return ERR;                                        // unknown continuation
}

// Driver: the tail-call chain runs to K_HALT; no trampoline.
extern u32 g_env;
static u32 run(u32 expr){ return eval_expr(expr, g_env, g_khalt); }

// ---- global env & init -----------------------------------------------------
u32 g_env;
static void bindp(const char* nm, u32 prim){
  u32 l=0; while(nm[l])l++;
  u32 s=intern(nm,l);
  global_define(s,prim);
}
static void init(){
  cell_top=0; sym_top=0; g_oom=0;
  s_quote =intern("quote",5);
  s_if    =intern("if",2);
  s_define=intern("define",6);
  s_lambda=intern("lambda",6);
  s_let   =intern("let",3);
  s_begin =intern("begin",5);
  s_cond  =intern("cond",4);
  s_else  =intern("else",4);
  s_setbang=intern("set!",4);
  s_closure=intern("%closure",8);
  s_cont   =intern("%cont",5);
  // sentinel head: car is an unbound marker that matches no real symbol.
  g_head = cons(UNBOUND, NIL);
  g_env  = g_head;
  g_khalt= cons(mkfix(K_HALT), NIL);
  u32 s_nil=intern("nil",3), s_t=intern("t",1);
  global_define(s_nil,NIL);
  global_define(s_t,TRUE);
  bindp("cons",mkspec(PR_CONS)); bindp("car",mkspec(PR_CAR));
  bindp("cdr",mkspec(PR_CDR));   bindp("+",mkspec(PR_ADD));
  bindp("-",mkspec(PR_SUB));     bindp("*",mkspec(PR_MUL));
  bindp("/",mkspec(PR_DIV));     bindp("mod",mkspec(PR_MOD));
  bindp("=",mkspec(PR_EQ));      bindp("<",mkspec(PR_LT));
  bindp("null?",mkspec(PR_NULLP));bindp("pair?",mkspec(PR_PAIRP));
  bindp("list?",mkspec(PR_LISTQ));
  bindp("number?",mkspec(PR_NUMBERP)); bindp("symbol?",mkspec(PR_SYMBOLP));
  bindp("set-car!",mkspec(PR_SETCAR)); bindp("set-cdr!",mkspec(PR_SETCDR));
  bindp("call/cc",mkspec(PR_CALLCC));
}

// ---- printer (writes into a fixed output buffer in linear memory) ----------
#define OUTCAP 4096
static char outbuf[OUTCAP];
static u32 outlen;
static void emit(char c){ if(outlen<OUTCAP) outbuf[outlen++]=c; }
static void emits(const char*s){ while(*s) emit(*s++); }
static void emitint(i32 n){
  if(n<0){ emit('-'); n=-n; }
  char tmp[12]; int k=0;
  if(n==0){ emit('0'); return; }
  while(n){ tmp[k++]='0'+(n%10); n/=10; }
  while(k) emit(tmp[--k]);
}
static void print_val(u32 v){
  if(is_fix(v)){ emitint(fixval(v)); return; }
  if(v==NIL){ emits("()"); return; }
  if(v==TRUE){ emits("t"); return; }
  if(v==ERR){ emits("<error>"); return; }
  if(is_prim(v)){ emits("<primitive>"); return; }
  if(is_sym(v)){ u32 i=symidx(v); for(u32 k=0;k<symlen[i];k++) emit(symname[i][k]); return; }
  if(is_closure(v)){ emits("<lambda>"); return; }
  if(is_cont(v)){ emits("<continuation>"); return; }
  if(is_cons(v)){
    emit('(');
    u32 p=v; int first=1;
    while(is_cons(p)){ if(!first)emit(' '); first=0; print_val(car(p)); p=cdr(p); }
    if(!is_nil(p)){ emits(" . "); print_val(p); }
    emit(')');
  }
}

// ---- exported entry points -------------------------------------------------
// JS writes source into `inbuf`, calls eval_source(len). We eval every top-level
// form, printing the result of the LAST one into outbuf. Returns outbuf length.
#define INCAP 8192
char inbuf[INCAP];

__attribute__((export_name("input_ptr"))) char* input_ptr(){ return inbuf; }
__attribute__((export_name("output_ptr"))) char* output_ptr(){ return outbuf; }
__attribute__((export_name("eval_source")))
u32 eval_source(u32 len){
  init();
  rp=inbuf; rend=inbuf+(len<INCAP?len:INCAP);
  outlen=0;
  u32 result=NIL;
  for(;;){
    skipws();
    if(rp>=rend) break;
    u32 form=read_expr();
    if(form==ERR){ result=ERR; break; }
    result=run(form);
    if(result==ERR) break;
  }
  print_val(result);
  return outlen;
}
