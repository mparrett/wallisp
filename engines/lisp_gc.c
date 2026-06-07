// lisp_gc.c — tree-walker + mark-sweep garbage collector.
//
// Same evaluator as lisp.c. Adds mark-sweep GC and the bookkeeping that comes
// with it. This is the harder GC port — bytecode_gc and cek_gc had structured
// register state we could hoist; the tree-walker is a recursive `eval()` whose
// live cons references live in C locals across recursive calls. A naive port
// collects them. The fix here is an explicit shadow stack (R_save) that every
// allocating recursion pushes/pops.
//
// What's added on top of lisp.c:
//   * Mark-sweep collector lifted from bytecode_gc.c (non-moving, free-list
//     sweep, mark bits + explicit mark stack).
//   * R_save shadow stack with a strict push/pop protocol in eval() and
//     eval_list(). At each entry, (x, env) get pushed. Intermediate locals
//     that span allocating recursive calls (fn, args, newenv, v) get extra
//     slots when alive. Every return path restores R_ssp before returning.
//
//   * Tail returns (`return eval(next, env)`) keep clang's TRE working — the
//     property that makes the tree-walker unbounded (H1 for the tree-walker)
//     — by extracting `next` while the outer frame is still rooted, popping
//     the outer frame, then returning. The pop and the inner eval's prologue
//     push cancel net-zero, the C frame is recycled by TRE, and the shadow
//     stack stays at the caller's baseline. Deep tail recursion still flat.
//
// What is NOT done:
//   * Mid-eval roots for `x`'s sub-expressions read multiple times. `x` is
//     rooted; reading `car(cdr(x))` is safe; the value returned is in a C
//     local that survives only until the next allocating call. That's enough
//     because each branch either uses the value immediately (no alloc in
//     between) or pushes it.
//
// Build: -fno-builtin (memset stub keeps modules zero-imports). No tail-call
//        extension needed — the tree-walker uses ordinary C recursion + TRE.

typedef unsigned int   u32;
typedef int            i32;
typedef unsigned char  u8;

void* memset(void* p, int c, unsigned long n){ u8* s=(u8*)p; while(n--) *s++=(u8)c; return p; }
void* memcpy(void* d, const void* s, unsigned long n){ u8* a=(u8*)d; const u8* b=(const u8*)s; while(n--) *a++=*b++; return d; }

// ---- tag scheme (identical to lisp.c) --------------------------------------
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

enum {
  SP_NIL=0, SP_T, SP_ERR, SP_UNBOUND,
  PR_CONS, PR_CAR, PR_CDR, PR_ADD, PR_SUB, PR_MUL, PR_DIV, PR_MOD, PR_EQ, PR_LT,
  PR_NULLP, PR_PAIRP, PR_LISTQ, PR_NUMBERP, PR_SYMBOLP, PR_SETCAR, PR_SETCDR,
  SP_COUNT
};
#define NIL      mkspec(SP_NIL)
#define TRUE     mkspec(SP_T)
#define ERR      mkspec(SP_ERR)
#define UNBOUND  mkspec(SP_UNBOUND)
#define is_nil(v) ((v)==NIL)
#define is_prim(v) (tagof(v)==TAG_SPEC && ((v)>>2) >= PR_CONS && ((v)>>2) < SP_COUNT)

// ---- arena (matches bytecode_gc/cek_gc default for comparable timings) -----
#define MAX_CELLS 262144
typedef struct { u32 car, cdr; } Cell;
static Cell cells[MAX_CELLS];
static u32  cell_top = 0;
static int  g_oom = 0;

// ---- mark-sweep GC state ---------------------------------------------------
#define FREE_END 0xFFFFFFFFu
static u8  markbit[MAX_CELLS];
static u32 markstk[MAX_CELLS];
static u32 msp;
static u32 freelist = FREE_END;
static u32 gc_a, gc_b;
static int gc_enabled = 0;
static u32 g_numgc = 0;

