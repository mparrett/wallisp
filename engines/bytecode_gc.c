// bytecode_gc.c — bytecode VM + a mark-sweep garbage collector.
// Non-moving collector: indices never change, so roots are only MARKED, never
// rewritten. All heap objects are uniform cons cells, so the free list has no
// fragmentation. GC runs only during execution (the compile-phase AST isn't
// rooted, so collection is disabled until run() starts).
//
// the same tiny Lisp, COMPILED to bytecode and run on a stack
// machine, for an apples-to-apples speed A/B against the tree-walker (lisp.c)
// and the CEK machine (cek.c). Same reader, printer, arena, primitives.
//
// The idea: walk each expression's cons-tree ONCE at compile time into a flat
// u32 instruction array, resolving variables to lexical (depth,index) addresses
// and special forms to jumps. The VM then fetch-decode-executes with an operand
// stack — no re-walking the tree, no special-form dispatch, no arg-list consing
// on the hot path. Globals are an O(1) array indexed by symbol id.

typedef unsigned int u32;
typedef int          i32;
typedef unsigned char u8;

// Freestanding builds have no libc; clang still emits memset/memcpy for array
// clears (e.g. the GC mark-bit reset). Provide minimal versions (-fno-builtin
// keeps these from being optimized into calls to themselves).
void* memset(void* p, int c, unsigned long n){ u8* s=(u8*)p; while(n--) *s++=(u8)c; return p; }
void* memcpy(void* d, const void* s, unsigned long n){ u8* a=(u8*)d; const u8* b=(const u8*)s; while(n--) *a++=*b++; return d; }

// ---- tag scheme (identical to lisp.c) --------------------------------------
#define TAG_MASK 3u
#define TAG_FIX 0u
#define TAG_CONS 1u
#define TAG_SYM 2u
#define TAG_SPEC 3u
#define mkfix(n)  ((u32)(((i32)(n))<<2)|TAG_FIX)
#define fixval(v) (((i32)(v))>>2)
#define mkcons(i) (((u32)(i)<<2)|TAG_CONS)
#define considx(v)((v)>>2)
#define mksym(i)  (((u32)(i)<<2)|TAG_SYM)
#define symidx(v) ((v)>>2)
#define mkspec(i) (((u32)(i)<<2)|TAG_SPEC)
#define tagof(v)  ((v)&TAG_MASK)
#define is_fix(v) (tagof(v)==TAG_FIX)
#define is_cons(v)(tagof(v)==TAG_CONS)
#define is_sym(v) (tagof(v)==TAG_SYM)

enum { SP_NIL=0, SP_T, SP_ERR, SP_UNBOUND,
       PR_CONS, PR_CAR, PR_CDR, PR_ADD, PR_SUB, PR_MUL, PR_DIV, PR_MOD, PR_EQ, PR_LT,
       PR_NULLP, PR_PAIRP, PR_LISTQ, PR_SETCAR, PR_SETCDR, SP_COUNT };
#define NIL mkspec(SP_NIL)
#define TRUE mkspec(SP_T)
#define ERR mkspec(SP_ERR)
#define UNBOUND mkspec(SP_UNBOUND)
#define is_nil(v) ((v)==NIL)
#define is_prim(v) (tagof(v)==TAG_SPEC && ((v)>>2)>=PR_CONS && ((v)>>2)<SP_COUNT)

// ---- arena (identical) -----------------------------------------------------
#define MAX_CELLS 262144
typedef struct { u32 car, cdr; } Cell;
static Cell cells[MAX_CELLS];
static int  g_oom=0;

// ---- mark-sweep GC state ---------------------------------------------------
#define FREE_END 0xFFFFFFFFu
static u8  markbit[MAX_CELLS];
static u32 markstk[MAX_CELLS];     // explicit mark stack (bounds C recursion)
static u32 msp;                    // mark-stack pointer
static u32 freelist=FREE_END;      // head of free-cell list
static u32 gc_a, gc_b;             // protect cons() args across a collection
static int gc_enabled=0;           // off during compile, on during run()
static u32 g_numgc=0;              // GC count (stats)
// VM registers, hoisted to file scope so gc() can see them as roots:
static u32 R_vsp=0, R_csp=0, R_env=0;
static void gc(void);
#define MAX_SYMS 1024
#define SYM_CHARS 16
static char symname[MAX_SYMS][SYM_CHARS];
static u32  symlen[MAX_SYMS];
static u32  sym_top=0;

