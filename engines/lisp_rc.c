// lisp_rc.c — tree-walker + reference-counting GC (H12).
//
// Fourth point on the GC-strategy axis: no-GC / region-drop / mark-sweep /
// **refcount**. Same recursive tree-walker semantics as lisp.c / lisp_gc.c;
// the collector is the variable. Inspired by Justine Tunney's SectorLambda,
// whose Church-Krivine-Tromp VM refcounts closures in ~7 lines.
//
// Two design points that differ from lisp_gc.c:
//
//   * No shadow stack, no root scan. A value stays live because some cell or
//     C local holds an owned reference (refs > 0). eval() therefore follows a
//     strict ownership protocol instead of the R_save push/pop discipline.
//
//   * eval() is an explicit trampoline (`for(;;)`), NOT C-recursion + TRE.
//     Refcounting must release the environment frame *after* a tail call's
//     body finishes — impossible under C TRE (no hook after the call). The
//     loop reassigns x/env at tail positions and releases the old owned frame
//     on each hop, so tailsum(30000) stays flat AND frames are reclaimed.
//     Non-tail sub-evals (args, if-test, non-tail fib) are ordinary C
//     recursion, bounded by expression depth exactly as in lisp.c.
//
// Ownership contract:
//   * eval(x, env) RETURNS an owned reference; it BORROWS x and env.
//   * apply_prim / cons / make_closure / env_define return owned references.
//   * car()/cdr()/env_lookup() return BORROWED references (retain to keep).
//   * Only cons cells are counted. Fixnums, symbols (interned), and the
//     tagged singletons are never counted.
//
// Cycles: pure refcounting leaks cycles, which a mutable Lisp can build via
// set-car!/set-cdr!. The whole bench suite is cycle-free, so the numbers are
// valid; leaked cycles in mutation tests are bounded and don't affect output.
// See the H12 pre-registration note.
//
// AST + reader cells are PINNED (allocated with g_reading=1): never counted,
// never freed. They live for one eval_source() call, which re-inits the arena.
// This keeps the shared reader.h (which splices cell cdrs directly) correct
// without per-engine changes, and closures that capture a pinned AST body stay
// valid because release() skips pinned cells.
//
// Build: -fno-builtin (memset stub keeps the module zero-imports). No tail-call
//        extension — the trampoline is an ordinary loop.

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

// ---- arena (matches the other engines for comparable timings) --------------
#define MAX_CELLS 262144
typedef struct { u32 car, cdr; } Cell;
static Cell cells[MAX_CELLS];
static u32  cell_top = 0;
static int  g_oom = 0;

// ---- refcount state --------------------------------------------------------
#define FREE_END 0xFFFFFFFFu
static u32 refs[MAX_CELLS];      // per-cell reference count (unpinned only)
static u8  pinned[MAX_CELLS];    // 1 = AST/reader cell: never counted, never freed
static u32 freelist = FREE_END;  // reclaimed unpinned cells
static int g_reading = 0;        // reader allocates pinned cells
static u32 g_reclaimed = 0;      // cells returned to the free list (diagnostic)

#define MAX_SYMS    1024
#define SYM_CHARS   16
static char symname[MAX_SYMS][SYM_CHARS];
static u32  symlen[MAX_SYMS];
static u32  sym_top = 0;

// ---- refcount core ---------------------------------------------------------
static inline void retain(u32 v){
  if(!is_cons(v)) return;
  u32 i=considx(v);
  if(pinned[i]) return;
  refs[i]++;
}
// release: iterative down the cdr spine, recursive on the car. The cdr spine
// is the deep direction (lists, env chains), so keeping it iterative bounds C
// stack to the car-nesting depth (≈ expression depth), never the list length.
static void release(u32 v){
  while(is_cons(v)){
    u32 i=considx(v);
    if(pinned[i]) return;
    if(--refs[i] != 0) return;
    u32 a=cells[i].car, d=cells[i].cdr;
    cells[i].cdr=freelist; freelist=i;      // reclaim
    g_reclaimed++;
    release(a);                             // recurse on car
    v=d;                                    // loop on cdr
  }
}

