// residual_fib_untagged.c — Futamura projection #1, with type-flow analysis.
//
// What a SMART partial evaluator would emit when binding-time analysis tracks
// not just dispatch but the value rep too: type-flow proves fib's argument
// and result are always fixnums, so it specializes the rep — operates on raw
// i32 inside fib, taggs only at the entry/exit boundary.
//
// In the limit, this is just "fib in C compiled to wasm" — i.e. the same
// thing as native_bench_baseline but going through V8. The gap between this
// and the tagged residual is the "rep specialization headroom."
//
// Public ABI matches the engines: input_ptr/output_ptr/eval_source(len).

typedef unsigned int  u32;
typedef int           i32;

static i32 fib(i32 n){
  if(n<2) return n;
  return fib(n-1) + fib(n-2);
}

#define INCAP  8192
#define OUTCAP 4096
static char inbuf[INCAP];
static char outbuf[OUTCAP];
static u32  outlen;

static void emit(char c){ if(outlen<OUTCAP) outbuf[outlen++]=c; }
static void emitint(i32 n){
  if(n<0){ emit('-'); n=-n; }
  char tmp[12]; int k=0;
  if(n==0){ emit('0'); return; }
  while(n){ tmp[k++]='0'+(n%10); n/=10; }
  while(k) emit(tmp[--k]);
}

__attribute__((export_name("input_ptr")))  char* input_ptr(void){ return inbuf; }
__attribute__((export_name("output_ptr"))) char* output_ptr(void){ return outbuf; }
__attribute__((export_name("eval_source")))
u32 eval_source(u32 len){
  i32 n=0; int neg=0; u32 i=0;
  while(i<len && (inbuf[i]==' '||inbuf[i]=='\n'||inbuf[i]=='\t'||inbuf[i]=='\r')) i++;
  if(i<len && inbuf[i]=='-'){ neg=1; i++; }
  while(i<len && inbuf[i]>='0' && inbuf[i]<='9'){ n=n*10+(inbuf[i]-'0'); i++; }
  if(neg) n=-n;
  i32 r = fib(n);
  outlen=0;
  emitint(r);
  return outlen;
}
