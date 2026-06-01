// cek_gc.c — CEK machine + mark-sweep garbage collector.
//
// Same machine as cek.c. Only the arena management differs. Built to test H4
// (FINDINGS.md): does GC widen bytecode's lead under a tight heap? Prediction
// pre-registered there BEFORE this build was measured.
//
// What's added on top of cek.c:
//   * Mark-sweep collector lifted from bytecode_gc.c (non-moving, free-list
//     sweep, mark bits + explicit mark stack).
//   * CEK registers hoisted to file scope (R_C, R_E, R_K, R_V) so gc() sees
//     them as roots — same trick bytecode_gc used with R_vsp/R_csp/R_env.
//   * A small R_save shadow stack for two unrooted-local windows:
//        - K_ARGS, after `done = cons(V, done)` — the new `done` cell is only
//          in a C local until it's woven into a new K_ARGS frame, and k_args
//          allocates 6 conses to do that. GC firing in any of them would
//          collect the new done unless we pin it.
//        - `s_let`, building parallel `vars` and `vals` accumulator lists and
//          the lambda wrapping them: at several points one list is the latest
//          local while another cons() is in flight, and neither cons's
//          gc_a/gc_b reaches it.
//   Everywhere else, the "next cons's gc_a/gc_b protects the live cell"
//   discipline (the same one bytecode_gc relies on) already covers us —
//   `make_closure`, `mklist4/5`, `reverse`, `env_define`, `bind_params`,
//   `global_define` all chain their conses so the in-flight result is the cdr
//   of the next allocation.
//
// Build:
//   wasm:   clang ... -mtail-call -fno-builtin engines/cek_gc.c
//   native: -mtail-call is wasm-only; native musttail works without it.

typedef unsigned int   u32;
typedef int            i32;
typedef unsigned char  u8;

// Freestanding wasm has no libc. clang still emits memset/memcpy calls for
// the mark-bit clear and large static-array init; provide minimal versions
// (-fno-builtin keeps these from being lowered into calls to themselves).
void* memset(void* p, int c, unsigned long n){ u8* s=(u8*)p; while(n--) *s++=(u8)c; return p; }
void* memcpy(void* d, const void* s, unsigned long n){ u8* a=(u8*)d; const u8* b=(const u8*)s; while(n--) *a++=*b++; return d; }

// ---- tag scheme (identical to cek.c) ---------------------------------------
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
  PR_NULLP, PR_PAIRP, PR_LISTQ, PR_SETCAR, PR_SETCDR,
  SP_COUNT
};
#define NIL      mkspec(SP_NIL)
#define TRUE     mkspec(SP_T)
#define ERR      mkspec(SP_ERR)
#define UNBOUND  mkspec(SP_UNBOUND)
#define is_nil(v) ((v)==NIL)
#define is_prim(v) (tagof(v)==TAG_SPEC && ((v)>>2) >= PR_CONS && ((v)>>2) < SP_COUNT)

// ---- arena -----------------------------------------------------------------
// Match bytecode_gc.c so timings are directly comparable: 262144 cells default.
#define MAX_CELLS 262144
typedef struct { u32 car, cdr; } Cell;
static Cell  cells[MAX_CELLS];
static u32   cell_top = 0;
static int   g_oom = 0;

// ---- mark-sweep GC state ---------------------------------------------------
#define FREE_END 0xFFFFFFFFu
static u8  markbit[MAX_CELLS];
static u32 markstk[MAX_CELLS];     // explicit mark stack (bounds C recursion)
static u32 msp;
static u32 freelist = FREE_END;
static u32 gc_a, gc_b;             // protect cons() args across a collection
static int gc_enabled = 0;         // off during reader, on during run()
static u32 g_numgc = 0;            // GC count (stats)

// CEK registers hoisted to file scope so gc() can see them as roots.
// eval_expr writes R_C/R_E/R_K at entry; return_val writes R_V/R_K. The TAIL
// chain guarantees these stay current — no window where they're stale during
// an allocation.
static u32 R_C = 0, R_E = 0, R_K = 0, R_V = 0;

// Shadow stack for the few unrooted-local windows (see file header). Caller
// push/pops; gc() marks the live region. Max depth = 3 (s_let building the
// lambda wrapper). 64 is overkill but cheap.
#define SAVE_MAX 64
static u32 R_save[SAVE_MAX];
static u32 R_ssp = 0;

