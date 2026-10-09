#!/usr/bin/env python3
"""Independent combined review; writes only owned logs and scratch trees."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path('/home/name/Documents/420-EMBDX/A')
LOG = ROOT / 'logs/commands/clear-cd-review'
SCRATCH = Path(tempfile.mkdtemp(prefix='clear-cd-independent-', dir=os.environ['TMPDIR']))
ENV = {**os.environ, 'LC_ALL': 'C'}
FILES = [p for d in ['src', 'tests'] for p in (ROOT / d).rglob('*')
         if p.is_file() and '__pycache__' not in p.parts]
FILES += [ROOT / 'Makefile', ROOT / 'README.md', ROOT / 'FILES.md']

def hashes():
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(FILES)}

before = hashes()
records = []

def run(label, args, cwd=ROOT, extra=None):
    result = subprocess.run(args, cwd=cwd, env={**ENV, **(extra or {})},
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=300)
    for stream in ['stdout', 'stderr']:
        with (LOG / ('checks.' + stream)).open('ab') as out:
            out.write(('\n### ' + label + '\n').encode())
            out.write(getattr(result, stream))
    records.append({'label': label, 'argv': args, 'cwd': str(cwd),
                    'environment_overrides': extra or {}, 'status': result.returncode})
    (LOG / 'manifest.json').write_text(json.dumps({
        'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip(),
        'scratch': str(SCRATCH), 'euid': os.geteuid(), 'checks': records,
        'before_sha256': before, 'after_sha256': hashes(),
        'sources_and_mixed_docs_unchanged': before == hashes()}, indent=2) + '\n')
    print(label, result.returncode, flush=True)
    return result.returncode

if __name__ == '__main__':
    run('actual-full-regression', ['make', '-B', 'test'])
    run('actual-build', ['make', '-B', 'all'])
    run('actual-lint', ['make', 'lint'])
    scope = 'src/registry.c tests/test_shell_module.c src/cmd_clear.c src/cmd_clear.h src/clear_main.c tests/test_clear_module.c src/cmd_cd.c src/cmd_cd.h tests/test_cd_module.c'
    run('scoped-format', ['make', 'format-check', 'C_FILES=' + scope])
    run('global-format', ['make', 'format-check'])
    run('actual-pty-cwd-edges', ['python3', str(ROOT / 'logs/commands/clear-cd-integration/edges.py'), str(ROOT / 'build/main')])
    run('dependency-internal-oom', [str(ROOT / 'build/test_cd_module'), '--internal-oom'])
    flags = '-std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes -Werror -g3 '
    for label, options, extra in [
        ('strict-o2', '-O2', {}),
        ('sanitizer-o0', '-O0 -fsanitize=address,undefined -fno-omit-frame-pointer',
         {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1',
          'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'})]:
        tree = SCRATCH / label
        tree.mkdir()
        for d in ['src', 'tests']:
            shutil.copytree(ROOT / d, tree / d, ignore=shutil.ignore_patterns('__pycache__'))
        shutil.copy2(ROOT / 'Makefile', tree / 'Makefile')
        (tree / 'build').mkdir()
        for dep in ['argtable3', 'cunit']:
            (tree / 'build' / dep).symlink_to((ROOT / 'build' / dep).resolve())
        if run(label, ['make', '-B', 'test-shell', 'test-clear', 'test-cd', 'CFLAGS=' + flags + options], tree, extra) == 0:
            run(label + '-pty-cwd-edges', ['python3', str(ROOT / 'logs/commands/clear-cd-integration/edges.py'), str(tree / 'build/main')], tree, extra)
    run('index-still-empty', ['git', 'diff', '--cached', '--exit-code'])
