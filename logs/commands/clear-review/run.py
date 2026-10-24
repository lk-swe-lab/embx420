#!/usr/bin/env python3
"""Independent isolated clear review. All subprocess streams are captured."""
import difflib
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path('/home/name/Documents/420-EMBDX/A')
LOG = ROOT / 'logs/commands/clear-review'
WORK = Path('/home/name/.hermes/cache/scratch/clear-review')
TREE = WORK / 'tree'
FLAGS = ['-std=c17', '-Wall', '-Wextra', '-Wpedantic', '-Wformat=2',
         '-Wshadow', '-Wconversion', '-Wstrict-prototypes', '-g3', '-Werror']
WRAPS = ['-Wl,--wrap=write,--wrap=isatty,--wrap=malloc,--wrap=arg_lit0,--wrap=arg_end']
CHECKS = '-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling'
records = []
LOG.mkdir(parents=True, exist_ok=True)
TREE.mkdir(parents=True, exist_ok=True)
for folder in ['src', 'tests']:
    (TREE / folder).mkdir(exist_ok=True)
    for source in (ROOT / folder).iterdir():
        if source.is_file() and source.suffix in ['.c', '.h', '.py']:
            shutil.copy2(source, TREE / folder / source.name)
for name in ['Makefile', '.clang-format']:
    shutil.copy2(ROOT / name, TREE / name)
(TREE / 'build').mkdir(exist_ok=True)
for lib in ['argtable3', 'cunit']:
    dest = TREE / 'build' / lib
    if not dest.exists():
        dest.symlink_to(ROOT / 'build' / lib, target_is_directory=True)
manifest = {str(p.relative_to(TREE)): hashlib.sha256(p.read_bytes()).hexdigest()
            for folder in ['src', 'tests'] for p in (TREE / folder).iterdir() if p.is_file()}
(LOG / 'snapshot-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n')
INC = ['-I' + str(TREE / 'src'), '-I' + str(TREE / 'build/argtable3/include')]
LIB = ['-L' + str(TREE / 'build/argtable3/lib'), '-largtable3', '-lm']

def run(label, args, expected=0, env=None):
    result = subprocess.run(args, cwd=TREE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=180,
                            env=env or dict(os.environ, LC_ALL='C'))
    (LOG / (label + '.stdout')).write_bytes(result.stdout)
    (LOG / (label + '.stderr')).write_bytes(result.stderr)
    records.append(dict(label=label, argv=args, cwd=str(TREE),
                        status=result.returncode, expected=expected,
                        environment_overrides={k: env[k] for k in ['ASAN_OPTIONS','UBSAN_OPTIONS'] if env and k in env}))
    (LOG / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
    print(label, 'status', result.returncode, 'expected', expected, flush=True)
    if result.returncode != expected:
        print(repr(result.stdout), repr(result.stderr))
        raise SystemExit(1)
    return result

shell = ['main', 'traverse', 'shell', 'registry', 'cmd_cat', 'cmd_head',
         'cmd_tail', 'cmd_pwd', 'cmd_stat', 'cmd_ls', 'cmd_mkdir', 'cmd_touch', 'cmd_lines']
sources = ['src/' + name + '.c' for name in shell]
run('build-baseline-main', ['cc', *FLAGS, '-O0', *INC, *sources, *LIB, '-o', str(WORK / 'baseline-main')])
run('baseline-no-clear', [str(WORK / 'baseline-main'), 'clear'], 127)
registry = TREE / 'src/registry.c'
original = registry.read_text()
assert '&cmd_clear_spec' not in original
registry.write_text(original.replace('#include "registry.h"', '#include "cmd_clear.h" /* cmd_clear_spec. */\n#include "registry.h"', 1).replace('&cmd_touch_spec};', '&cmd_touch_spec, &cmd_clear_spec};', 1))
run('format-scratch-registry', ['clang-format', '-i', 'src/registry.c'])
(LOG / 'registry.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True), registry.read_text().splitlines(True), fromfile='a/src/registry.c', tofile='b/src/registry.c')))

for mode, optimization, extra in [('strict', '-O0', []), ('optimized', '-O2', []),
                                 ('sanitize', '-O0', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']),
                                 ('sanitize-O2', '-O2', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie'])]:
    env = dict(os.environ, LC_ALL='C', ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    for target, code, wraps in [('clear', ['src/clear_main.c','src/cmd_clear.c','src/cmd_lines.c'], []),
                                ('module', ['tests/test_clear_module.c','src/cmd_clear.c','src/cmd_lines.c'], WRAPS),
                                ('main', [*sources,'src/cmd_clear.c'], [])]:
        run(mode + '-build-' + target, ['cc', *FLAGS, optimization, *extra, *INC, *code, *wraps, *LIB, '-o', str(WORK / (mode + '-' + target))])
    run(mode + '-module', [str(WORK / (mode + '-module'))], env=env)
    run(mode + '-cli-main', [sys.executable, 'tests/test_clear.py', '--program', str(WORK / (mode + '-clear')), '--main', str(WORK / (mode + '-main')), '-v'], env=env)

run('format-owned', ['clang-format', '--dry-run', '--Werror', 'src/cmd_clear.c','src/cmd_clear.h','src/clear_main.c','tests/test_clear_module.c'])
run('lint-owned', ['clang-tidy', '--checks=' + CHECKS, '--warnings-as-errors=*', 'src/cmd_clear.c','src/clear_main.c','tests/test_clear_module.c','--', *INC, *FLAGS, '-O0'])
# Exercise the worker's proposed build wiring only in the isolated tree.
patch = LOG / 'worker-Makefile-clear.patch'
run('apply-scratch-make-patch', ['patch', '-p1', '--input', str(patch)])
run('scratch-make-test-clear', ['make', '-B', 'test-clear', 'CFLAGS=' + ' '.join(FLAGS + ['-O0'])])
# Record source stability; parent can change other shared paths concurrently.
owned = ['src/cmd_clear.c','src/cmd_clear.h','src/clear_main.c','tests/test_clear.py','tests/test_clear_module.c']
stable = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() == manifest[p] for p in owned}
(LOG / 'source-stability.json').write_text(json.dumps(stable, indent=2) + '\n')
assert all(stable.values())
print('ALL INDEPENDENT CORE GATES PASSED')
