#!/usr/bin/env python3
"""Independent byte oracle; reference/common and project differences separate."""
import argparse
import errno
import fcntl
import os
from pathlib import Path
import pty
import random
import select
import subprocess
import tempfile
import termios
import time

P = argparse.ArgumentParser()
P.add_argument('program')
P.add_argument('--reference', action='store_true')
P.add_argument('--slice', choices=['literal', 'parser', 'all'], default='all')
P.add_argument('--main')
A = P.parse_args()
PROGRAM = os.fsencode(Path(A.program).resolve())
ENV = {b'PATH': b'/usr/bin:/bin', b'LC_ALL': b'C'}
COUNT = 0


def check(args, out, status=0, error=False, prefix=None):
    global COUNT
    cmd = [PROGRAM] if prefix is None else prefix
    r = subprocess.run(cmd + args, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, env=ENV, timeout=10)
    assert (r.returncode, r.stdout) == (status, out), (
        args, 'expected', status, out[:100], 'got', r.returncode, r.stdout[:100])
    assert bool(r.stderr) == error, (args, r.stderr)
    COUNT += 1
    return r


# Expected bytes are specified independently, never generated with GNU echo.
COMMON = [([], b'\n'), ([b''], b'\n'), ([b'', b''], b' \n'),
          ([b'alpha', b'beta'], b'alpha beta\n'),
          ([b' a ', b'', b'b '], b' a   b \n'),
          ([b'a\nb', b'c\rd', b'\t'], b'a\nb c\rd \t\n'),
          ([b'\\n', b'\\0', b'\\x41'], b'\\n \\0 \\x41\n'),
          ([b'\xff\xfe', b'\x01\x7f'], b'\xff\xfe \x01\x7f\n'),
          ([b'-'], b'-\n'), ([b'x' * 65000], b'x' * 65000 + b'\n'),
          ([b'q'] * 4096, b'q ' * 4095 + b'q\n')]
if A.slice in ('literal', 'all'):
    for args, expected in COMMON:
        check(args, expected)
    for value in range(1, 256):
        byte = bytes([value])
        check([byte], byte + b'\n')
    rng = random.Random(420)
    for _ in range(200):
        operands = [b'literal:' + bytes(rng.randrange(1, 256) for _ in range(
            rng.randrange(0, 80))) for _ in range(rng.randrange(0, 16))]
        # GNU treats some initial '-' strings as options; prefix a literal.
        check([b'fixture'] + operands,
              b'fixture' + (b' ' if operands else b'')
              + b' '.join(operands) + b'\n')
    try:
        subprocess.run([PROGRAM, b'a\0b'], env=ENV)
    except ValueError:
        COUNT += 1
    else:
        raise AssertionError('OS argv accepted embedded NUL')