#define MAX_SYMS    1024
#define SYM_CHARS   16
static char symname[MAX_SYMS][SYM_CHARS];
static u32  symlen[MAX_SYMS];
static u32  sym_top = 0;

static void gc(void);

// Slow path (bump arena full): reuse a freed cell, or collect, or OOM. Out of
// line so the bump fast path stays free of any call and inlines.
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

// ---- symbol interner (identical) -------------------------------------------
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

// ---- reader (identical) ----------------------------------------------------
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

// ---- environment (identical) -----------------------------------------------
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
// PR2: walk env, mutate the binding cons's cdr in place. No allocation,
// GC-safe without shadow-stack discipline. See engines/lisp.c.
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
// g_head is the stable sentinel cell whose cdr is the global binding list.
// Mutating it in place keeps captured closures' env handles valid as globals
// are added. g_env aliases g_head.
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
// CEK MACHINE (semantics identical to cek.c; see that file's banner)
// ════════════════════════════════════════════════════════════════════════════

enum { K_HALT=0, K_IF, K_ARGS, K_DEFINE, K_SEQ, K_SETBANG };

static u32 g_khalt;
static u32 k_kind(u32 K){ return fixval(car(K)); }
static u32 kf(u32 K, int i){ u32 p=cdr(K); while(i--) p=cdr(p); return car(p); }

static u32 mklist4(u32 a,u32 b,u32 c,u32 d){ return cons(a,cons(b,cons(c,cons(d,NIL)))); }
static u32 mklist5(u32 a,u32 b,u32 c,u32 d,u32 e){ return cons(a,cons(b,cons(c,cons(d,cons(e,NIL))))); }

static u32 k_if(u32 thn,u32 els,u32 env,u32 next){
  return cons(mkfix(K_IF), mklist4(thn,els,env,next));
}
static u32 k_args(u32 fn,u32 done,u32 todo,u32 env,u32 next){
  return cons(mkfix(K_ARGS), mklist5(fn,done,todo,env,next));
}
static u32 k_define(u32 name,u32 next){
  return cons(mkfix(K_DEFINE), cons(name,cons(next,NIL)));
}
static u32 k_seq(u32 rest,u32 env,u32 next){
  return cons(mkfix(K_SEQ), cons(rest,cons(env,cons(next,NIL))));
}
// K_SETBANG: when V returns, env_set(env, sym, V). No new GC roots beyond
// what k_define already does — the cons calls follow the same pattern.
static u32 k_setbang(u32 sym,u32 env,u32 next){
  return cons(mkfix(K_SETBANG), cons(sym,cons(env,cons(next,NIL))));
}

static u32 reverse(u32 lst){
  u32 r=NIL; for(u32 p=lst; is_cons(p); p=cdr(p)) r=cons(car(p),r); return r;
}
// bind_params is safe without R_save: `env` is updated only through
// env_define's outer cons, which sets gc_a=<inner> and gc_b=env, so env is
// always the cdr of the next live allocation. p and the cursor a are derived
// from the closure's fn (rooted via R_K → K_ARGS slot 0) and from args (rooted
// by the caller in R_save); no allocation happens between the car/cdr reads.
static u32 bind_params(u32 fn, u32 args){
  u32 p=car(cdr(fn)), env=car(cdr(cdr(cdr(fn))));
  u32 a=args;
  for(; is_cons(p)&&is_cons(a); p=cdr(p),a=cdr(a))
    env=env_define(env,car(p),car(a));
  if(is_cons(p) || is_cons(a)) return ERR;            // arity mismatch
  return env;
}

// Both states share the (u32,u32,u32) signature for musttail. return_val
// ignores its middle arg.
static u32 eval_expr(u32 C, u32 E, u32 K);
static u32 return_val(u32 V, u32 _unused, u32 K);

#define TAIL __attribute__((musttail)) return

