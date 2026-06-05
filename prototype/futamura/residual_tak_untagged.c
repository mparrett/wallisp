// residual_tak_untagged.c — PE residual for tak with type-flow specialization.
// Raw i32 throughout; tag only at the I/O boundary (here, even that drops out).

typedef unsigned int u32;
typedef int          i32;

static i32 tak(i32 x, i32 y, i32 z){
  if(y < x){
    return tak(tak(x-1, y, z), tak(y-1, z, x), tak(z-1, x, y));
  }
  return z;
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
static i32 parse_int(u32 len, u32* i_io){
  u32 i = *i_io;
  while(i<len && (inbuf[i]==' '||inbuf[i]=='\n'||inbuf[i]=='\t'||inbuf[i]=='\r')) i++;
  int neg=0; if(i<len && inbuf[i]=='-'){ neg=1; i++; }
  i32 n=0;
  while(i<len && inbuf[i]>='0' && inbuf[i]<='9'){ n=n*10+(inbuf[i]-'0'); i++; }
  if(neg) n=-n;
  *i_io = i;
  return n;
}

__attribute__((export_name("input_ptr")))  char* input_ptr(void){ return inbuf; }
__attribute__((export_name("output_ptr"))) char* output_ptr(void){ return outbuf; }
__attribute__((export_name("eval_source")))
u32 eval_source(u32 len){
  u32 i=0;
  i32 x = parse_int(len, &i);
  i32 y = parse_int(len, &i);
  i32 z = parse_int(len, &i);
  i32 r = tak(x, y, z);
  outlen=0;
  emitint(r);
  return outlen;
}
