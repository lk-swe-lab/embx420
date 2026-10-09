#!/usr/bin/env python3
"""Audit narrow HEAD-based delivery without touching the real index."""
import difflib
import hashlib
import io
import json
import os
from pathlib import Path
import posixpath
import re
import shutil
import subprocess
import tarfile
import tempfile
from urllib.parse import unquote

ROOT = Path('/home/name/Documents/420-EMBDX/A')
LOG = ROOT / 'logs/commands/clear-cd-review'
OLD = Path('/home/name/.hermes/cache/scratch/clear-cd-index-proposal-9r5nfy4z')
OUT = Path(tempfile.mkdtemp(prefix='clear-cd-reviewed-proposal-', dir=os.environ['TMPDIR']))
head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
core = (OLD / 'core-allowlist.txt').read_text().splitlines()
evidence = (OLD / 'evidence-allowlist.txt').read_text().splitlines()
extra = ['docs/reports/clear-cd-review.md'] + [
    'logs/commands/clear-cd-review/' + n for n in
    ['run.py', 'checks.stdout', 'checks.stderr', 'manifest.json', 'audit.py',
     'audit.json', 'final-allowlist.txt']]
final = core + evidence + extra
assert len(final) == len(set(final))
(LOG / 'final-allowlist.txt').write_text(''.join(n + '\n' for n in final))
original = {n: subprocess.check_output(['git', 'show', 'HEAD:' + n], cwd=ROOT).decode()
            for n in ['README.md', 'FILES.md']}
readme = original['README.md'].replace('runs eight builtin', 'runs ten builtin', 1)
readme = readme.replace('Optional standalone tools (run from the project root):',
    'Nine commands also have optional standalone tools in `build/`; `cd` is\n'
    'in-process only, with no standalone executable. Existing standalone help\n'
    'examples below remain valid; every command also supports `main help COMMAND`.')
anchor = '| [touch](docs/commands/touch.md) | Update timestamps/create files; optional JSON | `./build/touch --help` |'
assert anchor in readme
readme = readme.replace(anchor, anchor + '\n'
    '| [clear](docs/commands/clear.md) | Clear visible ANSI terminal screen; redirected output is silent | `./build/main help clear` |\n'
    '| [cd](docs/commands/cd.md) | Change physical cwd persistently in the loop | `./build/main help cd` |')
readme = readme.replace('These are selected subsets, not full GNU replacements; also registered builtins.',
    'These are selected subsets, not full GNU replacements; also registered builtins.\n'
    'Clear/cd [combined review](docs/reports/clear-cd-review.md) passes runtime gates;\n'
    'final delivery remains conditional. See [integration evidence](docs/reports/clear-cd-integration.md).')
files = (OLD / 'FILES.md').read_text()
anchor = '- logs/commands/clear-cd-integration/ — exact commands, raw stdout/stderr, fresh manifests and edge gates.'
assert anchor in files
files = files.replace(anchor, anchor + '\n'
    '- docs/reports/clear-cd-review.md — independent actual-main review and narrow delivery audit.\n'
    '- logs/commands/clear-cd-review/ — fresh gate outputs, hashes, audit and explicit final delivery allowlist.')
versions = {'README.md': readme, 'FILES.md': files}
patch = ''
for n, text in versions.items():
    (OUT / n).write_text(text)
    patch += ''.join(difflib.unified_diff(original[n].splitlines(True), text.splitlines(True),
                                         fromfile='a/' + n, tofile='b/' + n))
(OUT / 'mixed-docs.patch').write_text(patch)
clear = (ROOT / 'docs/reports/clear.md').read_text()
fixed = clear
for target in ['../COMMAND_ANATOMY.md', '../COMMAND_API.md']:
    fixed = re.sub(r'\[([^\]]*)\]\(' + re.escape(target) + r'\)',
                   lambda m: m[1] + ' (historical snapshot reference: `' + str(target) + '`; excluded from this delivery)', fixed)
(OUT / 'historical-links-optional.patch').write_text(''.join(difflib.unified_diff(
    clear.splitlines(True), fixed.splitlines(True), fromfile='a/docs/reports/clear.md',
    tofile='b/docs/reports/clear.md')))