static u32 cons(u32 a, u32 d){
  u32 i;
  if(cell_top<MAX_CELLS){ i=cell_top++; }
  else if(freelist!=FREE_END){ i=freelist; freelist=cells[i].cdr; }
  else { g_oom=1; return ERR; }
  cells[i].car=a; cells[i].cdr=d;
  if(g_reading){ pinned[i]=1; refs[i]=0; }
  else { pinned[i]=0; refs[i]=1; retain(a); retain(d); }
  return mkcons(i);
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
static u32 env_lookup(u32 env, u32 sym){        // returns BORROWED
  for(u32 f=env; is_cons(f); f=cdr(f)){
    u32 binding=car(f);
    if(is_cons(binding) && car(binding)==sym) return cdr(binding);
  }
  return UNBOUND;
}
// (cons (cons sym val) env) — returns OWNED. Each runtime cons retains its
// args, so the result holds sym/val/env; we drop only our transient `inner`.
static u32 env_define(u32 env, u32 sym, u32 val){
  u32 inner = cons(sym, val);
  if(inner==ERR) return ERR;
  u32 outer = cons(inner, env);
  release(inner);
  return outer;
}
// Mutate a binding's cdr in place. Refcount: retain new, release old.
static u32 env_set(u32 env, u32 sym, u32 val){
  for(u32 f=env; is_cons(f); f=cdr(f)){
    u32 binding=car(f);
    if(is_cons(binding) && car(binding)==sym){
      u32 old = cells[considx(binding)].cdr;
      retain(val);
      cells[considx(binding)].cdr = val;
      release(old);
      return val;
    }
  }
  return UNBOUND;
}
static u32 g_head;
static u32 g_env;
// Globals persist for the whole session; their chain is intentionally never
// released (the overwritten g_head.cdr ref is a deliberate, bounded leak).
static void global_define(u32 sym, u32 val){
  u32 inner = cons(sym, val);
  if(inner==ERR) return;
  u32 link = cons(inner, cdr(g_head));
  release(inner);
  cells[considx(g_head)].cdr = link;
}

// ---- closures --------------------------------------------------------------
static u32 s_closure;
static u32 make_closure(u32 params, u32 body, u32 env){   // returns OWNED
  u32 c4 = cons(env, NIL);
  if(c4==ERR) return ERR;
  u32 c3 = cons(body, c4);   release(c4);
  u32 c2 = cons(params, c3); release(c3);
  u32 c1 = cons(s_closure, c2); release(c2);
  return c1;
}
static int is_closure(u32 v){ return is_cons(v) && car(v)==s_closure; }

// ---- primitives ------------------------------------------------------------
// PR1 semantics identical to lisp.c. apply_prim returns an OWNED reference:
// component-returning prims (car/cdr, set-car!/set-cdr!) retain before return
// so the result survives the caller's release(args); PR_CONS is already owned.
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
    case PR_CAR:   if(!is_nil(d0) || !is_cons(a)) return ERR;
                   { u32 r=cells[considx(a)].car; retain(r); return r; }
    case PR_CDR:   if(!is_nil(d0) || !is_cons(a)) return ERR;
                   { u32 r=cells[considx(a)].cdr; retain(r); return r; }
  }

  if(!is_cons(d0)) return ERR;
  u32 b  = car(d0);
  u32 d1 = cdr(d0);

  switch(id){
    case PR_CONS:  if(!is_nil(d1)) return ERR; return cons(a,b);   // owned
    case PR_SETCAR: if(!is_nil(d1) || !is_cons(a)) return ERR;
                    { u32 old=cells[considx(a)].car; retain(b);
                      cells[considx(a)].car=b; release(old);
                      retain(b); return b; }                       // owned copy for return
    case PR_SETCDR: if(!is_nil(d1) || !is_cons(a)) return ERR;
                    { u32 old=cells[considx(a)].cdr; retain(b);
                      cells[considx(a)].cdr=b; release(old);
                      retain(b); return b; }
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

// ---- eval_list — returns an OWNED list -------------------------------------
static u32 eval(u32 x, u32 env);
static u32 eval_list(u32 lst, u32 env){
  if(!is_cons(lst)) return NIL;
  u32 v = eval(car(lst), env);
  if(v==ERR) return ERR;
  u32 rest = eval_list(cdr(lst), env);
  if(rest==ERR){ release(v); return ERR; }
  u32 result = cons(v, rest);          // retains v, rest
  release(v); release(rest);
  return result;
}

// ════════════════════════════════════════════════════════════════════════════
// EVAL — explicit trampoline. `env_owned` tracks whether the current frame is
// ours to release. Tail positions reassign x (and env, releasing the old owned
// frame) then `continue`; every value exit routes through `done:` which
// releases the current frame if owned. Returns an OWNED reference.
// ════════════════════════════════════════════════════════════════════════════
static u32 eval(u32 x, u32 env){
  int env_owned = 0;
  u32 result;
  for(;;){
    if(g_oom){ result=ERR; goto done; }

    if(is_fix(x) || tagof(x)==TAG_SPEC){ result=x; goto done; }   // self-eval, uncounted
    if(is_sym(x)){
      u32 v = env_lookup(env, x);
      if(v==UNBOUND){ result=ERR; goto done; }
      retain(v); result=v; goto done;                             // borrowed -> owned
    }

    u32 op = car(x);
    if(op==s_quote){ u32 r=car(cdr(x)); retain(r); result=r; goto done; }

    if(op==s_if){
      u32 t = eval(car(cdr(x)), env);
      if(t==ERR){ result=ERR; goto done; }
      u32 next = is_nil(t) ? car(cdr(cdr(cdr(x)))) : car(cdr(cdr(x)));
      release(t);
      x = next;                                                   // tail, same env
      continue;
    }

    if(op==s_define){
      u32 head = car(cdr(x));
      if(is_cons(head)){
        u32 name=car(head), params=cdr(head), body=car(cdr(cdr(x)));
        u32 clo = make_closure(params, body, env);
        if(clo==ERR){ result=ERR; goto done; }
        global_define(name, clo);
        release(clo);
        result=name; goto done;
      }
      u32 val = eval(car(cdr(cdr(x))), env);
      if(val==ERR){ result=ERR; goto done; }
      global_define(head, val);
      release(val);
      result=head; goto done;
    }

    if(op==s_setbang){
      u32 d1=cdr(x); if(!is_cons(d1)){ result=ERR; goto done; }
      u32 d2=cdr(d1); if(!is_cons(d2) || !is_nil(cdr(d2))){ result=ERR; goto done; }
      u32 sym=car(d1); if(!is_sym(sym)){ result=ERR; goto done; }
      u32 val = eval(car(d2), env);
      if(val==ERR){ result=ERR; goto done; }
      u32 r = env_set(env, sym, val);          // retains val for the binding
      if(r==UNBOUND){ release(val); result=ERR; goto done; }
      result=val; goto done;                   // our ownership -> caller; binding keeps its own
    }

    if(op==s_cond){
      int matched=0;
      for(u32 c=cdr(x); is_cons(c); c=cdr(c)){
        u32 cl=car(c); u32 test=car(cl);
        if(test==s_else){ x=car(cdr(cl)); matched=1; break; }
        u32 t=eval(test, env);
        if(t==ERR){ result=ERR; goto done; }
        if(!is_nil(t)){ release(t); x=car(cdr(cl)); matched=1; break; }
        release(t);
      }
      if(matched) continue;                    // tail, same env
      result=NIL; goto done;
    }

    if(op==s_lambda){
      u32 params=car(cdr(x)), body=car(cdr(cdr(x)));
      result = make_closure(params, body, env);
      goto done;
    }

    if(op==s_let){
      u32 binds=car(cdr(x)), body=car(cdr(cdr(x)));
      u32 nenv=env; int nenv_owned=0;
      for(u32 b=binds; is_cons(b); b=cdr(b)){
        u32 pair=car(b); u32 val_expr=car(cdr(pair));
        u32 v=eval(val_expr, env);             // let (not let*): outer env
        if(v==ERR){ if(nenv_owned) release(nenv); result=ERR; goto done; }
        u32 nw=env_define(nenv, car(pair), v);
        release(v);
        if(nenv_owned) release(nenv);
        nenv=nw; nenv_owned=1;
      }
      if(!nenv_owned){ x=body; continue; }     // no bindings: body in same env
      if(env_owned) release(env);
      x=body; env=nenv; env_owned=1;           // tail, new frame
      continue;
    }

    if(op==s_begin){
      u32 b=cdr(x);
      if(!is_cons(b)){ result=NIL; goto done; }
      while(is_cons(cdr(b))){
        u32 r=eval(car(b), env);
        if(r==ERR){ result=ERR; goto done; }
        release(r);
        b=cdr(b);
      }
      x=car(b);                                // tail, same env
      continue;
    }

    // application: eval operator, then args, then dispatch.
    u32 fn = eval(op, env);
    if(fn==ERR){ result=ERR; goto done; }
    u32 args = eval_list(cdr(x), env);
    if(args==ERR){ release(fn); result=ERR; goto done; }

    if(is_prim(fn)){
      result = apply_prim(fn, args);           // owned
      release(fn); release(args);
      goto done;
    }
    if(is_closure(fn)){
      u32 cenv = car(cdr(cdr(cdr(fn))));        // borrowed (inside fn)
      u32 nenv = cenv; int nenv_owned=0;
      u32 p = car(cdr(fn)), a = args;
      while(is_cons(p) && is_cons(a)){
        u32 nw = env_define(nenv, car(p), car(a));
        if(nenv_owned) release(nenv);
        nenv=nw; nenv_owned=1;
        p=cdr(p); a=cdr(a);
      }
      if(is_cons(p) || is_cons(a)){             // arity mismatch
        if(nenv_owned) release(nenv);
        release(fn); release(args);
        result=ERR; goto done;
      }
      u32 body = car(cdr(cdr(fn)));             // pinned AST; safe across release(fn)
      if(!nenv_owned){ retain(nenv); nenv_owned=1; }  // 0-param: pin cenv before releasing fn
      release(fn); release(args);
      if(env_owned) release(env);
      x=body; env=nenv; env_owned=1;            // tail into the new frame
      continue;
    }
    release(fn); release(args);
    result=ERR; goto done;                      // applied a non-function
  }
done:
  if(env_owned) release(env);
  return result;
}

// ---- init ------------------------------------------------------------------
static void bindp(const char* nm, u32 prim){
  u32 l=0; while(nm[l])l++;
  u32 s=intern(nm,l);
  global_define(s,prim);
}
static void init(){
  cell_top=0; sym_top=0; g_oom=0; g_reclaimed=0; g_reading=0;
  freelist=FREE_END;
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

// ---- printer (identical to lisp.c) -----------------------------------------
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
static int pr_depth=0;
#define PRDEPTH_MAX 200
static void print_val_body(u32 v);
static void print_val(u32 v){
  if(outlen>=OUTCAP) return;
  if(++pr_depth > PRDEPTH_MAX){ --pr_depth; emits("..."); return; }
  print_val_body(v);
  --pr_depth;
}
static void print_val_body(u32 v){
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
    while(is_cons(p) && outlen<OUTCAP){ if(!first)emit_(' '); first=0; print_val(car(p)); p=cdr(p); }
    if(!is_nil(p)){ emits(" . "); print_val(p); }
    emit_(')');
  }
}

// ---- exported entry points -------------------------------------------------
#define INCAP 8192
char inbuf[INCAP];

__attribute__((export_name("input_ptr")))  char* input_ptr(){ return inbuf; }
__attribute__((export_name("output_ptr"))) char* output_ptr(){ return outbuf; }
__attribute__((export_name("gc_count")))   unsigned gc_count(){ return g_reclaimed; }
__attribute__((export_name("eval_source")))
u32 eval_source(u32 len){
  init();
  rp=inbuf; rend=inbuf+(len<INCAP?len:INCAP);
  outlen=0;
  u32 result=NIL;
  for(;;){
    skipws();
    if(rp>=rend) break;
    g_reading = 1;                 // AST cells are pinned (never counted/freed)
    u32 form = read_expr();
    g_reading = 0;
    if(form==ERR){ result=ERR; break; }
    result = eval(form, g_env);
    if(result==ERR) break;
  }
  print_val(result);
  return outlen;
}
