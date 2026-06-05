/* comptime.c — a minimal comptime specializer (partial evaluator) in C.
 *
 * Demonstrates the "eval-or-emit" core: one recursive walk over a source AST
 * that (a) FOLDS comptime-known subexpressions into values (including first-class
 * Type values), and (b) EMITS residual IR for runtime subexpressions. Comptime
 * trip counts unroll; a first-class Type value selects the i32/f32 op variant;
 * a monomorphization cache dedupes identical instantiations.
 *
 * This pass runs in the COMPILER (host), so it freely uses libc. Only the
 * RESIDUAL IR it produces is meant to lower to freestanding wasm.
 *
 *   cc -std=c11 -Wall -Wextra -O0 comptime.c -o comptime && ./comptime
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* ---- Object-language types are first-class *comptime values* ------------- */
typedef enum { TY_I32, TY_F32 } Type;
static const char *type_name(Type t) { return t == TY_I32 ? "i32" : "f32"; }

/* ---- Source AST ---------------------------------------------------------- */
typedef enum { A_INT, A_TYPE, A_PARAM, A_LOAD, A_TBIN, A_REPEAT } AKind;
typedef enum { OP_ADD, OP_MUL } BinOp;

typedef struct ANode ANode;
struct ANode {
    AKind k;
    int64_t ival;            /* A_INT  */
    Type    tval;            /* A_TYPE */
    int     slot;            /* A_PARAM: env index */
    ANode  *base, *index;    /* A_LOAD: base is runtime, index must be comptime */
    BinOp   op;              /* A_TBIN */
    ANode  *type_of, *lhs, *rhs;            /* A_TBIN: type_of picks i/f variant */
    ANode  *count, *init, *body;            /* A_REPEAT: comptime count -> unroll */
    int     acc_slot, idx_slot;             /* A_REPEAT: loop-carried acc + index */
};
static ANode *mk(AKind k) { ANode *n = calloc(1, sizeof *n); n->k = k; return n; }
static ANode *Int(int64_t v){ ANode*n=mk(A_INT);  n->ival=v; return n; }
static ANode *Par(int s)    { ANode*n=mk(A_PARAM);n->slot=s; return n; }
static ANode *Load(ANode*b,ANode*i){ ANode*n=mk(A_LOAD); n->base=b; n->index=i; return n; }
static ANode *TBin(BinOp o,ANode*ty,ANode*a,ANode*b){
    ANode*n=mk(A_TBIN); n->op=o; n->type_of=ty; n->lhs=a; n->rhs=b; return n; }
static ANode *Repeat(ANode*cnt,int acc,ANode*init,int idx,ANode*body){
    ANode*n=mk(A_REPEAT); n->count=cnt; n->acc_slot=acc; n->init=init;
    n->idx_slot=idx; n->body=body; return n; }

/* ---- Specialization values: a comptime value OR a residual IR temp ------- */
typedef enum { SV_INT, SV_TYPE, SV_RES } SVKind;
typedef struct { SVKind k; int64_t i; Type t; int temp; } SVal;
static SVal CInt(int64_t v){ return (SVal){SV_INT,v,0,0}; }
static SVal CTy (Type t)  { return (SVal){SV_TYPE,0,t,0}; }
static SVal Res (int tmp) { return (SVal){SV_RES,0,0,tmp}; }

/* ---- Residual IR --------------------------------------------------------- */
typedef enum { IR_CONST, IR_LOAD, IR_MUL_I, IR_MUL_F, IR_ADD_I, IR_ADD_F, IR_RET } IROp;
typedef struct { IROp op; int dst, a, b; int64_t imm; } IRInst;

typedef struct {
    char    name[80];
    int     nparams;          /* runtime params occupy the first temps */
    IRInst  code[512];
    int     ncode, ntemps;
} IRFunc;

static int emit(IRFunc *f, IROp op, int a, int b, int64_t imm) {
    int dst = f->ntemps++;
    f->code[f->ncode++] = (IRInst){op, dst, a, b, imm};
    return dst;
}
/* Force an SVal into a runtime temp (materialize a comptime constant if needed). */
static int as_temp(IRFunc *f, SVal v) {
    if (v.k == SV_RES) return v.temp;
    if (v.k == SV_INT) return emit(f, IR_CONST, 0, 0, v.i);
    fprintf(stderr, "comptime ERROR: a Type value escaped into runtime code\n");
    exit(1);
}

static void barrier(bool ok, const char *what) {
    if (!ok) { fprintf(stderr, "comptime ERROR: %s must be comptime-known\n", what); exit(1); }
}

