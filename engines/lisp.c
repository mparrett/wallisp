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
  PR_NULLP, PR_PAIRP, PR_LISTQ,
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

#define MAX_SYMS    1024
#define SYM_CHARS   16
static char  symname[MAX_SYMS][SYM_CHARS];
static u32   symlen[MAX_SYMS];
static u32   sym_top = 0;

static u32 cons(u32 a, u32 d){
  if (cell_top >= MAX_CELLS) return ERR;
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
static u32 s_quote,s_if,s_define,s_lambda,s_let,s_begin,s_cond,s_else;

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

static u32 eval(u32 x, u32 env);

static u32 eval_list(u32 lst, u32 env){ // eval each, return new list of results
  if(!is_cons(lst)) return NIL;
  u32 v=eval(car(lst),env);
  if(v==ERR) return ERR;
  u32 rest=eval_list(cdr(lst),env);
  if(rest==ERR) return ERR;
  return cons(v,rest);
}

// PR1: 30-bit signed fixnum range. mkfix/fixval shift by 2; results outside
// this band silently wrap, which was the project's known measurement hazard
// (tailsum(50000)). apply_prim traps overflow at each arithmetic op.
#define FIX_MAX  ((i32) 0x1FFFFFFF)   //  536870911
#define FIX_MIN  (-(i32)0x20000000)   // -536870912
static int fits_fix(i32 v){ return v >= FIX_MIN && v <= FIX_MAX; }
static int fits_fix64(long long v){ return v >= FIX_MIN && v <= FIX_MAX; }

// PR1: every primitive validates arity and operand types. `=` stays
// polymorphic (raw identity compare — the metacircular evaluator relies on
// `(= 'quote 'quote)`), but every other prim is strictly typed. +/-/* are
// properly variadic with ≥2 args (current binary-only behaviour silently
// dropped trailing args, e.g. (+ 1 2 3) -> 3). / and mod are new and require
// exactly 2 args; division by zero is an error.
//
// Arity is checked inline (was a list_len() walk in the first draft —
// ~13% fib regression; folded). The binary case of +/-/* stays on i32
// with a single fits_fix check; only the rare 3+ arg variadic path
// promotes to long long. Apple-clang -O2 hoists the id-comparison out
// of the variadic loop.
static u32 apply_prim(u32 prim, u32 args){
  u32 id=prim>>2;
  if(!is_cons(args)) return ERR;                     // all prims need ≥1
  u32 a  = car(args);
  u32 d0 = cdr(args);

  // unary prims: ≥1 args + exactly 1 (d0 == nil)
  switch(id){
    case PR_NULLP: if(!is_nil(d0)) return ERR; return is_nil(a)?TRUE:NIL;
    case PR_PAIRP: if(!is_nil(d0)) return ERR; return is_cons(a)?TRUE:NIL;
    case PR_LISTQ: if(!is_nil(d0)) return ERR; return (is_nil(a)||is_cons(a))?TRUE:NIL;
    case PR_CAR:   if(!is_nil(d0) || !is_cons(a)) return ERR; return cells[considx(a)].car;
    case PR_CDR:   if(!is_nil(d0) || !is_cons(a)) return ERR; return cells[considx(a)].cdr;
  }

  // ≥2 args from here
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
      i32 r=fixval(a)/bv; if(!fits_fix(r)) return ERR;   // catches FIX_MIN/-1
      return mkfix(r);
    }
    case PR_MOD: {
      if(!is_nil(d1) || !is_fix(a) || !is_fix(b)) return ERR;
      i32 bv=fixval(b); if(bv==0) return ERR;
      return mkfix(fixval(a)%bv);
    }
    case PR_ADD: case PR_SUB: case PR_MUL: {
      if(!is_fix(a) || !is_fix(b)) return ERR;
      // Binary fast path: 30-bit + 30-bit fits in i32 — single fits_fix
      // is enough for ADD/SUB. MUL needs i64 since 30*30 = 60 bits.
      i32 av=fixval(a), bv=fixval(b), r;
      if(id==PR_MUL){
        long long m=(long long)av*(long long)bv;
        if(!fits_fix64(m)) return ERR;
        r=(i32)m;
      } else {
        r=(id==PR_ADD)?(av+bv):(av-bv);
        if(!fits_fix(r)) return ERR;
      }
      if(is_nil(d1)) return mkfix(r);                  // common case: binary
      // ≥3 args: continue accumulating in i64 with overflow check per step.
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

static u32 eval(u32 x, u32 env){
  // self-evaluating
  if(is_fix(x)) return x;
  if(tagof(x)==TAG_SPEC) return x;          // nil, t, prims evaluate to self
  if(is_sym(x)){
    u32 v=env_lookup(env,x);
    return (v==UNBOUND)?ERR:v;
  }
  // must be a list form
  u32 op=car(x);
  // special forms (compared by interned symbol identity — reorder-safe)
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
    // (let ((a v) (b w)) body) -> extend env, eval body
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
  // application
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

// ---- global env & init -----------------------------------------------------
u32 g_env;
static void bindp(const char* nm, u32 prim){
  u32 l=0; while(nm[l])l++;
  u32 s=intern(nm,l);
  global_define(s,prim);
}
static void init(){
  cell_top=0; sym_top=0;
  s_quote =intern("quote",5);
  s_if    =intern("if",2);
  s_define=intern("define",6);
  s_lambda=intern("lambda",6);
  s_let   =intern("let",3);
  s_begin =intern("begin",5);
  s_cond  =intern("cond",4);
  s_else  =intern("else",4);
  s_closure=intern("%closure",8);
  // sentinel head: car is an unbound marker that matches no real symbol.
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
    result=eval(form,g_env);
    if(result==ERR) break;
  }
  print_val(result);
  return outlen;
}