static u32 cell_top=0;             // bump frontier (used until the arena fills)
static void gc(void);
// Slow path (arena full): reuse a freed cell, or collect, or OOM. Out of line so
// the fast path stays free of any call and inlines into allocation sites.
static u32 cons_slow(u32 a,u32 d){
  u32 i;
  if(freelist!=FREE_END){ i=freelist; freelist=cells[i].cdr; }
  else {
    if(gc_enabled){ gc_a=a; gc_b=d; gc(); }
    if(freelist!=FREE_END){ i=freelist; freelist=cells[i].cdr; }
    else { g_oom=1; return ERR; }
  }
  cells[i].car=a; cells[i].cdr=d; return mkcons(i);
}
static inline u32 cons(u32 a,u32 d){
  if(cell_top<MAX_CELLS){ u32 i=cell_top++; cells[i].car=a; cells[i].cdr=d; return mkcons(i); }
  return cons_slow(a,d);                                         // full -> out of line
}
static u32 car(u32 v){ return is_cons(v)?cells[considx(v)].car:NIL; }
static u32 cdr(u32 v){ return is_cons(v)?cells[considx(v)].cdr:NIL; }

static int streq(const char*a,u32 la,const char*b,u32 lb){
  if(la!=lb)return 0; for(u32 i=0;i<la;i++) if(a[i]!=b[i])return 0; return 1;
}
static u32 intern(const char*s,u32 len){
  if(len>SYM_CHARS)len=SYM_CHARS;
  for(u32 i=0;i<sym_top;i++) if(streq(symname[i],symlen[i],s,len)) return mksym(i);
  if(sym_top>=MAX_SYMS) return ERR;
  u32 i=sym_top++; for(u32 k=0;k<len;k++) symname[i][k]=s[k]; symlen[i]=len;
  return mksym(i);
}
static u32 s_quote,s_if,s_define,s_lambda,s_let,s_begin,s_closure,s_cond,s_else,s_setbang;

// ---- reader (identical) ----------------------------------------------------
static const char*rp; static const char*rend;
static void skipws(){
  while(rp<rend){ char c=*rp;
    if(c==' '||c=='\t'||c=='\n'||c=='\r')rp++;
    else if(c==';'){ while(rp<rend&&*rp!='\n')rp++; }
    else break; }
}
static int is_delim(char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='('||c==')'||c==';'||c==0; }
static u32 read_expr();
static u32 read_list(){
  u32 first=NIL,last=NIL;
  for(;;){ skipws();
    if(rp>=rend) return first;
    if(*rp==')'){ rp++; return first; }
    u32 e=read_expr(); u32 link=cons(e,NIL); if(link==ERR)return ERR;
    if(is_nil(first)){first=link;last=link;} else {cells[considx(last)].cdr=link;last=link;}
  }
}
static u32 read_atom(){
  const char*start=rp; while(rp<rend&&!is_delim(*rp))rp++;
  u32 len=(u32)(rp-start);
  int neg=0; const char*p=start; u32 n=len;
  if(n>0&&(*p=='-'||*p=='+')){neg=(*p=='-');p++;n--;}
  if(n>0){ int isnum=1; i32 val=0;
    for(u32 i=0;i<n;i++){char c=p[i]; if(c<'0'||c>'9'){isnum=0;break;} val=val*10+(c-'0');}
    if(isnum) return mkfix(neg?-val:val); }
  return intern(start,len);
}
static u32 read_expr(){
  skipws(); if(rp>=rend) return ERR;
  char c=*rp;
  if(c=='('){rp++; return read_list();}
  if(c=='\''){rp++; u32 e=read_expr(); return cons(s_quote,cons(e,NIL));}
  return read_atom();
}

// ---- primitives (PR1: validated; see engines/lisp.c for the design notes) --
#define FIX_MAX  ((i32) 0x1FFFFFFF)   //  536870911
#define FIX_MIN  (-(i32)0x20000000)   // -536870912
static int fits_fix(i32 v){ return v >= FIX_MIN && v <= FIX_MAX; }
static int fits_fix64(long long v){ return v >= FIX_MIN && v <= FIX_MAX; }