// Shadow stack — the tree-walker's only GC handle.
// Sized for ack(3,4) worst case (~500 deep × ~5 slots per frame) and ap/nrev
// (~300 deep × ~5 slots). 16384 is comfortable and only 64KB.
#define SAVE_MAX 16384
static u32 R_save[SAVE_MAX];
static u32 R_ssp = 0;

#define MAX_SYMS    1024
#define SYM_CHARS   16
static char symname[MAX_SYMS][SYM_CHARS];
static u32  symlen[MAX_SYMS];
static u32  sym_top = 0;

static void gc(void);

static u32 cons_slow(u32 a, u32 d){
  u32 i;
  if(freelist!=FREE_END){ i=freelist; freelist=cells[i].cdr; }
  else {
    if(gc_enabled){ gc_a=a; gc_b=d; gc(); }
    if(freelist!=FREE_END){ i=freelist; freelist=cells[i].cdr; }
    else { g_oom=1; return ERR; }
  }
  cells[i].car=a; cells[i].cdr=d; return mkcons(i);
}
static inline u32 cons(u32 a, u32 d){
  if(cell_top<MAX_CELLS){ u32 i=cell_top++; cells[i].car=a; cells[i].cdr=d; return mkcons(i); }
  return cons_slow(a,d);
}
static u32 car(u32 v){ return is_cons(v) ? cells[considx(v)].car : NIL; }
static u32 cdr(u32 v){ return is_cons(v) ? cells[considx(v)].cdr : NIL; }

// ---- symbol interner (identical to lisp.c) ---------------------------------
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

static u32 s_quote,s_if,s_define,s_lambda,s_let,s_begin,s_cond,s_else,s_setbang;

// ---- reader (shared) -------------------------------------------------------
#include "reader.h"

// ---- environment -----------------------------------------------------------
static u32 env_lookup(u32 env, u32 sym){
  for(u32 f=env; is_cons(f); f=cdr(f)){
    u32 binding=car(f);
    if(is_cons(binding) && car(binding)==sym) return cdr(binding);
  }
  return UNBOUND;
}
// env_define is GC-safe by chained-cons discipline: inner cons protects
// (sym, val), outer cons protects (inner_result, env) via gc_a/gc_b. Callers
// only need to ensure `env` is rooted before calling.
static u32 env_define(u32 env, u32 sym, u32 val){
  return cons(cons(sym,val), env);
}
// PR2: walk env, mutate the binding cons's cdr in place. See engines/lisp.c
// for design notes. No allocation, so GC-safe with no shadow-stack discipline.
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
static u32 g_head;
static u32 g_env;
static void global_define(u32 sym, u32 val){
  u32 link=cons(cons(sym,val), cdr(g_head));
  cells[considx(g_head)].cdr = link;
}

// ---- closures (identical) --------------------------------------------------
static u32 s_closure;
static u32 make_closure(u32 params, u32 body, u32 env){
  return cons(s_closure, cons(params, cons(body, cons(env, NIL))));
}
static int is_closure(u32 v){ return is_cons(v) && car(v)==s_closure; }

// ---- primitives (identical) ------------------------------------------------
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
// EVAL — recursive tree-walker, GC-safe via R_save shadow stack.
//
// Discipline at every entry:
//     base = R_ssp;
//     R_save[R_ssp++] = x;        // [base+0]
//     R_save[R_ssp++] = env;      // [base+1]
//   …and at every return path: R_ssp = base; before returning.
//
// Tail-return pattern that preserves clang's TRE (the H1 property):
//     u32 next = car(cdr(cdr(x)));    // read while x rooted
//     R_ssp = base;                   // release outer frame
//     return eval(next, env);         // TRE candidate — inner pushes its own frame
//
// The C local `env` survives the pop because no allocation happens between
// `R_ssp = base` and the recursive eval call's prologue. clang's TRE then
// recycles the C stack frame and the shadow stack stays at the caller's
// baseline — deep tail recursion runs flat in both stacks.
// ════════════════════════════════════════════════════════════════════════════

