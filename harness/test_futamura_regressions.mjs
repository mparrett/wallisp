import fs from 'fs';
import os from 'os';
import path from 'path';
import { fileURLToPath } from 'url';
import { spawnSync } from 'child_process';

const ROOT = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'wallisp-futamura-'));
const preproc = path.join(tmp, 'preproc');
const specialize = path.join(tmp, 'specialize');

function run(cmd, args, input = '') {
  return spawnSync(cmd, args, { input, encoding: 'utf8' });
}

function build(out, src) {
  const r = run('cc', ['-O2', '-Wall', '-Wextra', '-o', out, src]);
  if (r.status !== 0) {
    process.stderr.write(r.stderr);
    process.exit(r.status || 1);
  }
}

function assert(cond, msg) {
  if (!cond) {
    console.error('FAIL  ' + msg);
    process.exitCode = 1;
  } else {
    console.log('PASS  ' + msg);
  }
}

function runWasm(file, input = '') {
  const bytes = fs.readFileSync(file);
  const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), {});
  const e = instance.exports;
  const mem = new Uint8Array(e.memory.buffer);
  const enc = new TextEncoder();
  const dec = new TextDecoder();
  const src = enc.encode(input);
  mem.set(src, e.input_ptr());
  const n = e.eval_source(src.length);
  return dec.decode(mem.subarray(e.output_ptr(), e.output_ptr() + n));
}

build(preproc, path.join(ROOT, 'prototype/futamura/preproc.c'));
build(specialize, path.join(ROOT, 'prototype/futamura/specialize.c'));

{
  const input = "(define bar 10)\n(define x '(foo $bar baz))\n";
  const r = run(preproc, [], input);
  assert(r.status === 0, 'preproc quote case exits cleanly');
  assert(r.stdout.includes('(quote (foo (__comptime__ bar) baz))'), 'quoted $ form is not folded');
  assert(!r.stdout.includes('(quote (foo 10 baz))'), 'quoted $ form does not become evaluated data');
}

{
  const input = [
    '(define (choose c) (if c (lambda (x) (+ x 1)) (lambda (x) (+ x 2))))',
    '(define (work n) ((choose 1) n))',
    '',
  ].join('\n');
  const r = run(specialize, [], input);
  assert(r.status === 0, 'closure-valued if can fold with comptime condition');
  assert(r.stdout.includes('static u32 work'), 'folded closure-valued helper emits runtime caller');
}

{
  const input = [
    '(define (choose c) (if c (lambda (x) (+ x 1)) (lambda (x) (+ x 2))))',
    '(define (work n) ((choose n) n))',
    '',
  ].join('\n');
  const r = run(specialize, [], input);
  assert(r.status !== 0, 'closure-valued function rejects runtime condition');
  assert(r.stderr.includes('comptime-only closure-valued function `choose` called with runtime arguments'), 'runtime closure-valued call has clear diagnostic');
}

{
  const input = '(define (work n) (let ((y n) (f (lambda (x) (+ x y)))) (f 1)))\n';
  const r = run(specialize, [], input);
  assert(r.status === 0, 'same-scope runtime closure capture still beta-reduces');
  assert(r.stdout.includes('u32 y = n'), 'same-scope runtime capture emits the local binding');
}

{
  const input = '(define (work n) (let ((f (let ((y n)) (lambda (x) (+ x y))))) (f 1)))\n';
  const r = run(specialize, [], input);
  assert(r.status !== 0, 'runtime closure capture cannot escape its local scope');
  assert(r.stderr.includes('closure-valued let binding `f` depends on runtime data'), 'escaping runtime capture has clear diagnostic');
}

{
  const input = '(define (foo-bar x) (+ x 1))\n(define (foo_bar x) (+ x 2))\n';
  const r = run(specialize, [], input);
  assert(r.status !== 0, 'C identifier mangling collision is rejected');
  assert(r.stderr.includes('collides with an existing C identifier after mangling'), 'mangling collision has clear diagnostic');
}

{
  const input = '(define (work) (cons 1 2))\n';
  const r = run(specialize, [], input);
  assert(r.status === 0, 'comptime cons specializes successfully');
  assert(r.stdout.includes('static const Cell ct_pool[1]'), 'escaped comptime cons emits a residual pool');
  assert(r.stdout.includes('return mkcons(0);'), 'escaped comptime cons returns the materialized cell');
}

{
  const input = "(define (work) (car '(1 2 3)))\n";
  const r = run(specialize, [], input);
  assert(r.status === 0, 'car of quoted list specializes successfully');
  assert(r.stdout.includes('return mkfix(1);'), 'car of quoted list folds to 1');
}

{
  const input = "(define (work) (null? '()))\n";
  const r = run(specialize, [], input);
  assert(r.status === 0, 'null? of quoted nil specializes successfully');
  assert(r.stdout.includes('return mkfix(1);'), 'null? of nil folds truthy');
}

{
  const input = '(define (f x) (cons x 1))\n';
  const r = run(specialize, [], input);
  assert(r.status !== 0, 'runtime cons is rejected');
  assert(r.stderr.includes('specialize: runtime cons not yet supported (B-v3 v1 is comptime-only)'), 'runtime cons has clear diagnostic');
}

{
  const src = fs.readFileSync(path.join(ROOT, 'prototype/futamura/nrev_demo.lisp'), 'utf8');
  const r = run(specialize, [], src);
  assert(r.status === 0, 'nrev demo specializes successfully');
  assert(r.stdout.includes('return mkfix(1275);'), 'nrev demo folds to mkfix(1275)');
}

{
  const residuals = [
    'residual_fib_gen.wasm',
    'residual_tak_gen.wasm',
    'residual_closures_demo_gen.wasm',
    'residual_closures_v2_demo_gen.wasm',
    'residual_closures_named_demo_gen.wasm',
  ];
  const before = new Map(residuals.map(f => [f, fs.readFileSync(path.join(ROOT, f))]));
  const r = run('bash', [path.join(ROOT, 'prototype/futamura/build.sh')]);
  if (r.status !== 0) {
    process.stderr.write(r.stdout);
    process.stderr.write(r.stderr);
  }
  assert(r.status === 0, 'futamura build including nrev demo succeeds');
  for (const f of residuals) {
    const after = fs.readFileSync(path.join(ROOT, f));
    assert(Buffer.compare(before.get(f), after) === 0, `${f} stays byte-identical after rebuild`);
  }
  assert(runWasm(path.join(ROOT, 'residual_nrev_demo_gen.wasm')) === '1275', 'nrev residual wasm produces 1275');
}