tracked = set(subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', 'HEAD'], cwd=ROOT).decode().splitlines())
live_patch = ''
for n in ['docs/commands/clear.md', 'docs/commands/cd.md']:
    old_text = (ROOT / n).read_text()
    text = old_text.replace('Combined independent review and final commit are pending.',
                            'Combined independent runtime review passes; final delivery is conditional.')
    text = text.replace('combined independent review\nand final commit are pending.',
                        'combined independent runtime review passes;\nfinal delivery is conditional.')
    text += '\n[Combined review and delivery conditions](../reports/clear-cd-review.md).\n'
    assert text != old_text
    versions[n] = text
    target_path = OUT / n
    target_path.parent.mkdir(parents=True, exist_ok=True)
    target_path.write_text(text)
    live_patch += ''.join(difflib.unified_diff(old_text.splitlines(True), text.splitlines(True),
                                               fromfile='a/' + n, tofile='b/' + n))
(OUT / 'live-status.patch').write_text(live_patch)
delivered = tracked | set(final)
missing_links = []
private_flags = []
binary = []
for n in final:
    if n == 'logs/commands/clear-cd-review/audit.json':
        continue
    payload = versions[n].encode() if n in versions else (ROOT / n).read_bytes()
    if b'\0' in payload:
        binary.append(n)
    text = payload.decode(errors='replace')
    if re.search(r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY|\b(?:sk-|ghp_)[A-Za-z0-9_-]{20,}', text):
        private_flags.append(n)
    if n.endswith('.md'):
        for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)', text):
            t = target.split('#')[0]
            if not t or ':' in t:
                continue
            resolved = posixpath.normpath(posixpath.join(posixpath.dirname(n), unquote(t)))
            if resolved not in delivered and not any(x.startswith(resolved.rstrip('/') + '/') for x in delivered):
                missing_links.append({'source': n, 'target': target, 'resolved': resolved})
assert not binary and not private_flags
assert {x['resolved'] for x in missing_links} == {'docs/COMMAND_ANATOMY.md', 'docs/COMMAND_API.md'}
reviewed = {}
clear_hashes = json.loads((ROOT / 'logs/commands/clear-review/reviewed-sha256.json').read_text())
cd_hashes = json.loads((ROOT / 'logs/commands/cd-fix-review-20261008-210921/manifest.json').read_text())['reviewed_source_sha256']
for source in [clear_hashes, cd_hashes]:
    for n, value in source.items():
        if n.startswith(('src/', 'tests/')):
            reviewed[n] = hashlib.sha256((ROOT / n).read_bytes()).hexdigest() == value
assert all(reviewed.values())
stderr = (LOG / 'checks.stderr').read_text()
format_paths = sorted(set(re.findall(r'^((?:src|tests)/[^:]+):\d+:\d+: error: code should be clang-formatted', stderr, re.M)))
baseline = {n: (ROOT / n).read_bytes() == subprocess.check_output(['git', 'show', 'HEAD:' + n], cwd=ROOT) for n in format_paths}
assert len(format_paths) == 6 and all(baseline.values())
audit = {'head': head, 'proposal': str(OUT), 'counts': {'original_core': len(core),
    'original_evidence': len(evidence), 'review_additions': len(extra), 'final': len(final)},
    'original_proposal_bytes': sum((ROOT / n).stat().st_size for n in core + evidence),
    'duplicate_paths': [], 'binary_paths': binary, 'credential_flags': private_flags,
    'historical_missing_links': missing_links, 'disposition': 'Preserve byte snapshots under HEAD docs/reports/README.md historical-link notice; optional correction only in scratch.',
    'prior_reviewed_code_matches': reviewed, 'format_paths_match_HEAD': baseline,
    'source_unchanged_after_gates': json.loads((LOG / 'manifest.json').read_text())['sources_and_mixed_docs_unchanged'],
    'mixed_doc_versions_sha256': {n: hashlib.sha256(t.encode()).hexdigest() for n, t in versions.items()},
    'original_allowlist_sha256': hashlib.sha256((OLD / 'final-allowlist.txt').read_bytes()).hexdigest()}
(LOG / 'audit.json').write_text(json.dumps(audit, indent=2) + '\n')
tree = OUT / 'tree'
tree.mkdir()
archive = subprocess.check_output(['git', 'archive', 'HEAD'], cwd=ROOT)
with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
    tar.extractall(tree, filter='data')
for n in final:
    target = tree / n
    target.parent.mkdir(parents=True, exist_ok=True)
    if n in versions:
        target.write_text(versions[n])
    else:
        shutil.copy2(ROOT / n, target)
(tree / 'build').mkdir(exist_ok=True)
for dependency in ['argtable3', 'cunit']:
    (tree / 'build' / dependency).symlink_to((ROOT / 'build' / dependency).resolve())
# Check HEAD-relative patches in a separate scratch checkout, never the real index.
check = OUT / 'patch-check'
check.mkdir()
for n, text in original.items():
    (check / n).write_text(text)
result = subprocess.run(['git', 'apply', '--check', str(OUT / 'mixed-docs.patch')], cwd=check, capture_output=True)
assert result.returncode == 0, result.stderr
for n in ['docs/commands/clear.md', 'docs/commands/cd.md']:
    (check / n).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / n, check / n)
result = subprocess.run(['git', 'apply', '--check', str(OUT / 'live-status.patch')], cwd=check, capture_output=True)
assert result.returncode == 0, result.stderr
result = subprocess.run(['make', '-B', 'test'], cwd=tree,
    env={**os.environ, 'LC_ALL': 'C'}, capture_output=True, timeout=300)
for stream in ['stdout', 'stderr']:
    with (LOG / ('checks.' + stream)).open('ab') as f:
        f.write(b'\n### exact-reviewed-proposal-full-regression\n' + getattr(result, stream))
manifest = json.loads((LOG / 'manifest.json').read_text())
manifest['checks'].append({'label': 'exact-reviewed-proposal-full-regression',
    'argv': ['make', '-B', 'test'], 'cwd': str(tree), 'status': result.returncode,
    'environment_overrides': {'LC_ALL': 'C'}})
(LOG / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
audit['exact_proposal_test_status'] = result.returncode
audit['mixed_docs_patch_check_status'] = 0
(LOG / 'audit.json').write_text(json.dumps(audit, indent=2) + '\n')
print(json.dumps({'proposal': str(OUT), 'counts': audit['counts'],
    'proposal_test_status': result.returncode, 'reviewed_code': reviewed,
    'historical_missing_links': missing_links}, indent=2))
assert result.returncode == 0