static u32 eval(u32 x, u32 env);

// eval_list — recursive non-tail. Shadow stack:
//   [base+0]=lst, [base+1]=env, [base+2]=v (across the recursive eval_list).
static u32 eval_list(u32 lst, u32 env){
  u32 base = R_ssp;
  R_save[R_ssp++] = lst;
  R_save[R_ssp++] = env;
  if(!is_cons(lst)){ R_ssp = base; return NIL; }
  u32 v = eval(car(lst), env);
  if(v==ERR){ R_ssp = base; return ERR; }
  R_save[R_ssp++] = v;
  u32 rest = eval_list(cdr(lst), env);
  if(rest==ERR){ R_ssp = base; return ERR; }
  u32 result = cons(v, rest);                  // cons protects v, rest via gc_a/gc_b
  R_ssp = base;
  return result;
}

static u32 eval(u32 x, u32 env){
  if(g_oom) return ERR;
  u32 base = R_ssp;
  R_save[R_ssp++] = x;                          // [base+0]
  R_save[R_ssp++] = env;                        // [base+1]

  // self-evaluating
  if(is_fix(x) || tagof(x)==TAG_SPEC){ R_ssp = base; return x; }
  if(is_sym(x)){
    u32 v = env_lookup(env, x);
    R_ssp = base;
    return (v==UNBOUND) ? ERR : v;
  }

  u32 op = car(x);
  if(op==s_quote){ u32 r = car(cdr(x)); R_ssp = base; return r; }

  if(op==s_if){
    u32 t = eval(car(cdr(x)), env);
    if(t==ERR){ R_ssp = base; return ERR; }
    // Tail call: extract `next` while x is still rooted, then pop, then TRE.
    u32 next = is_nil(t) ? car(cdr(cdr(cdr(x)))) : car(cdr(cdr(x)));
    R_ssp = base;
    return eval(next, env);                     // TRE-preserving
  }

  if(op==s_define){
    u32 head = car(cdr(x));
    if(is_cons(head)){
      // (define (f a b) body) -> name=f, val=(lambda (a b) body)
      // params/body reachable via x (rooted at base+0) across make_closure.
      u32 name = car(head), params = cdr(head), body = car(cdr(cdr(x)));
      u32 clo  = make_closure(params, body, env);
      global_define(name, clo);
      R_ssp = base;
      return name;
    }
    u32 val  = eval(car(cdr(cdr(x))), env);
    if(val==ERR){ R_ssp = base; return ERR; }
    // global_define's internal conses chain protects via gc_a/gc_b; val is the
    // gc_b of the inner cons(name, val), so it's covered.
    global_define(head, val);
    R_ssp = base;
    return head;
  }
  if(op==s_setbang){
    // (set! sym expr) — strict arity 2. env_set does no allocation, so val
    // on the C stack is safe across the call; no new R_save discipline needed.
    u32 d1 = cdr(x);
    if(!is_cons(d1)){ R_ssp = base; return ERR; }
    u32 d2 = cdr(d1);
    if(!is_cons(d2) || !is_nil(cdr(d2))){ R_ssp = base; return ERR; }
    u32 sym = car(d1);
    if(!is_sym(sym)){ R_ssp = base; return ERR; }
    u32 val = eval(car(d2), env);
    if(val==ERR){ R_ssp = base; return ERR; }
    u32 r = env_set(env, sym, val);
    R_ssp = base;
    if(r==UNBOUND) return ERR;
    return val;
  }
  if(op==s_cond){
    // (cond (t1 e1) ... [(else en)]): walk clauses, eval test non-tail, body tail.
    // x rooted at base+0 keeps the clause chain reachable across each test eval.
    for(u32 c = cdr(x); is_cons(c); c = cdr(c)){
      u32 cl = car(c); u32 test = car(cl);
      if(test==s_else){
        u32 body = car(cdr(cl));
        R_ssp = base;
        return eval(body, env);                 // TRE-preserving
      }
      u32 t = eval(test, env);
      if(t==ERR){ R_ssp = base; return ERR; }
      if(!is_nil(t)){
        u32 body = car(cdr(cl));
        R_ssp = base;
        return eval(body, env);                 // TRE-preserving
      }
    }
    R_ssp = base;
    return NIL;                                 // no clause matched
  }

  if(op==s_lambda){
    u32 params = car(cdr(x));
    u32 body   = car(cdr(cdr(x)));
    u32 clo    = make_closure(params, body, env);   // chained-cons safe
    R_ssp = base;
    return clo;
  }

  if(op==s_let){
    // (let ((a v) (b w)) body): extend env left-to-right, then eval body in tail.
    // newenv accumulates and needs to span the per-binding eval() calls; pin it.
    u32 binds  = car(cdr(x));
    u32 body   = car(cdr(cdr(x)));
    u32 ne_slot = R_ssp;
    R_save[R_ssp++] = env;                      // [base+2] = newenv accumulator
    for(u32 b = binds; is_cons(b); b = cdr(b)){
      u32 pair = car(b);                        // pair, val_expr reachable via x
      u32 val_expr = car(cdr(pair));
      u32 v = eval(val_expr, env);              // recurse; R_save[ne_slot] keeps newenv alive
      if(v==ERR){ R_ssp = base; return ERR; }
      // v is fresh; env_define's inner cons protects v via gc_b before any
      // further allocation. R_save[ne_slot] holds the current newenv.
      R_save[ne_slot] = env_define(R_save[ne_slot], car(pair), v);
    }
    u32 nenv = R_save[ne_slot];
    R_ssp = base;
    return eval(body, nenv);                    // TRE-preserving
  }

  if(op==s_begin){
    // Sequence: evaluate forms left-to-right. Last form in tail position.
    u32 r = NIL;
    u32 b = cdr(x);
    if(!is_cons(b)){ R_ssp = base; return NIL; }
    // Walk until the last form; evaluate non-tail forms and discard result.
    while(is_cons(cdr(b))){
      r = eval(car(b), env);
      if(r==ERR){ R_ssp = base; return ERR; }
      b = cdr(b);                               // b's cells reachable via x
    }
    u32 last = car(b);
    R_ssp = base;
    return eval(last, env);                     // TRE-preserving
  }

  // application: evaluate operator, then args, then dispatch.
  // Slot layout during the application path:
  //   [base+0] x   [base+1] env   [base+2] fn   [base+3] args
  //   [base+4] newenv (closure-bind accumulator, only in closure branch)
  u32 fn = eval(op, env);
  if(fn==ERR){ R_ssp = base; return ERR; }
  R_save[R_ssp++] = fn;                         // [base+2]
  u32 args = eval_list(cdr(x), env);
  if(args==ERR){ R_ssp = base; return ERR; }
  R_save[R_ssp++] = args;                       // [base+3]

  if(is_prim(fn)){
    u32 r = apply_prim(fn, args);               // PR_CONS allocates; args/fn rooted via R_save
    R_ssp = base;
    return r;
  }
  if(is_closure(fn)){
    u32 ne_slot = R_ssp;
    R_save[R_ssp++] = car(cdr(cdr(cdr(fn))));   // [base+4] = closure's captured env
    u32 params = car(cdr(fn));                  // params/body reachable via fn at [base+2]
    u32 p = params, a = args;
    while(is_cons(p) && is_cons(a)){
      R_save[ne_slot] = env_define(R_save[ne_slot], car(p), car(a));
      p = cdr(p); a = cdr(a);
    }
    if(is_cons(p) || is_cons(a)){ R_ssp = base; return ERR; }   // arity mismatch
    u32 body = car(cdr(cdr(fn)));
    u32 nenv = R_save[ne_slot];
    R_ssp = base;
    return eval(body, nenv);                    // TRE-preserving
  }
  R_ssp = base;
  return ERR;                                   // applied a non-function
}

