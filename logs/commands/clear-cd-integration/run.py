#!/usr/bin/env python3
"""Capture exact integration commands and outputs, never print terminal bytes."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
LOG = Path(__file__).resolve().parent


def run(label, command, cwd=ROOT, env=None):
    process = subprocess.run(command, cwd=cwd,
                             env={**os.environ, 'LC_ALL': 'C', **(env or {})},
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             timeout=240)
    (LOG / (label + '.stdout')).write_bytes(process.stdout)
    (LOG / (label + '.stderr')).write_bytes(process.stderr)
    record = {'command': command, 'cwd': str(cwd), 'environment': env or {},
              'status': process.returncode}
    (LOG / (label + '.json')).write_text(json.dumps(record, indent=2) + '\n')
    print(label, 'status', process.returncode)
    return process.returncode


if __name__ == '__main__':
    raise SystemExit(run(sys.argv[1], sys.argv[2:]))