if not A.reference and A.slice in ('parser', 'all'):
    help_result = subprocess.run(
        [PROGRAM, b'--help'], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env=ENV, timeout=10)
    help_bytes = help_result.stdout
    assert help_result.returncode == 0 and not help_result.stderr
    assert b'Usage: echo [-h] [TEXT]...\n' in help_bytes
    assert b'Print literal' in help_bytes and b'-h' in help_bytes
    # Host parser mode must not turn flags after operands into literal output.
    targets = [[PROGRAM]]
    if A.main:
        targets.append([os.fsencode(Path(A.main).resolve()), b'echo'])
    for environment in (ENV, {**ENV, b'POSIXLY_CORRECT': b'1'}):
        for target in targets:
            for args, expected, status in (
                    ([b'a', b'--help'], help_bytes, 0),
                    ([b'a', b'--bogus'], b'', 2),
                    ([b'--', b'a', b'--help'], b'a --help\n', 0),
                    ([b'', b'--', b'-h', b'a'], b' -h a\n', 0),
                    ([b'-', b'a', b'--', b'-n'], b'- a -n\n', 0),
                    ([b'-h', b'a', b'--', b'--bogus'], help_bytes, 0)):
                r = subprocess.run(target + args, env=environment,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, timeout=10)
                assert (r.returncode, r.stdout) == (status, expected), (
                    'host-parser-mode', args, r.returncode, r.stdout)
                assert bool(r.stderr) == (status != 0)
                COUNT += 1
    for args in ([b'-h'], [b'a', b'--help'], [b'--help', b'a', b'b']):
        check(args, help_bytes)
    for args in ([b'-n'], [b'-e'], [b'-E'], [b'--json'], [b'--version'],
                 [b'x', b'--bogus'], [b'--help', b'--bogus'],
                 [b'-h', b'--help']):
        check(args, b'', 2, True)
    for args, expected in [([b'--'], b'\n'),
                           ([b'--', b'-n', b'--help'], b'-n --help\n'),
                           ([b'--', b'--', b''], b'-- \n')]:
        check(args, expected)
    for args in ([], [b'--help']):
        with open('/dev/full', 'wb') as out:
            r = subprocess.run([PROGRAM] + args, stdout=out,
                               stderr=subprocess.PIPE, env=ENV, timeout=10)
        assert r.returncode == 1 and r.stderr
        COUNT += 1
    if A.main:
        main = [os.fsencode(Path(A.main).resolve())]
        for args, expected in COMMON:
            check(args, expected, prefix=main + [b'echo'])
        check([b'echo'], help_bytes, prefix=main + [b'help'])
        check([b'--help'], help_bytes, prefix=main + [b'echo'])
        check([b'--json'], b'', 2, True, main + [b'echo'])
        discovery = subprocess.run(main + [b'--help'], env=ENV,
                                   stdout=subprocess.PIPE, timeout=10)
        assert discovery.returncode == 0
        assert b'echo' in discovery.stdout and b'rmdir' not in discovery.stdout
        script = b"echo '' 'a b'\necho -- -n --help\necho --bogus\necho done\nexit\n"
        r = subprocess.run(main, input=script, env=ENV,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           timeout=10)
        assert r.returncode == 0 and r.stdout == b' a b\n-n --help\ndone\n'
        assert r.stderr and b'echo' in r.stderr
        r = subprocess.run(main, input=b'echo a\0b\necho recovered\nexit\n',
                           env=ENV, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, timeout=10)
        assert r.stdout == b'recovered\n' and r.returncode == 0 and r.stderr
        r = subprocess.run(main, input=(b"echo '' 'two words'\n"
                           b'echo -- -n --help\necho --bogus\n'
                           b'help echo\necho recovered\n') * 100 + b'exit\n',
                           env=ENV, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, timeout=10)
        assert r.returncode == 0
        assert r.stdout == (b' two words\n-n --help\n' + help_bytes
                            + b'recovered\n') * 100
        assert r.stderr.count(b'echo:') == 100
        COUNT += 1
        # Both input/output are one child-owned PTY; disable echo/CR translation.
        master, slave = pty.openpty()
        attrs = termios.tcgetattr(slave)
        attrs[1] &= ~termios.OPOST
        attrs[3] &= ~termios.ECHO
        termios.tcsetattr(slave, termios.TCSANOW, attrs)
        def own_terminal():
            os.setsid()
            fcntl.ioctl(slave, termios.TIOCSCTTY, 0)

        proc = subprocess.Popen(main, stdin=slave, stdout=slave, stderr=slave,
                                env=ENV, close_fds=True,
                                preexec_fn=own_terminal)
        os.close(slave)
        os.write(master, b'echo first\necho --help\necho -- -n\necho last\nexit\n')
        stream = b''
        deadline = time.monotonic() + 10
        try:
            while time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]:
                    try:
                        data = os.read(master, 65536)
                    except OSError as exc:
                        if exc.errno == errno.EIO:
                            break
                        raise
                    if not data:
                        break
                    stream += data
            assert proc.wait(timeout=2) == 0
            assert stream == (b'main> first\nmain> ' + help_bytes
                              + b'main> -n\nmain> last\nmain> '), stream
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            os.close(master)
        COUNT += 6
print('echo:', COUNT, 'checks passed;', 'reference' if A.reference else 'project')