// ---- garbage collector -----------------------------------------------------
static void mark(u32 v){
  if(!is_cons(v)) return;
  u32 i=considx(v);
  if(markbit[i]) return;
  markbit[i]=1;
  markstk[msp++]=i;
}
static void gc(void){
  g_numgc++;
  for(u32 i=0;i<MAX_CELLS;i++) markbit[i]=0;
  msp=0;
  // roots
  mark(gc_a); mark(gc_b);                       // in-flight cons args
  mark(g_env);                                  // = g_head; covers all globals
  for(u32 i=0; i<R_ssp; i++) mark(R_save[i]);   // every active eval/eval_list frame
  // trace
  while(msp>0){ u32 i=markstk[--msp]; mark(cells[i].car); mark(cells[i].cdr); }
  // sweep
  freelist=FREE_END;
  for(u32 i=0;i<MAX_CELLS;i++)
    if(!markbit[i]){ cells[i].cdr=freelist; freelist=i; }
}

// ---- init ------------------------------------------------------------------
static void bindp(const char* nm, u32 prim){
  u32 l=0; while(nm[l])l++;
  u32 s=intern(nm,l);
  global_define(s,prim);
}
static void init(){
  cell_top=0; sym_top=0; g_oom=0; g_numgc=0; gc_enabled=0;
  freelist=FREE_END; R_ssp=0; gc_a=gc_b=0;
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
  g_head = cons(UNBOUND, NIL);
  g_env  = g_head;
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
}

