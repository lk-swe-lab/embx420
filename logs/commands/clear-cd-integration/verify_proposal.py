#!/usr/bin/env python3
"""Verify HEAD plus the exact proposed core, without staging anything."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from urllib.parse import unquote
from run import ROOT, LOG, run

record = json.loads((LOG / 'delivery-proposal.json').read_text())
proposal = Path(record['proposal'])
core = (proposal / 'core-allowlist.txt').read_text().splitlines()
tracked = set(subprocess.check_output(['git', 'ls-files'], cwd=ROOT).decode().splitlines())
tree = Path(tempfile.mkdtemp(prefix='clear-cd-proposed-core-', dir=os.environ['TMPDIR']))
# Read only build/code/test inputs from HEAD; never archive or read AI logs/wiki.
for path in sorted(tracked):
    if path.startswith(('src/', 'tests/')) or path in ('Makefile', '.clang-format'):
        target = tree / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(subprocess.check_output(['git', 'show', 'HEAD:' + path], cwd=ROOT))
for path in core:
    target = tree / path
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(proposal / path if path in ('README.md', 'FILES.md') else ROOT / path, target)
(tree / 'build').mkdir()
for dependency in ['argtable3', 'cunit']:
    (tree / 'build' / dependency).symlink_to((ROOT / 'build' / dependency).resolve())
status = run('proposed-core-full-test', ['make', '-B', 'test'], tree)
# Only resolve links introduced by newly added command/implementation/review docs;
# tracked targets are available in a prospective checkout even if not copied here.
missing = []
for path in core:
    if not path.startswith('docs/') or path in tracked:
        continue
    for raw in re.findall(r'\]\(([^)]+)\)', (ROOT / path).read_text()):
        url = raw.split('#', 1)[0]
        if not url or '://' in url:
            continue
        target = os.path.normpath(str(Path(path).parent / unquote(url)))
        present = target in tracked or target in core or any(
            candidate.startswith(target.rstrip('/') + '/') for candidate in core)
        if not present:
            missing.append({'source': path, 'target': target})
assert {item['target'] for item in missing} == {'docs/COMMAND_ANATOMY.md', 'docs/COMMAND_API.md'}, missing
assert not subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=ROOT).strip()
result = {'tree': str(tree), 'runtime_full_test_status': status,
          'missing_new_markdown_links': missing, 'real_index_empty': True}
(LOG / 'proposed-core-verification.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
raise SystemExit(status)
