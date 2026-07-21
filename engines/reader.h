// reader.h — shared s-expression reader for the freestanding wasm engines.
//
// Hand-rolled recursive descent over a source buffer. Supports ints (with
// leading '-'), symbols, lists, and 'quote shorthand. Good enough; no strings,
// no dotted pairs, no #t literals (engines expose nil/t as symbols bound in env).
//
// Engine integration: each .c includes this header AFTER declaring its
//   cons(u32,u32) -> u32         // arena cons; returns ERR on OOM
//   intern(const char*, u32) -> u32  // symbol interning
//   mkfix(i32) -> u32            // fixnum constructor
//   NIL, ERR                     // tagged singleton constants
//   s_quote                      // interned 'quote symbol
//   cells[].cdr                  // cons-arena array used to splice list tails
//   considx(v) -> u32, is_nil(v) -> int, is_cons() unused here
//
// Engines that want extra dispatch cases (e.g. bytecode_gc's "..." strings)
// define READER_HAS_STRINGS before the include and provide a `static u32
// read_string()` body in the engine source. See engines/bytecode_gc.c.

static const char* rp;
static const char* rend;

// Recursion-depth guard for the reader. read_expr/read_list/read_string recurse
// per nesting level; without a cap, deeply nested input (e.g. tens of thousands
// of '(') overflows the C stack and traps the module instead of returning an
// error. 200 is far beyond any hand-written nesting and safe for the default
// wasm stack. The counter is balanced (inc on entry, dec on every exit), so it
// returns to 0 between top-level forms without an explicit reset.
#define READ_MAX_DEPTH 200
static int r_depth = 0;

static void skipws(){
  while(rp<rend){
    char c=*rp;
    if(c==' '||c=='\t'||c=='\n'||c=='\r'||c==','){rp++;}
    else if(c==';'){ while(rp<rend && *rp!='\n') rp++; }
    else break;
  }
}
static int is_delim(char c){
  return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='('||c==')'||c==','||c==';'||c==0;
}
static u32 read_expr();
static u32 read_expr_body();

static u32 read_list(){
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
}
static u32 read_atom(){
  const char* start=rp;
  while(rp<rend && !is_delim(*rp)) rp++;
  u32 len=(u32)(rp-start);
  int neg=0; const char* p=start; u32 n=len;
  if(n>0 && (*p=='-'||*p=='+')){ neg=(*p=='-'); p++; n--; }
  if(n>0){
    // Fixnums are 30-bit signed (2-bit tag in mkfix). Accumulate in i64 and
    // saturate past the range so a long digit string can't overflow the
    // accumulator (UB) or silently wrap into a wrong fixnum. An out-of-range
    // numeric literal is an error, matching the arithmetic prims' fits_fix.
    #define READER_FIX_MAX  536870911LL     //  0x1FFFFFFF
    #define READER_FIX_MIN  (-536870912LL)  // -0x20000000
    int isnum=1; long long val=0;
    for(u32 i=0;i<n;i++){
      char c=p[i]; if(c<'0'||c>'9'){isnum=0;break;}
      val=val*10+(c-'0');
      if(val > 1073741824LL) val = 1073741824LL;   // clamp: still > FIX_MAX, so rejected below
    }
    if(isnum){
      long long sv = neg ? -val : val;
      if(sv < READER_FIX_MIN || sv > READER_FIX_MAX) return ERR;
      return mkfix((i32)sv);
    }
  }
  return intern(start,len);
}

#ifdef READER_HAS_STRINGS
static u32 read_string();  // engine-provided; see e.g. bytecode_gc.c
#endif

static u32 read_expr(){
  if(++r_depth > READ_MAX_DEPTH){ --r_depth; return ERR; }
  u32 r = read_expr_body();
  --r_depth;
  return r;
}
static u32 read_expr_body(){
  skipws();
  if(rp>=rend) return ERR;
  char c=*rp;
  if(c=='('){ rp++; return read_list(); }
  if(c=='\''){ rp++; u32 e=read_expr(); return cons(s_quote,cons(e,NIL)); }
#ifdef READER_HAS_STRINGS
  if(c=='"'){ return read_string(); }
#endif
  u32 a=read_atom();
  // f-call sugar: atom immediately followed by '(' with no whitespace
  // reads as (atom args...). `fn (a b)` still parses as two siblings.
  if(rp<rend && *rp=='('){
    rp++;
    u32 args=read_list();
    return cons(a,args);
  }
  return a;
}
