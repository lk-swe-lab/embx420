#!/usr/bin/env python3
"""Fresh isolated builds from final working code, with borrowed dependencies."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
from run import ROOT, LOG, run

scratch = Path(tempfile.mkdtemp(prefix='clear-cd-final-', dir=os.environ['TMPDIR']))
flags = '-std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes -Werror -g3 '
results = []
for name, optimization, sanitizer in [('strict-o0', '-O0', False),
                                       ('strict-o2', '-O2', False),
                                       ('sanitizer-o0', '-O0', True)]:
    tree = scratch / name
    tree.mkdir()
    for directory in ['src', 'tests']:
        shutil.copytree(ROOT / directory, tree / directory)
    for filename in ['Makefile', '.clang-format']:
        shutil.copy2(ROOT / filename, tree / filename)
    (tree / 'build').mkdir()
    for dependency in ['argtable3', 'cunit']:
        (tree / 'build' / dependency).symlink_to((ROOT / 'build' / dependency).resolve())
    cflags = flags + optimization
    environment = {}
    if sanitizer:
        cflags += ' -fsanitize=address,undefined -fno-omit-frame-pointer'
        environment = {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1',
                       'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
    targets = ['test-clear', 'test-cd', 'test-shell'] if name == 'strict-o2' else ['test']
    status = run(name, ['make', '-B', *targets, 'CFLAGS=' + cflags], tree, environment)
    results.append({'name': name, 'tree': str(tree), 'status': status})
    if status == 0:
        edge_status = run(name + '-edges', ['python3', str(LOG / 'edges.py'),
                                          str(tree / 'build' / 'main')], tree, environment)
        results.append({'name': name + '-edges', 'status': edge_status})
manifest = {'scratch': str(scratch), 'results': results,
            'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for d in ['src', 'tests'] for p in sorted((ROOT / d).glob('*'))
                              if p.is_file()},
            'makefile_sha256': hashlib.sha256((ROOT / 'Makefile').read_bytes()).hexdigest()}
(LOG / 'fresh-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
raise SystemExit(any(record['status'] for record in results))