// Called only when the inline-prim fast path in OP_CALL/OP_TAILCALL doesn't
// match: variadic +/-/* with n≥3, plus any prim invoked with a wrong arity.
// The hot path (binary +/-/* /=/<, unary car/cdr/null?/pair?/list?) is
// inlined and validated at the VM level — apply_prim is the slow path.
static u32 apply_prim(u32 prim,u32 args){
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

// ---- globals: O(1) array indexed by symbol id ------------------------------
static u32 gval[MAX_SYMS];
static void global_define(u32 sym,u32 val){ gval[symidx(sym)]=val; }

// ---- closures: (s_closure body_addr nparams env) ---------------------------
static u32 mkclosure(u32 body_addr,u32 nparams,u32 env){
  return cons(s_closure, cons(mkfix(body_addr), cons(mkfix(nparams), cons(env,NIL))));
}
static int is_closure(u32 v){ return is_cons(v)&&car(v)==s_closure; }
#define clo_body(c)    fixval(car(cdr(c)))
#define clo_nparams(c) fixval(car(cdr(cdr(c))))
#define clo_env(c)     car(cdr(cdr(cdr(c))))

// ════════════════════════════════════════════════════════════════════════════
// COMPILER: cons-tree AST -> flat bytecode (done ONCE per expression)
// ════════════════════════════════════════════════════════════════════════════
enum {
  OP_CONST, OP_LOADL, OP_LOADG, OP_DEFG, OP_SETL, OP_SETG, OP_POP,
  OP_JMP, OP_JFALSE, OP_CLOSURE, OP_CALL, OP_TAILCALL, OP_RET, OP_HALT
};
#define CODE_MAX 65536
static u32 code[CODE_MAX];
static u32 cp=0;                       // code pointer (emit cursor)
static int g_cerr=0;                   // compile error flag
static void emit(u32 w){ if(cp<CODE_MAX) code[cp++]=w; else g_cerr=1; }

// compile-time env: a cons-list of frames; each frame a cons-list of param syms.
static int resolve(u32 sym,u32 cenv,u32*d,u32*i){
  u32 depth=0;
  for(u32 f=cenv; is_cons(f); f=cdr(f),depth++){
    u32 idx=0;
    for(u32 p=car(f); is_cons(p); p=cdr(p),idx++)
      if(car(p)==sym){ *d=depth; *i=idx; return 1; }
  }
  return 0;
}

// compile(x, cenv, tail): if tail is set, the emitted code RETURNS control to
// the caller — every tail path ends in OP_RET or OP_TAILCALL. If not, it just
// leaves the value on the operand stack. A lambda body is always compiled with
// tail=1 (its last expression is, by definition, in tail position).
static void compile(u32 x,u32 cenv,int tail){
  if(is_fix(x) || tagof(x)==TAG_SPEC){ emit(OP_CONST); emit(x); if(tail)emit(OP_RET); return; }
  if(is_sym(x)){
    u32 d,i;
    if(resolve(x,cenv,&d,&i)){ emit(OP_LOADL); emit(d); emit(i); }
    else { emit(OP_LOADG); emit(symidx(x)); }
    if(tail)emit(OP_RET); return;
  }
  u32 op=car(x);
  if(op==s_quote){ emit(OP_CONST); emit(car(cdr(x))); if(tail)emit(OP_RET); return; }
  if(op==s_if){
    compile(car(cdr(x)),cenv,0);                  // test (never tail)
    emit(OP_JFALSE); u32 p1=cp; emit(0);
    compile(car(cdr(cdr(x))),cenv,tail);          // then
    if(tail){
      // then-branch self-terminated (RET/TAILCALL); else falls right here
      code[p1]=cp;
      compile(car(cdr(cdr(cdr(x)))),cenv,tail);   // else
    } else {
      emit(OP_JMP); u32 p2=cp; emit(0);
      code[p1]=cp;
      compile(car(cdr(cdr(cdr(x)))),cenv,0);
      code[p2]=cp;
    }
    return;
  }
  if(op==s_define){
    u32 head=car(cdr(x));
    if(is_cons(head)){
      // (define (f a b) body) -> (define f (lambda (a b) body))
      u32 name=car(head), params=cdr(head), body=car(cdr(cdr(x)));
      u32 lam=cons(s_lambda,cons(params,cons(body,NIL)));
      u32 d=cons(s_define,cons(name,cons(lam,NIL)));
      compile(d,cenv,tail); return;
    }
    compile(car(cdr(cdr(x))),cenv,0);
    emit(OP_DEFG); emit(symidx(head));
    if(tail)emit(OP_RET); return;
  }
  if(op==s_setbang){
    // (set! sym expr) — strict arity 2, sym must be a symbol. Resolves to
    // OP_SETL (local: walk d frames + i positions, mutate cell.car) or
    // OP_SETG (global: gval[s] = top, error if previously unbound). The
    // value stays on the operand stack as the result of set!.
    u32 d1=cdr(x);
    if(!is_cons(d1)){ g_cerr=1; emit(OP_CONST); emit(ERR); if(tail)emit(OP_RET); return; }
    u32 d2=cdr(d1);
    if(!is_cons(d2) || !is_nil(cdr(d2))){ g_cerr=1; emit(OP_CONST); emit(ERR); if(tail)emit(OP_RET); return; }
    u32 sym=car(d1);
    if(!is_sym(sym)){ g_cerr=1; emit(OP_CONST); emit(ERR); if(tail)emit(OP_RET); return; }
    compile(car(d2), cenv, 0);                       // value on stack
    u32 d, i;
    if(resolve(sym, cenv, &d, &i)){ emit(OP_SETL); emit(d); emit(i); }
    else                          { emit(OP_SETG); emit(symidx(sym)); }
    if(tail)emit(OP_RET); return;
  }
  if(op==s_cond){
    // (cond (t1 e1) ... (else en)) -> (if t1 e1 (if t2 e2 ... en))
    // No clauses, or fall through past all of them, yields NIL.
    u32 expanded=NIL;
    // Walk clauses, collecting in order; then fold from the back.
    u32 cls=cdr(x);
    // Reverse onto rev so we can fold tail-first without recursion through C stack.
    u32 rev=NIL; for(u32 c=cls;is_cons(c);c=cdr(c)) rev=cons(car(c),rev);
    for(u32 c=rev;is_cons(c);c=cdr(c)){
      u32 cl=car(c); u32 test=car(cl), body=car(cdr(cl));
      if(test==s_else) expanded=body;
      else expanded=cons(s_if,cons(test,cons(body,cons(expanded,NIL))));
    }
    compile(expanded,cenv,tail); return;
  }
  if(op==s_lambda){
    u32 params=car(cdr(x)), body=car(cdr(cdr(x)));
    u32 nparams=0; for(u32 p=params;is_cons(p);p=cdr(p)) nparams++;
    emit(OP_JMP); u32 pj=cp; emit(0);             // skip the body at definition time
    u32 body_addr=cp;
    compile(body, cons(params,cenv), 1);          // body is tail; self-terminates
    code[pj]=cp;
    emit(OP_CLOSURE); emit(body_addr); emit(nparams);
    if(tail)emit(OP_RET); return;
  }
  if(op==s_begin){
    u32 b=cdr(x);
    if(!is_cons(b)){ emit(OP_CONST); emit(NIL); if(tail)emit(OP_RET); return; }
    while(is_cons(cdr(b))){ compile(car(b),cenv,0); emit(OP_POP); b=cdr(b); }
    compile(car(b),cenv,tail);                     // last expr inherits tail
    return;
  }
  if(op==s_let){
    u32 binds=car(cdr(x)), body=car(cdr(cdr(x)));
    u32 vars=NIL,vals=NIL;
    for(u32 bnd=binds;is_cons(bnd);bnd=cdr(bnd)){
      vars=cons(car(car(bnd)),vars);
      vals=cons(car(cdr(car(bnd))),vals);
    }
    u32 lam=cons(s_lambda,cons(vars,cons(body,NIL)));
    compile(cons(lam,vals),cenv,tail);             // application inherits tail
    return;
  }
  // application
  compile(op,cenv,0);
  u32 n=0;
  for(u32 a=cdr(x);is_cons(a);a=cdr(a)){ compile(car(a),cenv,0); n++; }
  if(tail){ emit(OP_TAILCALL); emit(n); }          // reuse frame, no return push
  else    { emit(OP_CALL);     emit(n); }
}

// ════════════════════════════════════════════════════════════════════════════
// VM: fetch-decode-execute over `code`, with an operand stack + call stack.
// Runtime frame = cons(values_list, parent_frame). LOADL d i walks d parents
// then i into the values. Globals are gval[]. Closures capture their frame.
// ════════════════════════════════════════════════════════════════════════════
#define VSTACK_MAX 65536
#define CALL_MAX   65536
static u32 vstack[VSTACK_MAX];
static u32 ret_ip[CALL_MAX];
static u32 ret_env[CALL_MAX];

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
  mark(gc_a); mark(gc_b);                          // pending cons() args
  mark(R_env);                                     // current environment
  for(u32 k=0;k<R_vsp;k++) mark(vstack[k]);        // operand stack
  for(u32 k=0;k<R_csp;k++) mark(ret_env[k]);       // saved frames
  for(u32 sj=0;sj<sym_top;sj++)                    // globals
    if(gval[sj]!=UNBOUND) mark(gval[sj]);
  // quoted data: OP_CONST operands embedded in the code stream
  { u32 p=0;
    while(p<cp){ u32 op=code[p++];
      switch(op){
        case OP_CONST: mark(code[p]); p+=1; break;
        case OP_LOADL: case OP_SETL: case OP_CLOSURE: p+=2; break;
        case OP_LOADG: case OP_DEFG: case OP_SETG: case OP_JMP:
        case OP_JFALSE: case OP_CALL: case OP_TAILCALL: p+=1; break;
        default: break;                            // OP_POP/OP_RET/OP_HALT: 0
      }
    }
  }
  // --- trace (iterative; mark stack avoids deep C recursion) ---
  while(msp>0){ u32 i=markstk[--msp]; mark(cells[i].car); mark(cells[i].cdr); }
  // --- sweep: thread every unmarked cell onto the free list ---
  freelist=FREE_END;
  for(u32 i=0;i<MAX_CELLS;i++)
    if(!markbit[i]){ cells[i].cdr=freelist; freelist=i; }
}

