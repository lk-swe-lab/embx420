#!/usr/bin/env python3
"""Independent short-interval contract checks, no long sleeps."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('program')
parser.add_argument('--reference', action='store_true')
parser.add_argument('--slice', type=int, default=4)
parser.add_argument('--main')
args = parser.parse_args()
program = str(Path(args.program).resolve())
env = {'PATH': '/usr/bin:/bin', 'LC_ALL': 'C', 'HOME': '/nonexistent'}

def invoke(argv, prefix=None, data=b'', extra=None):
    selected = env | (extra or {})
    return subprocess.run([*(prefix or [program]), *argv], input=data,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          env=selected, timeout=5)

def silent(argv, lower=0, prefix=None):
    start = time.monotonic_ns()
    p = invoke(argv, prefix)
    elapsed = time.monotonic_ns() - start
    assert (p.returncode, p.stdout, p.stderr) == (0, b'', b''), (argv, p)
    assert elapsed >= lower, ('sleep interval too short', argv, elapsed, lower)
    assert elapsed < 5_000_000_000
    print('interval', argv, elapsed, 'ns; lower bound', lower)

silent(['0'])
silent(['000'])
if args.slice >= 2:
    for argv, lower in [(['0.020'], 20_000_000), (['0.000000001'], 1),
                        (['000.000000000'], 0), (['--', '0.001'], 1_000_000)]:
        silent(argv, lower)
if not args.reference:
    bad = [[], [''], ['-1'], ['--wat'], ['0', '0']]
    if args.slice >= 2:
        bad += [[x] for x in ['NaN', 'nan', 'Inf', 'inf', '+0', '-0', '.1', '1.',
                             '0.0000000001', '0s', '0m', '1e-2', '0x0', '0,0',
                             ' 0', '0 ', '0x', '.', '0.1garbage', '0.٠', '１２', '0\t', '2147483648',
                             '2147483647.000000001', '9'*1000, '0'*33]]
        bad += [['0', '--wat'], ['--', '--help'], ['--help', '--wat']]
    for argv in bad:
        p = invoke(argv)
        assert p.returncode == 2 and p.stdout == b'' and p.stderr, (argv, p)
    for extra in [{}, {'POSIXLY_CORRECT': '1'}]:
        p = invoke(['0', '--wat'], extra=extra)
        assert p.returncode == 2 and not p.stdout and p.stderr
    help_results = [invoke(x) for x in [['-h'], ['--help'], ['0', '--help']]]
    for p in help_results:
        assert p.returncode == 0 and not p.stderr and b'Usage: sleep' in p.stdout
        assert b'2147483647' in p.stdout and b'EINTR' in p.stdout
    assert len({p.stdout for p in help_results}) == 1
    if args.main:
        main = str(Path(args.main).resolve())
        for argv in [['0'], ['--help'], ['bad'], ['--', '0.002']]:
            standalone = invoke(argv)
            dispatched = invoke(argv, [main, 'sleep'])
            assert (dispatched.returncode, dispatched.stdout, dispatched.stderr) == (
                standalone.returncode, standalone.stdout, standalone.stderr)
        p = invoke(['help', 'sleep'], [main])
        assert p.stdout == help_results[0].stdout and p.returncode == 0
        p = invoke([], [main], b'sleep 0\nsleep bad\nhelp sleep\nsleep 0.002\n'
                   b'sleep --help\necho alive\nexit\n')
        assert p.returncode == 0 and p.stdout == help_results[0].stdout*2+b'alive\n'
        assert p.stderr == b'sleep: invalid duration\n'
        script = b'sleep 0\nsleep bad\nsleep --help\n'*100 + b'echo alive\nexit\n'
        repeated = invoke([], [main], script)
        assert (repeated.returncode, repeated.stdout, repeated.stderr) == (
            0, help_results[0].stdout*100+b'alive\n',
            b'sleep: invalid duration\n'*100)
        p = invoke([], [main], b'sleep bad\nexit\n')
        assert p.returncode == 2 and p.stdout == b''
        silent(['sleep', '0.020'], 20_000_000, [main])

        import errno
        import fcntl
        import pty
        import select
        import termios
        master, slave = pty.openpty()
        settings = termios.tcgetattr(slave)
        settings[3] &= ~termios.ECHO
        termios.tcsetattr(slave, termios.TCSANOW, settings)
        def own_terminal():
            os.setsid()
            fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
        child = subprocess.Popen([main], stdin=slave, stdout=slave, stderr=slave,
                                 env=env, preexec_fn=own_terminal)
        os.close(slave)
        try:
            os.write(master, b'sleep 0\nsleep bad\nsleep 0.001\n'
                     b'help sleep\nsleep --help\necho alive\nexit\n')
            captured = b''
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                ready, _, _ = select.select([master], [], [], 0.05)
                if ready:
                    try:
                        chunk = os.read(master, 65536)
                    except OSError as error:
                        if error.errno == errno.EIO:
                            break
                        raise
                    if not chunk:
                        break
                    captured += chunk
                elif child.poll() is not None:
                    break
            assert child.wait(timeout=2) == 0
            usage = help_results[0].stdout.replace(b'\n', b'\r\n')
            expected = (b'main> main> sleep: invalid duration\r\nmain> '
                        b'main> ' + usage + b'main> ' + usage +
                        b'main> alive\r\nmain> ')
            assert captured == expected, (captured, expected)
        finally:
            if child.poll() is None:
                child.kill(); child.wait(timeout=2)
            os.close(master)
    if args.slice >= 4:
        # Output fault cannot be hidden by buffered help.
        with open('/dev/full', 'wb') as full:
            p = subprocess.run([program, '--help'], stdout=full,
                               stderr=subprocess.PIPE, env=env, timeout=5)
            assert p.returncode == 1 and p.stderr
        # Actual inherited SIGTERM and SIGINT cancellation, bounded by deadline.
        for sig in [signal.SIGTERM, signal.SIGINT]:
            p = subprocess.Popen([program, '0.5'], stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, env=env)
            try:
                # Signal as soon as exec is complete (Popen's exec error pipe).
                p.send_signal(sig)
                out, err = p.communicate(timeout=2)
                assert p.returncode == -sig and not out and not err, (sig, p.returncode)
            finally:
                if p.poll() is None:
                    p.kill(); p.communicate()
print('sleep CLI contract passed')