static u32 eval_expr(u32 C, u32 E, u32 K){
  R_C=C; R_E=E; R_K=K;                                // refresh roots
  if(g_oom) return ERR;
  if(is_fix(C) || tagof(C)==TAG_SPEC)
    { TAIL return_val(C, 0, K); }
  if(is_sym(C)){
    u32 v = env_lookup(E, C);
    if(v==UNBOUND) return ERR;
    TAIL return_val(v, 0, K);
  }
  u32 op = car(C);
  if(op==s_quote)  { TAIL return_val(car(cdr(C)), 0, K); }
  if(op==s_lambda) { TAIL return_val(make_closure(car(cdr(C)),car(cdr(cdr(C))),E), 0, K); }
  if(op==s_if){
    u32 k = k_if(car(cdr(cdr(C))), car(cdr(cdr(cdr(C)))), E, K);
    TAIL eval_expr(car(cdr(C)), E, k);
  }
  if(op==s_define){
    u32 head = car(cdr(C));
    if(is_cons(head)){
      // (define (f a b) body) -> name=f, val=(lambda (a b) body)
      // params/body reachable via C (R_C). make_closure chains cons protection.
      u32 name=car(head), params=cdr(head), body=car(cdr(cdr(C)));
      u32 clo = make_closure(params, body, E);
      global_define(name, clo);
      TAIL return_val(name, 0, K);
    }
    u32 k = k_define(head, K);
    TAIL eval_expr(car(cdr(cdr(C))), E, k);
  }
  if(op==s_setbang){
    // (set! sym expr) — strict arity 2. Queue K_SETBANG, eval value; the
    // K_SETBANG case in return_val performs env_set + propagates value.
    // No new R_save needed: k_setbang's cons chain follows the same
    // gc_a/gc_b discipline as k_define.
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
    // Desugar (cond (t1 e1) ... [(else en)]) -> nested if; tail-feed to eval_expr.
    // rev (reversed clause list) and expanded (in-progress nested-if) live in
    // R_save; clause cells stay reachable via the rooted C and rev.
    u32 base = R_ssp;
    R_save[R_ssp++] = NIL;                            // [base+0] = rev
    for(u32 c=cdr(C); is_cons(c); c=cdr(c))
      R_save[base+0] = cons(car(c), R_save[base+0]);
    R_save[R_ssp++] = NIL;                            // [base+1] = expanded
    for(u32 c=R_save[base+0]; is_cons(c); c=cdr(c)){
      u32 cl=car(c); u32 test=car(cl), body=car(cdr(cl));
      if(test==s_else){
        R_save[base+1] = body;
      } else {
        // (if test body expanded) — chained cons: each tmp protected as next gc_b.
        R_save[base+1] = cons(s_if, cons(test, cons(body, cons(R_save[base+1], NIL))));
      }
    }
    u32 expanded = R_save[base+1];
    R_ssp = base;
    TAIL eval_expr(expanded, E, K);
  }
  if(op==s_begin){
    u32 body=cdr(C);
    if(!is_cons(body)) { TAIL return_val(NIL,0,K); }
    if(!is_cons(cdr(body))) { TAIL eval_expr(car(body), E, K); }
    TAIL eval_expr(car(body), E, k_seq(cdr(body), E, K));
  }
  if(op==s_let){
    // Desugar (let ((a v)...) body) -> ((lambda (a...) body) v...).
    // vars and vals accumulate in parallel; the in-flight lambda wrapper
    // (body), (vars body), (lambda vars body) also needs protection. All
    // three lists live on R_save until the final application cons handoff.
    u32 binds=car(cdr(C)), body=car(cdr(cdr(C)));
    u32 base = R_ssp;
    R_save[R_ssp++] = NIL;                            // [base+0] = vars
    R_save[R_ssp++] = NIL;                            // [base+1] = vals
    for(u32 b=binds; is_cons(b); b=cdr(b)){
      R_save[base+0] = cons(car(car(b)),      R_save[base+0]);
      R_save[base+1] = cons(car(cdr(car(b))), R_save[base+1]);
    }
    R_save[R_ssp++] = cons(body, NIL);                // [base+2] = (body)
    R_save[base+2] = cons(R_save[base+0], R_save[base+2]);   // (vars body)
    R_save[base+2] = cons(s_lambda,        R_save[base+2]);  // (lambda vars body)
    u32 app = cons(R_save[base+2], R_save[base+1]);   // (lam . vals)
    R_ssp = base;
    TAIL eval_expr(app, E, K);
  }
  // application: evaluate operator first, remembering the unevaluated args
  TAIL eval_expr(op, E, k_args(UNBOUND, NIL, cdr(C), E, K));
}