static u32 run(u32 entry){
  u32 ip=entry; R_env=NIL; R_vsp=0; R_csp=0;
  for(;;){
    if(g_oom) return ERR;
    u32 opc=code[ip++];
    switch(opc){
      case OP_CONST: vstack[R_vsp++]=code[ip++]; break;
      case OP_LOADL: {
        u32 d=code[ip++], i=code[ip++], f=R_env;
        while(d--) f=cdr(f);
        u32 v=car(f); while(i--) v=cdr(v);
        vstack[R_vsp++]=car(v); break;
      }
      case OP_LOADG: {
        u32 v=gval[code[ip++]];
        if(v==UNBOUND) return ERR;
        vstack[R_vsp++]=v; break;
      }
      case OP_DEFG: {
        u32 s=code[ip++]; gval[s]=vstack[R_vsp-1]; vstack[R_vsp-1]=mksym(s); break;
      }
      case OP_SETL: {
        // (set! local-x value): walk d frames, walk i into frame's values
        // list, mutate the cons cell's car. Leaves value on the stack.
        u32 d=code[ip++], i=code[ip++], f=R_env;
        while(d--) f=cdr(f);
        u32 v=car(f); while(i--) v=cdr(v);
        cells[considx(v)].car = vstack[R_vsp-1]; break;
      }
      case OP_SETG: {
        // (set! global-x value): error if previously unbound. Leaves value
        // on the stack (set! returns the assigned value — matches lisp.c).
        u32 s=code[ip++];
        if(gval[s]==UNBOUND) return ERR;
        gval[s]=vstack[R_vsp-1]; break;
      }
      case OP_POP: R_vsp--; break;
      case OP_JMP: ip=code[ip]; break;
      case OP_JFALSE: { u32 t=code[ip++]; if(is_nil(vstack[--R_vsp])) ip=t; break; }
      case OP_CLOSURE: { u32 ba=code[ip++], np=code[ip++]; vstack[R_vsp++]=mkclosure(ba,np,R_env); break; }
      case OP_CALL: {
        u32 n=code[ip++]; u32 fn=vstack[R_vsp-n-1];
        if(is_prim(fn)){
          u32 id=fn>>2;
          if(n==2 && id>=PR_ADD && id<=PR_LT){      // inline 2-arg arith/cmp: no cons
            // PR1: type-check, overflow-trap, div-by-zero. PR_EQ stays
            // polymorphic; metacircular eval uses `(= 'quote 'quote)`.
            u32 a=vstack[R_vsp-2], b=vstack[R_vsp-1], r;
            if(id != PR_EQ && (!is_fix(a) || !is_fix(b))) return ERR;
            switch(id){
              case PR_ADD: { i32 s=fixval(a)+fixval(b); if(!fits_fix(s)) return ERR; r=mkfix(s); break; }
              case PR_SUB: { i32 d=fixval(a)-fixval(b); if(!fits_fix(d)) return ERR; r=mkfix(d); break; }
              case PR_MUL: { long long m=(long long)fixval(a)*(long long)fixval(b); if(!fits_fix64(m)) return ERR; r=mkfix((i32)m); break; }
              case PR_DIV: { i32 bv=fixval(b); if(bv==0) return ERR; i32 q=fixval(a)/bv; if(!fits_fix(q)) return ERR; r=mkfix(q); break; }
              case PR_MOD: { i32 bv=fixval(b); if(bv==0) return ERR; r=mkfix(fixval(a)%bv); break; }
              case PR_EQ:  r=(a==b)?TRUE:NIL; break;
              default:     r=(fixval(a)<fixval(b))?TRUE:NIL; break; // PR_LT
            }
            R_vsp-=3; vstack[R_vsp++]=r;
          } else if(n==1 && (id==PR_CAR||id==PR_CDR||(id>=PR_NULLP&&id<=PR_LISTQ))){
            // inline 1-arg car/cdr/null?/pair?/list?: skip arg-list cons + apply_prim
            u32 a=vstack[R_vsp-1], r;
            switch(id){
              case PR_CAR:   if(!is_cons(a)) return ERR; r=cells[considx(a)].car; break;
              case PR_CDR:   if(!is_cons(a)) return ERR; r=cells[considx(a)].cdr; break;
              case PR_NULLP: r=is_nil(a)?TRUE:NIL; break;
              case PR_PAIRP: r=is_cons(a)?TRUE:NIL; break;
              default:       r=(is_nil(a)||is_cons(a))?TRUE:NIL; break; // PR_LISTQ
            }
            R_vsp-=2; vstack[R_vsp++]=r;
          } else {
            u32 args=NIL; for(i32 k=(i32)R_vsp-1;k>=(i32)(R_vsp-n);k--) args=cons(vstack[k],args);
            u32 r=apply_prim(fn,args);
            R_vsp-=(n+1); vstack[R_vsp++]=r;
          }
        } else if(is_closure(fn)){
          if(n!=clo_nparams(fn)) return ERR;        // arity mismatch
          u32 vals=NIL; for(i32 k=(i32)R_vsp-1;k>=(i32)(R_vsp-n);k--) vals=cons(vstack[k],vals);
          u32 frame=cons(vals, clo_env(fn));
          R_vsp-=(n+1);
          if(R_csp>=CALL_MAX) return ERR;
          ret_ip[R_csp]=ip; ret_env[R_csp]=R_env; R_csp++;
          R_env=frame; ip=clo_body(fn);
        } else return ERR;                          // applied a non-function
        break;
      }
      case OP_TAILCALL: {
        // Tail position: do NOT push a return frame. A closure reuses the
        // current return context (its eventual RET goes to OUR caller), so a
        // tail loop runs in constant call-stack space. A prim produces the
        // value and returns it immediately.
        u32 n=code[ip++]; u32 fn=vstack[R_vsp-n-1];
        if(is_closure(fn)){
          if(n!=clo_nparams(fn)) return ERR;          // arity mismatch
          u32 vals=NIL; for(i32 k=(i32)R_vsp-1;k>=(i32)(R_vsp-n);k--) vals=cons(vstack[k],vals);
          u32 frame=cons(vals, clo_env(fn));
          R_vsp-=(n+1);
          R_env=frame; ip=clo_body(fn);               // reuse frame: no R_csp push
        } else if(is_prim(fn)){
          u32 id=fn>>2; u32 r;
          if(n==2 && id>=PR_ADD && id<=PR_LT){      // inline 2-arg arith/cmp: no cons
            u32 a=vstack[R_vsp-2], b=vstack[R_vsp-1];
            if(id != PR_EQ && (!is_fix(a) || !is_fix(b))) return ERR;
            switch(id){
              case PR_ADD: { i32 s=fixval(a)+fixval(b); if(!fits_fix(s)) return ERR; r=mkfix(s); break; }
              case PR_SUB: { i32 d=fixval(a)-fixval(b); if(!fits_fix(d)) return ERR; r=mkfix(d); break; }
              case PR_MUL: { long long m=(long long)fixval(a)*(long long)fixval(b); if(!fits_fix64(m)) return ERR; r=mkfix((i32)m); break; }
              case PR_DIV: { i32 bv=fixval(b); if(bv==0) return ERR; i32 q=fixval(a)/bv; if(!fits_fix(q)) return ERR; r=mkfix(q); break; }
              case PR_MOD: { i32 bv=fixval(b); if(bv==0) return ERR; r=mkfix(fixval(a)%bv); break; }
              case PR_EQ:  r=(a==b)?TRUE:NIL; break;
              default:     r=(fixval(a)<fixval(b))?TRUE:NIL; break; // PR_LT
            }
            R_vsp-=3; vstack[R_vsp++]=r;
          } else if(n==1 && (id==PR_CAR||id==PR_CDR||(id>=PR_NULLP&&id<=PR_LISTQ))){
            // inline 1-arg car/cdr/null?/pair?/list?: skip arg-list cons + apply_prim
            u32 a=vstack[R_vsp-1];
            switch(id){
              case PR_CAR:   if(!is_cons(a)) return ERR; r=cells[considx(a)].car; break;
              case PR_CDR:   if(!is_cons(a)) return ERR; r=cells[considx(a)].cdr; break;
              case PR_NULLP: r=is_nil(a)?TRUE:NIL; break;
              case PR_PAIRP: r=is_cons(a)?TRUE:NIL; break;
              default:       r=(is_nil(a)||is_cons(a))?TRUE:NIL; break; // PR_LISTQ
            }
            R_vsp-=2; vstack[R_vsp++]=r;
          } else {
            u32 args=NIL; for(i32 k=(i32)R_vsp-1;k>=(i32)(R_vsp-n);k--) args=cons(vstack[k],args);
            r=apply_prim(fn,args);
            R_vsp-=(n+1); vstack[R_vsp++]=r;
          }
          if(R_csp==0) return r;                       // (shouldn't happen below top level)
          R_csp--; ip=ret_ip[R_csp]; R_env=ret_env[R_csp];   // return the value to caller
        } else return ERR;
        break;
      }
      case OP_RET: { R_csp--; ip=ret_ip[R_csp]; R_env=ret_env[R_csp]; break; } // result stays on vstack
      case OP_HALT: return vstack[R_vsp-1];
    }
  }
}

