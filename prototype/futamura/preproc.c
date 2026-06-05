// preproc.c — source-to-source PE for wallisp with $-marked comptime.
//
// Reads Lisp source on stdin, writes (rewritten) Lisp source on stdout.
// Forms prefixed with `$` are evaluated at preprocess time and replaced with
// their resulting literal value. HARD-FAIL design: if a $-form can't be
// folded (unbound symbol, fuel exhausted, unsupported construct), preproc
// prints a diagnostic to stderr and exits non-zero. There is no soft
// fall-through to runtime.
//
//   ./preproc < input.lisp > residual.lisp
//   ./preproc --trace < input.lisp > residual.lisp     (logs each fold to stderr)
//
// Scope of v1:
//   * `$expr` may appear anywhere a value can appear.
//   * Top-level `(define NAME VALUE)` and `(define (NAME ARGS...) BODY)` are
//     collected into a defines table; their bodies are available to the
//     folder for recursive evaluation.
//   * Foldable forms: integer literals, parameter refs, `if`,
//     `+ - * < > = mod`, named function calls into the defines table.
//   * NOT supported (hard-fails inside a $-form): cons/car/cdr, strings,
//     lambda-as-value, let, cond, begin, set!, mutation.
//   * Output is plain Lisp; the engines (`lisp.wasm`, `bytecode_gc.wasm`,
//     etc.) consume it unchanged.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef unsigned int u32;
typedef int          i32;

/* ---- value rep (matches engines/lisp.c) ----------------------------------*/
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

#define MAX_CELLS  524288
#define MAX_SYMS   2048
#define SYM_CHARS  32
typedef struct { u32 car, cdr; } Cell;
static Cell cells[MAX_CELLS];
static u32  cell_top = 0;
static char symname[MAX_SYMS][SYM_CHARS];
static u32  symlen[MAX_SYMS];
static u32  sym_top = 0;
static const char *rp, *rend;

static u32 cons(u32 a, u32 d){
  if(cell_top >= MAX_CELLS){ fprintf(stderr, "preproc: cells exhausted\n"); exit(1); }
  u32 i = cell_top++;
  cells[i].car = a; cells[i].cdr = d;
  return mkcons(i);
}
static u32 car(u32 v){ return is_cons(v) ? cells[considx(v)].car : NIL; }
static u32 cdr(u32 v){ return is_cons(v) ? cells[considx(v)].cdr : NIL; }

static u32 intern(const char* s, u32 len){
  if(len > SYM_CHARS) len = SYM_CHARS;
  for(u32 i = 0; i < sym_top; i++)
    if(symlen[i] == len && memcmp(symname[i], s, len) == 0) return mksym(i);
  if(sym_top >= MAX_SYMS){ fprintf(stderr, "preproc: symbol table full\n"); exit(1); }
  u32 i = sym_top++;
  memcpy(symname[i], s, len);
  symlen[i] = len;
  return mksym(i);
}
static u32 intern_cstr(const char *s){ return intern(s, (u32)strlen(s)); }
static int sym_eq_str(u32 v, const char *s){
  if(!is_sym(v)) return 0;
  u32 i = symidx(v), n = (u32)strlen(s);
  return symlen[i] == n && memcmp(symname[i], s, n) == 0;
}

