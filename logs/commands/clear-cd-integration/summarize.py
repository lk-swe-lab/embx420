#!/usr/bin/env python3
"""Aggregate actual results and verify reviewed bytes/inherited failures."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
from run import ROOT, LOG

summary = {}
for label in ['baseline-shell', 'clear-green', 'combined-green', 'full-regression',
              'strict-o0', 'strict-o2', 'sanitizer-o0', 'actual-final-test']:
    errors = (LOG / (label + '.stderr')).read_text()
    output = (LOG / (label + '.stdout')).read_text()
    summary[label] = {
        'status': json.loads((LOG / (label + '.json')).read_text())['status'],
        'python_suite_counts': [int(x) for x in re.findall(r'Ran (\d+) tests?', errors)],
        'CUnit_totals': re.findall(r'(?m)^\s*(?:suites|tests|asserts)\s+[^\n]+', output)}
failed = sorted(set(re.findall(r'(?m)^([^:\n]+):\d+:\d+: error:',
                              (LOG / 'global-format.stderr').read_text())))
summary['format_fail_paths'] = failed
summary['format_paths_match_HEAD'] = {
    path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() ==
    hashlib.sha256(subprocess.check_output(['git', 'show', 'HEAD:' + path], cwd=ROOT)).hexdigest()
    for path in failed}
for command, review in [('clear', 'logs/commands/clear-review/reviewed-sha256.json'),
                        ('cd', 'logs/commands/cd-fix-review-20261008-210921/source-hashes.json')]:
    hashes = json.loads((ROOT / review).read_text())
    summary[command + '_reviewed_code_unchanged'] = {
        path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == digest
        for path, digest in hashes.items() if path.startswith(('src/', 'tests/'))}
summary['remaining_capacity'] = 32 - 10 - 1
summary['index_empty'] = not subprocess.check_output(
    ['git', 'diff', '--cached', '--name-only'], cwd=ROOT).strip()
(LOG / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