// ---- init ------------------------------------------------------------------
static void bindp(const char*nm,u32 prim){ u32 l=0; while(nm[l])l++; global_define(intern(nm,l),prim); }
static void init(){
  cell_top=0; sym_top=0; g_oom=0; cp=0; g_cerr=0; g_numgc=0; gc_enabled=0;
  freelist=FREE_END;
  for(u32 i=0;i<MAX_SYMS;i++) gval[i]=UNBOUND;
  s_quote=intern("quote",5); s_if=intern("if",2); s_define=intern("define",6);
  s_lambda=intern("lambda",6); s_let=intern("let",3); s_begin=intern("begin",5);
  s_closure=intern("%closure",8);
  s_cond=intern("cond",4); s_else=intern("else",4);
  s_setbang=intern("set!",4);
  global_define(intern("nil",3),NIL); global_define(intern("t",1),TRUE);
  bindp("cons",mkspec(PR_CONS)); bindp("car",mkspec(PR_CAR)); bindp("cdr",mkspec(PR_CDR));
  bindp("+",mkspec(PR_ADD)); bindp("-",mkspec(PR_SUB)); bindp("*",mkspec(PR_MUL));
  bindp("/",mkspec(PR_DIV)); bindp("mod",mkspec(PR_MOD));
  bindp("=",mkspec(PR_EQ)); bindp("<",mkspec(PR_LT));
  bindp("null?",mkspec(PR_NULLP)); bindp("pair?",mkspec(PR_PAIRP)); bindp("list?",mkspec(PR_LISTQ));
  bindp("set-car!",mkspec(PR_SETCAR)); bindp("set-cdr!",mkspec(PR_SETCDR));
}

