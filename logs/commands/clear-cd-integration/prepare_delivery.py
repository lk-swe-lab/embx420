#!/usr/bin/env python3
"""Prepare exact HEAD-based mixed-doc proposals without touching the index."""
import difflib
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from run import ROOT, LOG

prior = LOG / 'delivery-proposal.json'
proposal = (Path(json.loads(prior.read_text())['proposal']) if prior.exists() else
            Path(tempfile.mkdtemp(prefix='clear-cd-index-proposal-', dir=os.environ['TMPDIR'])))
assert proposal.is_dir()
base = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
head = {name: subprocess.check_output(['git', 'show', 'HEAD:' + name], cwd=ROOT).decode()
        for name in ['README.md', 'FILES.md']}
working = {name: (ROOT / name).read_text() for name in head}
# Preserve the unrelated working demo change, but exclude it from the index form.
readme = working['README.md'].split('## Demo\n', 1)[0] + '## Demo\n' + head['README.md'].split('## Demo\n', 1)[1]
# Insert only this owner's additions into the old, unreorganized HEAD registry.
addition = working['FILES.md'].split('- docs/commands/clear.md', 1)[1].split('- docs/commands/shell.md', 1)[0]
addition = '- docs/commands/clear.md' + addition
files = head['FILES.md'].replace('eight-command index', 'ten-command index', 1)
anchor = '- docs/commands/shell.md'
assert files.count(anchor) == 1
files = files.replace(anchor, addition + anchor, 1)
versions = {'README.md': readme, 'FILES.md': files}
patch = ''
for name, content in versions.items():
    (proposal / name).write_text(content)
    patch += ''.join(difflib.unified_diff(head[name].splitlines(True), content.splitlines(True),
                                          fromfile='a/' + name, tofile='b/' + name))
(proposal / 'mixed-docs.patch').write_text(patch)
# Check against HEAD in a separate scratch directory, not the dirty working tree.
check = proposal / 'patch-check'
check.mkdir(exist_ok=True)
for name, content in head.items():
    (check / name).write_text(content)
subprocess.run(['git', 'apply', '--check', str(proposal / 'mixed-docs.patch')], cwd=check, check=True)
core = ['Makefile', 'src/registry.c', 'tests/test_shell.py', 'tests/test_shell_module.c',
        'src/cmd_clear.c', 'src/cmd_clear.h', 'src/clear_main.c',
        'tests/test_clear.py', 'tests/test_clear_module.c',
        'src/cmd_cd.c', 'src/cmd_cd.h', 'tests/test_cd.py', 'tests/test_cd_module.c',
        'README.md', 'FILES.md', 'docs/commands/shell.md',
        'docs/commands/clear.md', 'docs/commands/cd.md',
        'docs/reports/clear.md', 'docs/reports/clear-review.md',
        'docs/reports/cd.md', 'docs/reports/cd-review.md', 'docs/reports/cd-fix-review.md',
        'docs/reports/clear-cd-integration.md',
        'logs/commands/cd-review-20261008-205019/evidence.json']
assert len(core) == len(set(core))
evidence = sorted(str(path.relative_to(ROOT)) for path in LOG.iterdir()
                  if path.is_file() and path.suffix in ('.py', '.json', '.stdout', '.stderr'))
for name, paths in [('core-allowlist.txt', core), ('evidence-allowlist.txt', evidence),
                    ('final-allowlist.txt', sorted(core + evidence))]:
    assert all((ROOT / path).is_file() for path in paths)
    (proposal / name).write_text('\n'.join(paths) + '\n')
assert not subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=ROOT).strip()
# Check immutable reviewed implementations and preserve exact inventory counts.
assert len(__import__('re').findall(r'^\| \[', readme, __import__('re').M)) == 10
assert 'demo/demo01.sh' not in readme
assert '## Data' not in files and 'wiki/wiki.sqlite' not in files
manifest = {'base_HEAD': base, 'proposal': str(proposal), 'core_count': len(core),
            'evidence_count': len(evidence), 'final_count': len(core + evidence),
            'real_index_empty': True, 'euid': os.geteuid(),
            'mixed_index_sha256': {name: hashlib.sha256(content.encode()).hexdigest()
                                    for name, content in versions.items()},
            'allowlisted_working_sha256': {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                                            for path in core},
            'new_doc_dependency_paths_excluded': ['docs/COMMAND_ANATOMY.md', 'docs/COMMAND_API.md']}
(proposal / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
(LOG / 'delivery-proposal.json').write_text(json.dumps(manifest, indent=2) + '\n')
# Include the generated proposal-pointer record in exact evidence/final lists.
evidence = sorted(set(evidence + [str((LOG / 'delivery-proposal.json').relative_to(ROOT))]))
(proposal / 'evidence-allowlist.txt').write_text('\n'.join(evidence) + '\n')
(proposal / 'final-allowlist.txt').write_text('\n'.join(sorted(core + evidence)) + '\n')
manifest['evidence_count'] = len(evidence)
manifest['final_count'] = len(core + evidence)
(proposal / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
(LOG / 'delivery-proposal.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps({'proposal': str(proposal), 'core_count': len(core),
                  'evidence_count': len(evidence), 'final_count': len(core + evidence),
                  'patch_check': 'pass', 'real_index_empty': True}, indent=2))
