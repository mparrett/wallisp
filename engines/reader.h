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
    int isnum=1; i32 val=0;
    for(u32 i=0;i<n;i++){ char c=p[i]; if(c<'0'||c>'9'){isnum=0;break;} val=val*10+(c-'0'); }
    if(isnum) return mkfix(neg?-val:val);
  }
  return intern(start,len);
}

#ifdef READER_HAS_STRINGS
static u32 read_string();  // engine-provided; see e.g. bytecode_gc.c
#endif

static u32 read_expr(){
  skipws();
  if(rp>=rend) return ERR;
  char c=*rp;
  if(c=='('){ rp++; return read_list(); }
  if(c=='\''){ rp++; u32 e=read_expr(); return cons(s_quote,cons(e,NIL)); }
#ifdef READER_HAS_STRINGS
  if(c=='"'){ return read_string(); }
#endif
  return read_atom();
}