// ---- printer (identical) ---------------------------------------------------
// Output buffer. Override at build time (e.g., -DOUTCAP=262144 for the
// disasm variant, where the bytecode listing is much larger than any
// program result we'd otherwise print).
#ifndef OUTCAP
#define OUTCAP 4096
#endif
static char outbuf[OUTCAP]; static u32 outlen;
static void oemit(char c){ if(outlen<OUTCAP)outbuf[outlen++]=c; }
static void emits(const char*s){ while(*s)oemit(*s++); }
static void emitint(i32 n){ if(n<0){oemit('-');n=-n;} char tmp[12];int k=0;
  if(n==0){oemit('0');return;} while(n){tmp[k++]='0'+(n%10);n/=10;} while(k)oemit(tmp[--k]); }
static void print_val(u32 v){
  if(is_fix(v)){emitint(fixval(v));return;}
  if(v==NIL){emits("()");return;} if(v==TRUE){emits("t");return;}
  if(v==ERR||v==UNBOUND){emits("<error>");return;}
  if(is_prim(v)){emits("<primitive>");return;}
  if(is_sym(v)){u32 i=symidx(v);for(u32 k=0;k<symlen[i];k++)oemit(symname[i][k]);return;}
  if(is_closure(v)){emits("<lambda>");return;}
  if(is_cons(v)){ oemit('('); u32 p=v;int first=1;
    while(is_cons(p)){if(!first)oemit(' ');first=0;print_val(car(p));p=cdr(p);}
    if(!is_nil(p)){emits(" . ");print_val(p);} oemit(')'); }
}