static u32 return_val(u32 V, u32 _unused, u32 K){
  (void)_unused;
  R_V=V; R_K=K;                                       // refresh roots (R_E left as-is)
  if(g_oom) return ERR;
  if(V==ERR) return ERR;
  switch(k_kind(K)){
    case K_HALT:
      return V;

    case K_IF: {
      u32 branch = is_nil(V) ? kf(K,1) : kf(K,0);
      TAIL eval_expr(branch, kf(K,2), kf(K,3));
    }

    case K_DEFINE: {
      global_define(kf(K,0), V);
      TAIL return_val(kf(K,0), 0, kf(K,1));
    }

    case K_SETBANG: {
      u32 sym=kf(K,0), E=kf(K,1), next=kf(K,2);
      if(env_set(E, sym, V)==UNBOUND) return ERR;
      TAIL return_val(V, 0, next);
    }

    case K_SEQ: {
      u32 rest=kf(K,0), Ek=kf(K,1), next=kf(K,2);
      if(is_cons(cdr(rest)))
        { TAIL eval_expr(car(rest), Ek, k_seq(cdr(rest), Ek, next)); }
      TAIL eval_expr(car(rest), Ek, next);
    }

    case K_ARGS: {
      u32 fn=kf(K,0), done=kf(K,1), todo=kf(K,2), Ek=kf(K,3), next=kf(K,4);
      if(fn==UNBOUND) fn=V;
      else            done=cons(V, done);             // new `done` is only in this local
      if(is_cons(todo)){
        // Pin `done` across k_args's 6-cons allocation chain.
        R_save[R_ssp++] = done;
        u32 newK = k_args(fn, done, cdr(todo), Ek, next);
        R_ssp--;
        TAIL eval_expr(car(todo), Ek, newK);
      }
      // All args evaluated. Reverse to argument order; pin `done` across
      // reverse() (which is itself safe but allocates len(done) cells), then
      // pin the resulting `args` across the apply step.
      R_save[R_ssp++] = done;
      u32 args = reverse(done);
      R_ssp--;
      if(is_prim(fn)){
        R_save[R_ssp++] = args;
        u32 r = apply_prim(fn, args);
        R_ssp--;
        if(r==ERR) return ERR;
        TAIL return_val(r, 0, next);
      }
      if(is_closure(fn)){
        R_save[R_ssp++] = args;
        u32 E2 = bind_params(fn, args);
        R_ssp--;
        if(E2==ERR) return ERR;                       // arity mismatch
        TAIL eval_expr(car(cdr(cdr(fn))), E2, next);
      }
      return ERR;
    }
  }
  return ERR;
}

static u32 run(u32 expr){
  R_ssp = 0;
  return eval_expr(expr, g_env, g_khalt);
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
  // --- roots ---
  mark(gc_a); mark(gc_b);                  // in-flight cons args
  mark(R_C); mark(R_E); mark(R_K); mark(R_V);
  mark(g_env);                              // = g_head; covers all global bindings
  mark(g_khalt);                            // singleton halt continuation
  for(u32 i=0; i<R_ssp; i++) mark(R_save[i]);
  // --- trace (iterative; avoids deep C recursion) ---
  while(msp>0){ u32 i=markstk[--msp]; mark(cells[i].car); mark(cells[i].cdr); }
  // --- sweep: thread every unmarked cell onto the free list ---
  freelist=FREE_END;
  for(u32 i=0;i<MAX_CELLS;i++)
    if(!markbit[i]){ cells[i].cdr=freelist; freelist=i; }
}

// ---- global env & init -----------------------------------------------------
static void bindp(const char* nm, u32 prim){
  u32 l=0; while(nm[l])l++;
  u32 s=intern(nm,l);
  global_define(s,prim);
}
static void init(){
  cell_top=0; sym_top=0; g_oom=0; g_numgc=0; gc_enabled=0;
  freelist=FREE_END; R_ssp=0;
  R_C=R_E=R_K=R_V=0; gc_a=gc_b=0;
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
    // Reader allocates a small AST and has unrooted locals (first/last in
    // read_list). Keep GC off during reading; the bump arena handles tiny
    // ASTs comfortably, and any heavy allocation pressure is downstream in
    // run() where the registers are rooted.
    gc_enabled = 0;
    u32 form=read_expr();
    if(form==ERR){ result=ERR; break; }
    gc_enabled = 1;
    result=run(form);
    if(result==ERR) break;
  }
  print_val(result);
  return outlen;
}