/* ---- The eval-or-emit core ---------------------------------------------- */
static SVal spec(ANode *n, SVal *env, IRFunc *f) {
    switch (n->k) {
    case A_INT:   return CInt(n->ival);
    case A_TYPE:  return CTy(n->tval);
    case A_PARAM: return env[n->slot];

    case A_LOAD: {                          /* a[i] : runtime base, comptime index */
        SVal base = spec(n->base, env, f);
        SVal idx  = spec(n->index, env, f);
        barrier(base.k == SV_RES,  "load base");      /* base is a runtime arg     */
        barrier(idx.k  == SV_INT,  "load index");     /* offset baked in at compile*/
        return Res(emit(f, IR_LOAD, base.temp, 0, idx.i));
    }

    case A_TBIN: {
        SVal ty = spec(n->type_of, env, f);
        barrier(ty.k == SV_TYPE, "op type");
        SVal a = spec(n->lhs, env, f), b = spec(n->rhs, env, f);

        /* Both operands comptime -> fold entirely away (only model integers). */
        if (a.k == SV_INT && b.k == SV_INT && ty.t == TY_I32)
            return CInt(n->op == OP_ADD ? a.i + b.i : a.i * b.i);

        /* Algebraic identities a comptime-known operand unlocks. */
        if (n->op == OP_ADD) {
            if (a.k == SV_INT && a.i == 0) return b;   /* 0 + x = x  (drop the add) */
            if (b.k == SV_INT && b.i == 0) return a;
        } else { /* OP_MUL */
            if ((a.k == SV_INT && a.i == 0) || (b.k == SV_INT && b.i == 0)) return CInt(0);
            if (a.k == SV_INT && a.i == 1) return b;
            if (b.k == SV_INT && b.i == 1) return a;
        }

        int ta = as_temp(f, a), tb = as_temp(f, b);
        IROp op = (n->op == OP_ADD)
                    ? (ty.t == TY_I32 ? IR_ADD_I : IR_ADD_F)
                    : (ty.t == TY_I32 ? IR_MUL_I : IR_MUL_F);
        return Res(emit(f, op, ta, tb, 0));
    }

    case A_REPEAT: {                        /* comptime count -> fully unrolled */
        SVal c = spec(n->count, env, f);
        barrier(c.k == SV_INT, "repeat count");
        SVal acc = spec(n->init, env, f);
        for (int64_t k = 0; k < c.i; ++k) {
            env[n->idx_slot] = CInt(k);
            env[n->acc_slot] = acc;
            acc = spec(n->body, env, f);
        }
        return acc;
    }
    }
    fprintf(stderr, "unreachable\n"); exit(1);
}

/* ---- Monomorphization: instantiate a generic fn, cache by comptime args -- */
typedef struct { Type T; int n; int func; } CacheEnt;
static IRFunc g_funcs[64]; static int g_nfuncs = 0;
static CacheEnt g_cache[64]; static int g_ncache = 0;
static int g_hits = 0;

/* dot(T:comptime, n:comptime, a, b) = sum_{k<n} a[k]*b[k]  in accumulator type T */
static int instantiate_dot(ANode *body, Type T, int n) {
    for (int i = 0; i < g_ncache; ++i)              /* cache lookup by (T,n) */
        if (g_cache[i].T == T && g_cache[i].n == n) { g_hits++; return g_cache[i].func; }

    int fi = g_nfuncs++;
    IRFunc *f = &g_funcs[fi];
    memset(f, 0, sizeof *f);
    snprintf(f->name, sizeof f->name, "dot$%s$n%d", type_name(T), n);
    f->nparams = 2; f->ntemps = 2;                  /* temps 0,1 = runtime args a,b */

    SVal env[6];
    env[0] = CTy(T);            /* slot0: T  (comptime type)  */
    env[1] = CInt(n);           /* slot1: n  (comptime count) */
    env[2] = Res(0);            /* slot2: a  (runtime arg)    */
    env[3] = Res(1);            /* slot3: b  (runtime arg)    */
    env[4] = CInt(0);           /* slot4: acc (scratch)       */
    env[5] = CInt(0);           /* slot5: k   (scratch)       */

    SVal out = spec(body, env, f);
    emit(f, IR_RET, as_temp(f, out), 0, 0);

    g_cache[g_ncache++] = (CacheEnt){T, n, fi};
    return fi;
}

/* ---- Pretty-print residual IR ------------------------------------------- */
static const char *opname(IROp o) {
    switch (o){case IR_CONST:return "const";case IR_LOAD:return "load";
    case IR_MUL_I:return "mul.i";case IR_MUL_F:return "mul.f";
    case IR_ADD_I:return "add.i";case IR_ADD_F:return "add.f";case IR_RET:return "ret";}
    return "?";
}
static void tname(char *buf, int t) {           /* show the two params as a,b */
    if (t == 0) strcpy(buf, "a"); else if (t == 1) strcpy(buf, "b");
    else sprintf(buf, "%%t%d", t);
}
static void print_func(IRFunc *f) {
    printf("func %s(a, b):\n", f->name);
    char x[16], y[16];
    for (int i = 0; i < f->ncode; ++i) {
        IRInst *c = &f->code[i];
        switch (c->op) {
        case IR_CONST: printf("  %%t%d = const %lld\n", c->dst, (long long)c->imm); break;
        case IR_LOAD:  tname(x, c->a); printf("  %%t%d = load  %s, +%lld\n", c->dst, x, (long long)c->imm); break;
        case IR_RET:   tname(x, c->a); printf("  ret %s\n", x); break;
        default:       tname(x, c->a); tname(y, c->b);
                       printf("  %%t%d = %s %s, %s\n", c->dst, opname(c->op), x, y); break;
        }
    }
    printf("  ; %d instructions\n\n", f->ncode);
}

int main(void) {
    /* Build the generic `dot` body ONCE (the AST stays a private compiler type). */
    ANode *mul  = TBin(OP_MUL, Par(0), Load(Par(2), Par(5)), Load(Par(3), Par(5)));
    ANode *step = TBin(OP_ADD, Par(0), Par(4), mul);          /* acc + a[k]*b[k] */
    ANode *body = Repeat(Par(1), /*acc*/4, Int(0), /*k*/5, step);

    struct { Type T; int n; } reqs[] = {
        {TY_I32, 4}, {TY_F32, 3}, {TY_I32, 4} /* <- cache hit */, {TY_I32, 8},
    };
    printf("=== instantiating dot for each (type, n) ===\n\n");
    for (size_t i = 0; i < sizeof reqs / sizeof reqs[0]; ++i) {
        int fi = instantiate_dot(body, reqs[i].T, reqs[i].n);
        print_func(&g_funcs[fi]);
    }
    printf("emitted %d distinct functions; %d cache hit(s)\n", g_nfuncs, g_hits);
    return 0;
}
