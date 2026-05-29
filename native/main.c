// native/main.c — native equivalent of harness/lisp-cli.mjs.
//
//   native_cli_<engine> program.lisp        # eval a file
//   native_cli_<engine> -e "(+ 1 2)"         # eval an inline expression
//   echo "(* 6 7)" | native_cli_<engine>     # eval from stdin
//
// Compile-time engine selection via -DENGINE_SRC:
//   clang -O2 -DENGINE_SRC='"../engines/bytecode_gc.c"' \
//         -o native_cli_bytecode_gc native/main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wunknown-attributes"
#define memset __engine_unused_memset
#define memcpy __engine_unused_memcpy
#include ENGINE_SRC
#undef memset
#undef memcpy
#pragma clang diagnostic pop

#define IN_CAP 8192

static int read_all_stdin(char *buf, int cap) {
  int n = 0;
  while (n < cap) {
    int r = (int)read(0, buf + n, cap - n);
    if (r <= 0) break;
    n += r;
  }
  return n;
}

static int read_file(const char *path, char *buf, int cap) {
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return -1; }
  size_t n = fread(buf, 1, cap, f);
  fclose(f);
  return (int)n;
}

int main(int argc, char **argv) {
  char *in = input_ptr();
  int n = 0;

  if (argc >= 3 && strcmp(argv[1], "-e") == 0) {
    n = (int)strlen(argv[2]);
    if (n > IN_CAP) { fprintf(stderr, "source too large\n"); return 2; }
    memcpy(in, argv[2], n);
  } else if (argc == 2) {
    n = read_file(argv[1], in, IN_CAP);
    if (n < 0) return 2;
  } else if (!isatty(0)) {
    n = read_all_stdin(in, IN_CAP);
  } else {
    fprintf(stderr, "usage: %s [file | -e expr | < stdin]\n", argv[0]);
    return 2;
  }

  int olen = eval_source((unsigned)n);
  fwrite(output_ptr(), 1, olen, stdout);
  fputc('\n', stdout);
  // Match lisp-cli.mjs exit-code semantics: 1 if the engine returned <error>.
  if (olen == 7 && memcmp(output_ptr(), "<error>", 7) == 0) return 1;
  return 0;
}