/* ---- reader (adds `$E` => `(__comptime__ E)` and `'E` => `(quote E)`) ----*/
static void skipws(void){
  while(rp < rend){
    char c = *rp;
    if(c==' '||c=='\t'||c=='\n'||c=='\r'){ rp++; }
    else if(c==';'){ while(rp<rend && *rp!='\n') rp++; }
    else break;
  }
}
static int is_delim(char c){
  return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='('||c==')'||c==';'||c==0||c=='$';
}
static u32 read_expr(void);
static u32 read_list(void){
  u32 first = NIL, last = NIL;
  for(;;){
    skipws();
    if(rp >= rend){ fprintf(stderr, "preproc: unterminated list\n"); exit(1); }
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
  if(len == 0){ fprintf(stderr, "preproc: unexpected char '%c'\n", *rp); exit(1); }
  int neg = 0; const char* p = s; u32 n = len;
  if(n > 0 && (*p == '-' || *p == '+')){ neg = (*p == '-'); p++; n--; }
  if(n > 0){
    int isnum = 1; i32 v = 0;
    for(u32 i = 0; i < n; i++){
      char c = p[i];
      if(c < '0' || c > '9'){ isnum = 0; break; }
      v = v*10 + (c - '0');
    }
    if(isnum) return mkfix(neg ? -v : v);
  }
  return intern(s, len);
}
static u32 read_expr(void){
  skipws();
  if(rp >= rend){ fprintf(stderr, "preproc: unexpected EOF\n"); exit(1); }
  if(*rp == '('){ rp++; return read_list(); }
  if(*rp == '\''){ rp++; u32 e = read_expr();
                   return cons(intern_cstr("quote"), cons(e, NIL)); }
  if(*rp == '$'){ rp++; u32 e = read_expr();
                  return cons(intern_cstr("__comptime__"), cons(e, NIL)); }
  return read_atom();
}

/* ---- defines table -------------------------------------------------------*/
typedef struct {
  u32 name;
  u32 params;        // proper list of param symbols
  u32 body;          // expression
} Define;
static Define defines[256];
static int    ndefines = 0;

static Define* find_define(u32 sym){
  for(int i = 0; i < ndefines; i++)
    if(defines[i].name == sym) return &defines[i];
  return NULL;
}

/* ---- in-host evaluator for $-folded expressions --------------------------*/
// Returns 1 on success (writing result to *out), 0 on hard-fail (after
// printing a diagnostic to stderr). Tracks fuel; bails after FUEL_INIT
// reductions to bound compile-time work.
#define FUEL_INIT 200000

static int trace = 0;  // --trace flag: log each $-fold to stderr

static int eval_ct(u32 expr, u32 env, int *fuel, u32 *out);

static int env_lookup(u32 env, u32 sym, u32 *out){
  for(u32 f = env; is_cons(f); f = cdr(f)){
    u32 b = car(f);
    if(is_cons(b) && car(b) == sym){ *out = cdr(b); return 1; }
  }
  return 0;
}

static int eval_binop(u32 head, u32 a, u32 b, u32 *out){
  if(!is_fix(a) || !is_fix(b)){
    fprintf(stderr, "preproc: $-fold: non-integer operand to ");
    fwrite(symname[symidx(head)], 1, symlen[symidx(head)], stderr);
    fputc('\n', stderr);
    return 0;
  }
  i32 x = fixval(a), y = fixval(b);
  if(sym_eq_str(head, "+"))   { *out = mkfix(x + y); return 1; }
  if(sym_eq_str(head, "-"))   { *out = mkfix(x - y); return 1; }
  if(sym_eq_str(head, "*"))   { *out = mkfix(x * y); return 1; }
  if(sym_eq_str(head, "<"))   { *out = mkfix(x < y); return 1; }
  if(sym_eq_str(head, ">"))   { *out = mkfix(x > y); return 1; }
  if(sym_eq_str(head, "="))   { *out = mkfix(x == y); return 1; }
  if(sym_eq_str(head, "mod")) {
    if(y == 0){ fprintf(stderr, "preproc: $-fold: mod by zero\n"); return 0; }
    *out = mkfix(x % y); return 1;
  }
  return -1;  // not a known binop; caller falls through to function-call path
}

static int eval_ct(u32 expr, u32 env, int *fuel, u32 *out){
  if(--(*fuel) < 0){
    fprintf(stderr, "preproc: $-fold: fuel exhausted (loop or deep recursion?)\n");
    return 0;
  }
  if(is_fix(expr)){ *out = expr; return 1; }
  if(is_sym(expr)){
    if(env_lookup(env, expr, out)) return 1;
    // Maybe a nullary define? Allow `(define x EXPR)` to be looked up by name.
    Define *d = find_define(expr);
    if(d && is_nil(d->params)) return eval_ct(d->body, NIL, fuel, out);
    fprintf(stderr, "preproc: $-fold: unbound symbol `");
    fwrite(symname[symidx(expr)], 1, symlen[symidx(expr)], stderr);
    fputs("`\n", stderr);
    return 0;
  }
  if(!is_cons(expr)){
    fprintf(stderr, "preproc: $-fold: unsupported atom shape\n");
    return 0;
  }
  u32 head = car(expr), args = cdr(expr);

  // if-special-form
  if(sym_eq_str(head, "if")){
    u32 c;
    if(!eval_ct(car(args), env, fuel, &c)) return 0;
    if(fixval(c)) return eval_ct(car(cdr(args)), env, fuel, out);
    u32 elsep = cdr(cdr(args));
    if(is_nil(elsep)){ *out = NIL; return 1; }   // (if c t) with c=false
    return eval_ct(car(elsep), env, fuel, out);
  }

  if(!is_sym(head)){
    fprintf(stderr, "preproc: $-fold: head of call is not a symbol\n");
    return 0;
  }

  // Try binop fast path.
  if(is_cons(args) && is_cons(cdr(args)) && is_nil(cdr(cdr(args)))){
    u32 a, b;
    if(!eval_ct(car(args), env, fuel, &a)) return 0;
    if(!eval_ct(car(cdr(args)), env, fuel, &b)) return 0;
    int r = eval_binop(head, a, b, out);
    if(r >= 0) return r;
    // not a binop — fall through to function call with these args (re-bind)
    // Build a new env with already-evaluated args bound to the function's params.
    Define *d = find_define(head);
    if(!d){
      fprintf(stderr, "preproc: $-fold: unknown function `");
      fwrite(symname[symidx(head)], 1, symlen[symidx(head)], stderr);
      fputs("`\n", stderr);
      return 0;
    }
    u32 vals[2] = { a, b };
    u32 newenv = NIL; int i = 0;
    for(u32 p = d->params; is_cons(p); p = cdr(p)){
      if(i >= 2){ fprintf(stderr, "preproc: arity mismatch on `");
                  fwrite(symname[symidx(head)], 1, symlen[symidx(head)], stderr);
                  fputs("`\n", stderr); return 0; }
      newenv = cons(cons(car(p), vals[i++]), newenv);
    }
    return eval_ct(d->body, newenv, fuel, out);
  }

  // General named-function call path (arity != 2).
  Define *d = find_define(head);
  if(!d){
    fprintf(stderr, "preproc: $-fold: unknown function `");
    fwrite(symname[symidx(head)], 1, symlen[symidx(head)], stderr);
    fputs("`\n", stderr);
    return 0;
  }
  u32 vals[8]; int n = 0;
  for(u32 p = args; is_cons(p); p = cdr(p)){
    if(n >= 8){ fprintf(stderr, "preproc: $-fold: too many args (max 8)\n"); return 0; }
    if(!eval_ct(car(p), env, fuel, &vals[n++])) return 0;
  }
  u32 newenv = NIL; int i = 0;
  for(u32 p = d->params; is_cons(p); p = cdr(p)){
    if(i >= n){ fprintf(stderr, "preproc: arity mismatch on `");
                fwrite(symname[symidx(head)], 1, symlen[symidx(head)], stderr);
                fputs("`\n", stderr); return 0; }
    newenv = cons(cons(car(p), vals[i++]), newenv);
  }
  return eval_ct(d->body, newenv, fuel, out);
}

/* ---- tree rewrite (replace `(__comptime__ X)` with eval result) ----------*/
static void print_val(FILE *out, u32 v);  // forward decl — used by trace output

static u32 rewrite(u32 expr){
  if(!is_cons(expr)) return expr;
  if(sym_eq_str(car(expr), "__comptime__")){
    u32 inner = car(cdr(expr));
    inner = rewrite(inner);  // nested $s fold first
    int fuel = FUEL_INIT;
    u32 result;
    if(!eval_ct(inner, NIL, &fuel, &result)){
      exit(1);  // diagnostic already printed
    }
    if(trace){
      fputc('$', stderr);
      print_val(stderr, inner);
      fputs(" => ", stderr);
      print_val(stderr, result);
      fputc('\n', stderr);
    }
    return result;
  }
  // Otherwise: rebuild the list with each element rewritten.
  u32 first = NIL, last = NIL;
  for(u32 p = expr; is_cons(p); p = cdr(p)){
    u32 link = cons(rewrite(car(p)), NIL);
    if(is_nil(first)){ first = link; last = link; }
    else { cells[considx(last)].cdr = link; last = link; }
  }
  return first;
}

/* ---- printer (emits wallisp source) --------------------------------------*/
static void print_int(FILE *out, i32 n){ fprintf(out, "%d", n); }

static void print_val(FILE *out, u32 v){
  if(is_fix(v)){ print_int(out, fixval(v)); return; }
  if(v == NIL){ fputs("()", out); return; }
  if(is_sym(v)){
    u32 i = symidx(v);
    fwrite(symname[i], 1, symlen[i], out);
    return;
  }
  if(is_cons(v)){
    fputc('(', out);
    u32 p = v; int first = 1;
    while(is_cons(p)){
      if(!first) fputc(' ', out);
      first = 0;
      print_val(out, car(p));
      p = cdr(p);
    }
    if(!is_nil(p)){ fputs(" . ", out); print_val(out, p); }
    fputc(')', out);
    return;
  }
  fputs("<?>", out);
}

/* ---- top-level driver ----------------------------------------------------*/
static void register_define(u32 form){
  // form is (define HEAD REST) where HEAD is either NAME (for `(define NAME EXPR)`)
  // or (NAME ARGS...) for `(define (NAME ARGS...) BODY)`.
  u32 head = car(cdr(form));
  u32 rest = cdr(cdr(form));
  Define *d = &defines[ndefines++];
  if(is_sym(head)){
    d->name = head;
    d->params = NIL;
    d->body = car(rest);
  } else if(is_cons(head)){
    d->name = car(head);
    d->params = cdr(head);
    d->body = car(rest);
  } else {
    fprintf(stderr, "preproc: malformed define\n");
    exit(1);
  }
}

int main(int argc, char **argv){
  for(int i = 1; i < argc; i++){
    if(strcmp(argv[i], "--trace") == 0) trace = 1;
    else { fprintf(stderr, "preproc: unknown arg `%s`\n", argv[i]); return 2; }
  }
  static char buf[1 << 22];
  size_t got = fread(buf, 1, sizeof buf, stdin);
  rp = buf; rend = buf + got;

  for(;;){
    skipws();
    if(rp >= rend) break;
    u32 form = read_expr();

    // Rewrite the form (folds any $s within it, using already-registered defines).
    u32 rewritten = rewrite(form);

    // If it's a define, register it AFTER rewriting so its body is the folded one.
    if(is_cons(rewritten) && sym_eq_str(car(rewritten), "define")){
      register_define(rewritten);
    }

    print_val(stdout, rewritten);
    fputc('\n', stdout);
  }
  return 0;
}