// ---- printer (identical) ---------------------------------------------------
#define OUTCAP 4096
static char outbuf[OUTCAP];
static u32 outlen;
static void emit_(char c){ if(outlen<OUTCAP) outbuf[outlen++]=c; }
static void emits(const char*s){ while(*s) emit_(*s++); }
static void emitint(i32 n){
  if(n<0){ emit_('-'); n=-n; }
  char tmp[12]; int k=0;
  if(n==0){ emit_('0'); return; }
  while(n){ tmp[k++]='0'+(n%10); n/=10; }
  while(k) emit_(tmp[--k]);
}
static void print_val(u32 v){
  if(is_fix(v)){ emitint(fixval(v)); return; }
  if(v==NIL){ emits("()"); return; }
  if(v==TRUE){ emits("t"); return; }
  if(v==ERR||v==UNBOUND){ emits("<error>"); return; }
  if(is_prim(v)){ emits("<primitive>"); return; }
  if(is_sym(v)){ u32 i=symidx(v); for(u32 k=0;k<symlen[i];k++) emit_(symname[i][k]); return; }
  if(is_closure(v)){ emits("<lambda>"); return; }
  if(is_cons(v)){
    emit_('(');
    u32 p=v; int first=1;
    while(is_cons(p)){ if(!first)emit_(' '); first=0; print_val(car(p)); p=cdr(p); }
    if(!is_nil(p)){ emits(" . "); print_val(p); }
    emit_(')');
  }
}

// ---- exported entry points -------------------------------------------------
#define INCAP 8192
char inbuf[INCAP];

__attribute__((export_name("input_ptr")))  char* input_ptr(){ return inbuf; }
__attribute__((export_name("output_ptr"))) char* output_ptr(){ return outbuf; }
__attribute__((export_name("gc_count")))   unsigned gc_count(){ return g_numgc; }
__attribute__((export_name("eval_source")))
u32 eval_source(u32 len){
  init();
  rp=inbuf; rend=inbuf+(len<INCAP?len:INCAP);
  outlen=0;
  u32 result=NIL;
  for(;;){
    skipws();
    if(rp>=rend) break;
    // Reader has unrooted-local windows (first/last in read_list). Keep GC off
    // during reading; the bump arena handles small ASTs trivially.
    gc_enabled = 0;
    u32 form = read_expr();
    if(form==ERR){ result=ERR; break; }
    gc_enabled = 1;
    result = eval(form, g_env);
    if(result==ERR) break;
  }
  print_val(result);
  return outlen;
}
