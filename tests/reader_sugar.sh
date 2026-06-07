#!/usr/bin/env bash
# tests/reader_sugar.sh — validates fn(a, b) ≡ (fn a b) reader sugar.
#
# Usage:
#   ./build.sh && ./tests/reader_sugar.sh
#   CLI=./native_cli_lisp_gc ./tests/reader_sugar.sh

set -u

CLI="${CLI:-./native_cli_lisp}"
[ -x "$CLI" ] || { echo "missing $CLI — run ./build.sh first" >&2; exit 2; }

pass=0
fail=0
failures=()

# check_eq: assert two source forms print the same result.
# Both are wrapped in quote so we're comparing AST shapes via the printer,
# independent of evaluation.
check_eq() {
  local desc="$1" sugar="$2" classic="$3"
  local g1 g2
  g1=$("$CLI" -e "$sugar" 2>&1)
  g2=$("$CLI" -e "$classic" 2>&1)
  if [ "$g1" = "$g2" ]; then
    pass=$((pass+1))
  else
    fail=$((fail+1))
    failures+=("$desc"$'\n'"  sugar:   $sugar -> $g1"$'\n'"  classic: $classic -> $g2")
  fi
}

# check_val: assert a source form evaluates to an exact printed value.
check_val() {
  local desc="$1" expr="$2" expected="$3"
  local got
  got=$("$CLI" -e "$expr" 2>&1)
  if [ "$got" = "$expected" ]; then
    pass=$((pass+1))
  else
    fail=$((fail+1))
    failures+=("$desc"$'\n'"  expr: $expr"$'\n'"  want: $expected"$'\n'"  got:  $got")
  fi
}

# ---- core equivalence ----
check_eq "zero args"          "'fn()"           "'(fn)"
check_eq "one arg"            "'fn(a)"          "'(fn a)"
check_eq "two args, space"    "'fn(a b)"        "'(fn a b)"
check_eq "two args, comma"    "'fn(a, b)"       "'(fn a b)"
check_eq "three args"         "'fn(a, b, c)"    "'(fn a b c)"

# ---- nesting ----
check_eq "nested call"        "'f(g(x))"        "'(f (g x))"
check_eq "nested with sib"    "'f(g(x), y)"     "'(f (g x) y)"
check_eq "triple nest"        "'f(g(h(x)))"     "'(f (g (h x)))"

# ---- mixed sugar + classic ----
check_eq "classic unchanged"  "'(fn a b)"       "'(fn a b)"
check_eq "sugar in classic"   "'(f g(x))"       "'(f (g x))"
check_eq "classic in sugar"   "'f((g x))"       "'(f (g x))"

# ---- critical whitespace-sensitive pair ----
# These two MUST stay distinct. If they collide the sugar has eaten the language.
check_eq "space => siblings"  "'(map fn (a b))" "'(map fn (a b))"
check_eq "tight => call"      "'(map fn(a b))"  "'(map (fn a b))"

# ---- comma-as-whitespace ----
check_eq "list with commas"   "'(a, b, c)"      "'(a b c)"
check_eq "extra commas"       "'f(a,, b)"       "'(f a b)"
check_eq "trailing comma"     "'f(a,)"          "'(f a)"

# ---- end-to-end eval ----
check_val "inc(5)" \
  "(begin (define inc (lambda (x) (+ x 1))) (inc 5))" \
  "6"
check_val "inc(5) via sugar" \
  "(begin (define inc (lambda (x) (+ x 1))) inc(5))" \
  "6"
check_val "fact(5) via sugar" \
  "(begin (define fact (lambda (n) (if (< n 2) 1 (* n fact(-(n, 1)))))) fact(5))" \
  "120"

# ---- semantic lock-in ----
# Sugar args are unparenthesized siblings, not "one expression". f(- n 1)
# means (f - n 1), NOT (f (- n 1)). To group, nest the sugar: f(-(n, 1)).
check_eq "args are siblings" "'f(- n 1)"    "'(f - n 1)"
check_eq "nest to group"     "'f(-(n, 1))"  "'(f (- n 1))"

# ---- report ----
echo
echo "passed: $pass"
echo "failed: $fail"
if [ "$fail" -gt 0 ]; then
  echo
  echo "failures:"
  printf '%s\n\n' "${failures[@]}"
  exit 1
fi
