// lisp_region.c — tree-walker + region-drop "GC". Tests the H2 zero floor.
//
// Same evaluator as lisp.c. Only the allocator changes: cells allocate
// DOWNWARD from MAX_CELLS, and gc() is the one-liner `sp = considx(cdr(g_head))`
// — drop everything above the live env chain head. Lifted from
// Robert van Engelen's tinylisp.
//
// Why this engine exists: bytecode_gc / lisp_gc / cek_gc each pay a measurable
// "GC tax" (1.05× / 1.34× / 1.83× wasm) that we attributed in FINDINGS.md to
// a per-cons-callsite optimization barrier — cons() has a runtime branch to
// cons_slow(), a function the compiler can't prove away, so it pessimizes the
// surrounding hot loop. lisp_region tests that mechanism directly by removing
// the slow path entirely: cons() is just `sp--; write; return mkcons(sp)`. No
// function call, no branch to anywhere the compiler doesn't see. Pre-registered
// prediction: lisp_region / lisp_big ≈ 1.0× both substrates.
//
// THE INVARIANT (why this works):
//   Every cons(a, b) lands at a strictly lower cell index than min(a, b),
//   because allocation is strictly monotonic downward (sp--). So the set of
//   cells reachable through env via car/cdr is exactly cell[sp..MAX_CELLS-1].
//   gc() resets sp to the env head's index. Anything below sp (lower index)
//   is unreachable; future allocations overwrite it.
//
// THE SENTINEL TRICK:
//   global_define mutates g_head.cdr in place (matches lisp.c). g_head itself
//   is at MAX_CELLS-1 (allocated first, highest index, never moves). Its cdr
//   points at the live env chain head — which IS at low index (newest cell).
//   gc() anchors at considx(cdr(g_head)), NOT considx(g_head), so the entire
//   env chain is preserved and the sentinel stays put forever.
//
// LIMITATION (documented, accepted for this measurement experiment):
//   Closures created during eval that are NOT bound into env via define can't
//   survive past their creating top-level form. This is fine for our bench
//   suite — each benchmark is one top-level form, so gc() never fires during
//   the benchmark. The tax measurement is unaffected. For a fully general
//   collector, tinylisp's "store nil instead of e when e == env" closure
//   trick (lazy global-env re-resolution at call time) handles this; we
//   skip it because we don't need it.
//
// Build: -fno-builtin (memset stub keeps modules zero-imports). No -mtail-call
//        needed — same C-stack-recursive shape as lisp.c. clang's -O2 TRE
//        applies to `return eval(...)` self-calls just like in lisp.c.

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
  PR_NULLP, PR_PAIRP, PR_LISTQ,
  SP_COUNT
};
#define NIL      mkspec(SP_NIL)
#define TRUE     mkspec(SP_T)
#define ERR      mkspec(SP_ERR)
#define UNBOUND  mkspec(SP_UNBOUND)
#define is_nil(v) ((v)==NIL)
#define is_prim(v) (tagof(v)==TAG_SPEC && ((v)>>2) >= PR_CONS && ((v)>>2) < SP_COUNT)

// ---- arena: downward-growing stack ----------------------------------------
// MAX_CELLS matches bytecode_gc.c / cek_gc.c / lisp_gc.c default for like-for-like
// comparison. For the bench harness, a _big variant with 16M cells lines up
// with lisp_big.wasm / cek_big.wasm / bytecode_big.wasm.
#define MAX_CELLS 262144
typedef struct { u32 car, cdr; } Cell;
static Cell cells[MAX_CELLS];
static u32  sp = MAX_CELLS;     // grows DOWN: cell[--sp] is the next allocation
static int  g_oom = 0;
static u32  g_numgc = 0;        // counts gc() invocations

// cons() — pure local mutation. No fast/slow split, no function call,
// nothing for the compiler to flag as side-effectful beyond writing to
// cells[]. The whole H2-zero-floor hypothesis turns on this being exactly
// as cheap as lisp.c's bump cons.
static u32 cons(u32 a, u32 d){
  if(sp == 0){ g_oom = 1; return ERR; }
  sp--;
  cells[sp].car = a;
  cells[sp].cdr = d;
  return mkcons(sp);
}
static u32 car(u32 v){ return is_cons(v) ? cells[considx(v)].car : NIL; }
static u32 cdr(u32 v){ return is_cons(v) ? cells[considx(v)].cdr : NIL; }