// ---- exports ---------------------------------------------------------------
#define INCAP 8192
char inbuf[INCAP];
__attribute__((export_name("input_ptr"))) char* input_ptr(){ return inbuf; }
__attribute__((export_name("output_ptr"))) char* output_ptr(){ return outbuf; }
__attribute__((export_name("gc_count"))) unsigned gc_count(){ return g_numgc; }
__attribute__((export_name("eval_source")))
u32 eval_source(u32 len){
  init();
  rp=inbuf; rend=inbuf+(len<INCAP?len:INCAP);
  outlen=0;
  // Compile all top-level forms into one stream: f1 POP f2 POP ... fN HALT
  u32 first=1; int any=0;
  for(;;){
    skipws(); if(rp>=rend) break;
    u32 form=read_expr();
    if(form==ERR){ if(!any){ print_val(ERR); return outlen; } break; }
    if(!first) emit(OP_POP);
    first=0; any=1;
    compile(form,NIL,0);
    if(g_oom||g_cerr){ print_val(ERR); return outlen; }
  }
  if(!any){ print_val(NIL); return outlen; }
  emit(OP_HALT);
#ifdef DISASM_ONLY
  // Build with -DDISASM_ONLY -DOUTCAP=262144 to dump the compiled bytecode
  // instead of executing. Used by harness/disasm.sh + harness/disasm.mjs to
  // inspect what the compiler emits for a given source program. See
  // docs/project_notes/bytecode_disasm.md and FINDINGS.md "Bytecode shape".
  u32 p=0;
  while(p<cp){
    u32 addr=p;
    if(addr<10){oemit(' ');oemit(' ');oemit(' ');}
    else if(addr<100){oemit(' ');oemit(' ');}
    else if(addr<1000){oemit(' ');}
    emitint((i32)addr); emits(": ");
    u32 op=code[p++];
    switch(op){
      case OP_CONST:    emits("CONST    "); print_val(code[p++]); break;
      case OP_LOADL:    emits("LOADL    d="); emitint(code[p++]); emits(" i="); emitint(code[p++]); break;
      case OP_LOADG:    emits("LOADG    "); { u32 i=code[p++]; for(u32 k=0;k<symlen[i];k++)oemit(symname[i][k]); } break;
      case OP_DEFG:     emits("DEFG     "); { u32 i=code[p++]; for(u32 k=0;k<symlen[i];k++)oemit(symname[i][k]); } break;
      case OP_SETL:     emits("SETL     d="); emitint(code[p++]); emits(" i="); emitint(code[p++]); break;
      case OP_SETG:     emits("SETG     "); { u32 i=code[p++]; for(u32 k=0;k<symlen[i];k++)oemit(symname[i][k]); } break;
      case OP_POP:      emits("POP"); break;
      case OP_JMP:      emits("JMP      -> "); emitint(code[p++]); break;
      case OP_JFALSE:   emits("JFALSE   -> "); emitint(code[p++]); break;
      case OP_CLOSURE:  emits("CLOSURE  body="); emitint(code[p++]); emits(" nparams="); emitint(code[p++]); break;
      case OP_CALL:     emits("CALL     n="); emitint(code[p++]); break;
      case OP_TAILCALL: emits("TAILCALL n="); emitint(code[p++]); break;
      case OP_RET:      emits("RET"); break;
      case OP_HALT:     emits("HALT"); break;
      default:          emits("???      "); emitint((i32)op); break;
    }
    oemit('\n');
  }
  emits("\n; --- bytecode total: "); emitint((i32)cp); emits(" words ---\n");
  return outlen;
#else
  gc_enabled=1;            // compile done; collect during execution
  u32 result=run(0);
  print_val(result);
  return outlen;
#endif
}
