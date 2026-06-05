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
  const char *name;
  u32 params[8];   // interned symbol values
  int arity;
  u32 body;        // AST node (cons or atom)
} Fn;
static Fn fns[32];
static int nfns = 0;

static const char* name_of(u32 sym){
  // dup into a fresh malloc so it survives later symstr() calls.
  u32 i = symidx(sym);
  char *s = malloc(symlen[i] + 1);
  memcpy(s, symname[i], symlen[i]); s[symlen[i]] = 0;
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

// Emit one expression, parenthesized.
static void emit_expr(FILE *out, u32 node);

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
    // Parameter or other top-level function reference — emit the name as-is.
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

  // Named function call.
  if(is_sym(head)){
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

  fprintf(stderr, "specialize: unsupported expression head\n");
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

  // Forward declarations for all recorded functions (lets them call each other).
  for(int i = 0; i < nfns; i++){
    fprintf(out, "static u32 %s(", fns[i].name);
    for(int j = 0; j < fns[i].arity; j++){
      if(j) fputs(", ", out);
      fputs("u32 ", out);
    }
    fputs(");\n", out);
  }
  fputc('\n', out);

  // Function definitions.
  for(int i = 0; i < nfns; i++){
    Fn *f = &fns[i];
    fprintf(out, "static u32 %s(", f->name);
    for(int j = 0; j < f->arity; j++){
      if(j) fputs(", ", out);
      fprintf(out, "u32 %s", symstr(f->params[j]));
    }
    fputs("){\n  return ", out);
    emit_expr(out, f->body);
    fputs(";\n}\n\n", out);
  }

  // ABI shim. Entry point = last recorded function. Parse N decimal ints.
  Fn *entry = &fns[nfns - 1];
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