// ---- symbol interner (identical to lisp.c — separate buffer, not in cell arena) ----
#define MAX_SYMS    1024
#define SYM_CHARS   16
static char symname[MAX_SYMS][SYM_CHARS];
static u32  symlen[MAX_SYMS];
static u32  sym_top = 0;

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

static u32 s_quote,s_if,s_define,s_lambda,s_let,s_begin,s_cond,s_else;

// ---- reader (identical to lisp.c) ------------------------------------------
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
  u32 first=NIL; u32 last=NIL;
  for(;;){
    skipws();
    if(rp>=rend) return first;
    if(*rp==')'){ rp++; return first; }
    u32 e=read_expr();
    u32 link=cons(e,NIL);
    if(link==ERR) return ERR;
    if(is_nil(first)){ first=link; last=link; }
    else { cells[considx(last)].cdr=link; last=link; }
  }
}
static u32 read_atom(){
  const char* start=rp;
  while(rp<rend && !is_delim(*rp)) rp++;
  u32 len=(u32)(rp-start);
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

// ---- environment + sentinel-mutation global_define (identical to lisp.c) ----
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
// Sentinel-mutation: g_head is the original cons cell (highest index, never
// moves); its cdr is the live env chain. Mutating in place lets closures
// captured at top level still see new bindings added later (top-level
// recursion works). gc() anchors at considx(cdr(g_head)), not considx(g_head),
// so the entire live chain is preserved while the sentinel stays put.
static u32 g_head;
static u32 g_env;
static void global_define(u32 sym, u32 val){
  u32 link=cons(cons(sym,val), cdr(g_head));
  cells[considx(g_head)].cdr = link;
}

// ---- closures + primitives (identical to lisp.c) ---------------------------
static u32 s_closure;
static u32 make_closure(u32 params, u32 body, u32 env){
  return cons(s_closure, cons(params, cons(body, cons(env, NIL))));
}
static int is_closure(u32 v){ return is_cons(v) && car(v)==s_closure; }

static u32 eval(u32 x, u32 env);

static u32 eval_list(u32 lst, u32 env){
  if(!is_cons(lst)) return NIL;
  u32 v=eval(car(lst),env);
  if(v==ERR) return ERR;
  u32 rest=eval_list(cdr(lst),env);
  if(rest==ERR) return ERR;
  return cons(v,rest);
}

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
    case PR_CAR:   if(!is_nil(d0) || !is_cons(a)) return ERR; return cells[considx(a)].car;
    case PR_CDR:   if(!is_nil(d0) || !is_cons(a)) return ERR; return cells[considx(a)].cdr;
  }

  if(!is_cons(d0)) return ERR;
  u32 b  = car(d0);
  u32 d1 = cdr(d0);

  switch(id){
    case PR_CONS:  if(!is_nil(d1)) return ERR; return cons(a,b);
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

// eval is BIT-FOR-BIT identical to lisp.c's eval. Same C-stack-recursive
// structure; same `return eval(...)` tail-position pattern that clang's -O2
// turns into a loop (H1). The point of this engine is the allocator change,
// nothing else.
static u32 eval(u32 x, u32 env){
  if(is_fix(x)) return x;
  if(tagof(x)==TAG_SPEC) return x;
  if(is_sym(x)){
    u32 v=env_lookup(env,x);
    return (v==UNBOUND)?ERR:v;
  }
  u32 op=car(x);
  if(op==s_quote)  return car(cdr(x));
  if(op==s_if){
    u32 t=eval(car(cdr(x)),env);
    if(t==ERR) return ERR;
    if(!is_nil(t)) return eval(car(cdr(cdr(x))),env);
    else           return eval(car(cdr(cdr(cdr(x)))),env);
  }
  if(op==s_define){
    u32 head=car(cdr(x));
    if(is_cons(head)){
      // (define (f a b) body) -> name=f, val=(lambda (a b) body)
      u32 name=car(head), params=cdr(head), body=car(cdr(cdr(x)));
      global_define(name, make_closure(params, body, env));
      return name;
    }
    u32 val=eval(car(cdr(cdr(x))),env);
    if(val==ERR) return ERR;
    global_define(head,val);
    return head;
  }
  if(op==s_cond){
    for(u32 c=cdr(x); is_cons(c); c=cdr(c)){
      u32 cl=car(c); u32 test=car(cl), body=car(cdr(cl));
      if(test==s_else) return eval(body,env);
      u32 t=eval(test,env);
      if(t==ERR) return ERR;
      if(!is_nil(t)) return eval(body,env);
    }
    return NIL;
  }
  if(op==s_lambda){
    return make_closure(car(cdr(x)), car(cdr(cdr(x))), env);
  }
  if(op==s_let){
    u32 binds=car(cdr(x));
    u32 newenv=env;
    for(u32 b=binds;is_cons(b);b=cdr(b)){
      u32 pair=car(b);
      u32 v=eval(car(cdr(pair)),env);
      if(v==ERR) return ERR;
      newenv=env_define(newenv,car(pair),v);
    }
    return eval(car(cdr(cdr(x))),newenv);
  }
  if(op==s_begin){
    u32 r=NIL;
    for(u32 b=cdr(x);is_cons(b);b=cdr(b)){ r=eval(car(b),env); if(r==ERR)return ERR; }
    return r;
  }
  u32 fn=eval(op,env);
  if(fn==ERR) return ERR;
  u32 args=eval_list(cdr(x),env);
  if(args==ERR) return ERR;
  if(is_prim(fn)) return apply_prim(fn,args);
  if(is_closure(fn)){
    u32 params=car(cdr(fn));
    u32 body  =car(cdr(cdr(fn)));
    u32 cenv  =car(cdr(cdr(cdr(fn))));
    u32 newenv=cenv;
    u32 p=params, a=args;
    while(is_cons(p) && is_cons(a)){
      newenv=env_define(newenv,car(p),car(a));
      p=cdr(p); a=cdr(a);
    }
    if(is_cons(p) || is_cons(a)) return ERR;        // arity mismatch
    return eval(body,newenv);
  }
  return ERR;
}

// ---- gc — the whole collector ---------------------------------------------
// One line: drop everything below the live env chain head. O(1), precise.
// The invariant (cons-monotonically-downward) guarantees that every cell
// reachable from cdr(g_head) has an index in [considx(cdr(g_head)), MAX_CELLS),
// so this is exactly equivalent to the set we want to preserve.
static void gc(void){
  g_numgc++;
  sp = considx(cdr(g_head));
}

// ---- init ------------------------------------------------------------------
static void bindp(const char* nm, u32 prim){
  u32 l=0; while(nm[l])l++;
  u32 s=intern(nm,l);
  global_define(s,prim);
}
static void init(){
  sp = MAX_CELLS; sym_top = 0; g_oom = 0; g_numgc = 0;
  s_quote =intern("quote",5);
  s_if    =intern("if",2);
  s_define=intern("define",6);
  s_lambda=intern("lambda",6);
  s_let   =intern("let",3);
  s_begin =intern("begin",5);
  s_cond  =intern("cond",4);
  s_else  =intern("else",4);
  s_closure=intern("%closure",8);
  // Sentinel: first cons goes to index MAX_CELLS-1 (highest possible),
  // stays there forever. Its cdr is mutated by every global_define.
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
  int first=1;
  for(;;){
    skipws();
    if(rp>=rend) break;
    // gc() between forms — before reading the next one. This pattern
    // preserves the LAST form's result (no gc fires after the final eval
    // and before print_val). Intermediate results are discarded — they
    // weren't bound to env, so they're garbage by definition.
    if(!first) gc();
    first = 0;
    u32 form=read_expr();
    if(form==ERR){ result=ERR; break; }
    result=eval(form,g_env);
    if(result==ERR) break;
  }
  print_val(result);
  return outlen;
}
