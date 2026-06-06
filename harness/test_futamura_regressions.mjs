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
