#!/usr/bin/env python3
"""Fixed independent expectations; reference and project modes are explicit."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--program', required=True)
p.add_argument('--reference', action='store_true')
a = p.parse_args()
program = str(Path(a.program).resolve())
checks = 0
with tempfile.TemporaryDirectory(prefix='head-test-') as tmp:
    root = Path(tmp)

    def check(name, data, args, expected, status=0, diagnostic=False):
        global checks
        source = root / 'input'
        source.write_bytes(data)
        result = subprocess.run([program, *args], input=data, cwd=tmp,
                                capture_output=True, timeout=5,
                                env={**os.environ, 'LC_ALL': 'C'})
        print(f'{name}: expected={expected[:90]!r} got={result.stdout[:90]!r}')
        assert result.stdout == expected, name
        assert result.returncode == status, (name, result.returncode, result.stderr)
        assert bool(result.stderr) == diagnostic, (name, result.stderr)
        assert source.read_bytes() == data, 'input modified'
        checks += 1

    check('default', b''.join(f'{n}\n'.encode() for n in range(12)), [],
          b''.join(f'{n}\n'.encode() for n in range(10)))
    for name, data, count, expected in [
        ('empty', b'', '3', b''), ('zero', b'a\nb', '0', b''),
        ('one', b'a\nb\nc', '1', b'a\n'),
        ('blank', b'\n\nx\n', '2', b'\n\n'),
        ('binary', b'\x00\xff\nB\x00', '1', b'\x00\xff\n'),
        ('long', b'x'*70000+b'\ny\n', '1', b'x'*70000+b'\n'),
        ('larger', b'a\nb', '20', b'a\nb'),
        ('unsigned_max', b'a\nb', '18446744073709551615', b'a\nb')]:
        check(name, data, ['-n', count], expected)
    check('named', b'a\nb', ['--lines=1', 'input'], b'a\n')
    check('explicit stdin', b'a\nb', ['-n1', '-'], b'a\n')
    check('last count wins', b'a\nb', ['-n2', '-n1'], b'a\n')
    (root/'-odd').write_bytes(b'odd\n')
    check('dash path', b'', ['--', '-odd'], b'odd\n')
    check('missing', b'', ['absent'], b'', 1, True)
    check('directory read', b'', ['.'], b'', 1, True)
    with open('/dev/full', 'wb') as full:
        r = subprocess.run([program], input=b'a\n', stdout=full,
                           stderr=subprocess.PIPE, timeout=5)
        assert r.returncode == 1 and r.stderr
        checks += 1
    if not a.reference:
        for bad in ['', '-1', '+1', ' 1', '1K', '1x',
                    '18446744073709551616']:
            check('bad count '+repr(bad), b'', ['-n', bad], b'', 2, True)
        for args in [['-n'], ['--bogus'], ['input', 'input'],
                     ['--help', '--bogus'], ['--help', '-n', 'bad']]:
            check('bad args '+str(args), b'', args, b'', 2, True)
        r = subprocess.run([program, '-h'], capture_output=True, timeout=5)
        assert r.returncode == 0 and b'Usage:' in r.stdout and not r.stderr
        checks += 1
        r = subprocess.run([program, '--help'], capture_output=True, timeout=5)
        assert r.returncode == 0 and b'Usage:' in r.stdout and not r.stderr
        checks += 1
        for data, count, lines in [(b'a\r\n\nlast', '2', ['a\r', '']),
                                   (b'\x00"\t\n', '10', ['\x00"\t']),
                                   ('é😀\n'.encode(), '1', ['é😀']),
                                   (b'', '0', [])]:
            expected = (json.dumps({'path':'-', 'lines':lines},
                                  ensure_ascii=False,
                                  separators=(',', ':'))+'\n').encode()
            check('json', data, ['--json', '-n', count], expected)
        check('bad UTF8', b'\xff\n', ['--json'], b'', 1, True)
        for invalid in [b'\xc0\xaf', b'\xed\xa0\x80', b'\xe2\x82',
                        b'\xf4\x90\x80\x80', b'\x80']:
            check('invalid scalar', invalid, ['--json'], b'', 1, True)
        check('terminal LF no phantom line', b'x\n', ['--json'],
              b'{"path":"-","lines":["x"]}\n')
        escaped_path = 'quote"\\name'
        (root / escaped_path).write_bytes(b'a\n')
        check('JSON path escape', b'', ['--json', escaped_path],
              (json.dumps({'path': escaped_path, 'lines': ['a']},
                          separators=(',', ':'))+'\n').encode())
        bad_path = os.fsdecode(b'bad\xff')
        (root / bad_path).write_bytes(b'a\n')
        check('invalid UTF8 path', b'', ['--json', bad_path], b'', 1, True)
        check('validate selected only', b'good\n\xff\n', ['--json', '-n1'],
              b'{"path":"-","lines":["good"]}\n')
        check('zero opens file', b'', ['-n0', 'absent'], b'', 1, True)
print(f'head: {checks} checks passed')
