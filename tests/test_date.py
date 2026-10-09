#!/usr/bin/env python3
"""Independent numeric datetime oracle and bounded real-clock observations."""
import argparse
import datetime as dt
import os
from pathlib import Path
import subprocess
import time

P = argparse.ArgumentParser()
P.add_argument('program')
P.add_argument('--reference', action='store_true')
P.add_argument('--slice', choices=['local', 'all'], default='all')
P.add_argument('--main')
A = P.parse_args()
PROGRAM = str(Path(A.program).resolve())
ENV = {'PATH': '/usr/bin:/bin', 'LC_ALL': 'C', 'TZ': 'UTC0'}
COUNT = 0


def current(prefix, args, utc=False, offset=0, env=None):
    global COUNT
    fmt = '+%Y-%m-%dT%H:%M:%S' + ('Z' if utc else '')
    if A.reference:
        args = args + [fmt]
    before = time.time_ns() // 1000000000
    r = subprocess.run(prefix + args, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, env=env or ENV, timeout=10)
    after = time.time_ns() // 1000000000
    assert before <= after and after - before < 10, 'clock changed/test too slow'
    expected = {(dt.datetime.fromtimestamp(s, dt.timezone.utc) +
                 dt.timedelta(seconds=offset)).isoformat(timespec='seconds')[:19]
                .encode() + (b'Z\n' if utc else b'\n')
                for s in range(before, after + 1)}
    assert (r.returncode == 0 and not r.stderr and r.stdout in expected), (
        'DATE_CURRENT', args, r.returncode, r.stdout, r.stderr, expected)
    COUNT += 1


current([PROGRAM], [])
if A.reference and A.slice == 'all':
    # GNU-only fixed-clock adapter: -d is NOT a supported project operand.
    fixtures = [(0, 'UTC0', [], b'1970-01-01T00:00:00\n'),
                (-1, 'UTC0', ['-u'], b'1969-12-31T23:59:59Z\n'),
                (951827696, 'XXX-5:30', ['--utc'], b'2000-02-29T12:34:56Z\n'),
                (0, 'XXX-5:30', [], b'1970-01-01T05:30:00\n'),
                (1710053999, 'EST5EDT,M3.2.0/2,M11.1.0/2', [],
                 b'2024-03-10T01:59:59\n'),
                (1710054000, 'EST5EDT,M3.2.0/2,M11.1.0/2', [],
                 b'2024-03-10T03:00:00\n'),
                (1730613599, 'EST5EDT,M3.2.0/2,M11.1.0/2', [],
                 b'2024-11-03T01:59:59\n'),
                (1730613600, 'EST5EDT,M3.2.0/2,M11.1.0/2', [],
                 b'2024-11-03T01:00:00\n')]
    for epoch, tz, flags, expected in fixtures:
        fmt = '+%Y-%m-%dT%H:%M:%S' + ('Z' if flags else '')
        r = subprocess.run([PROGRAM, '--date=@' + str(epoch), *flags, fmt],
                           env=dict(ENV, TZ=tz), capture_output=True, timeout=10)
        assert (r.returncode, r.stdout, r.stderr) == (0, expected, b''), (
            'DATE_REFERENCE_FIXED', epoch, tz, r)
        COUNT += 1
if A.slice == 'all':
    for args in [ ['-u'], ['--utc'], ['--universal'], ['-uu'] ]:
        current([PROGRAM], args, utc=True)
    for tz, offset in [('UTC0', 0), ('XXX-5:30', 19800), ('XXX3', -10800)]:
        for locale in ['C', 'C.UTF-8']:
            env = dict(ENV, TZ=tz, LC_ALL=locale)
            current([PROGRAM], [], offset=offset, env=env)
            current([PROGRAM], ['-u'], utc=True, env=env)
    if not A.reference:
        for posix in [False, True]:
            env = dict(ENV)
            if posix:
                env['POSIXLY_CORRECT'] = '1'
            current([PROGRAM], ['--'], env=env)
            for args in [['--bogus'], ['+%s'], ['--json'], ['-d', 'now'],
                         ['-s', 'now'], ['01010000'], [''], ['-'], ['--', '-u'],
                         ['--help', 'x'], ['x', '--help'], ['--help', '--bogus'],
                         ['--utc=x'], ['--utc', 'x']]:
                r = subprocess.run([PROGRAM] + args, env=env,
                                   capture_output=True, timeout=10)
                assert r.returncode == 2 and not r.stdout and r.stderr, (
                    'DATE_INVALID', args, r)
                COUNT += 1
        help_bytes = None
        for args in [['-h'], ['--help'], ['-uh'], ['--universal', '--help']]:
            r = subprocess.run([PROGRAM] + args, env=ENV,
                               capture_output=True, timeout=10)
            assert r.returncode == 0 and not r.stderr
            assert all(token in r.stdout for token in [b'Usage: date', b'--help',
                       b'--utc', b'--universal', b'YYYY-MM-DDTHH:MM:SS', b'local'])
            if help_bytes is None:
                help_bytes = r.stdout
            assert r.stdout == help_bytes
            COUNT += 1
        for args in [[], ['-u'], ['--help']]:
            with open('/dev/full', 'wb') as full:
                r = subprocess.run([PROGRAM] + args, env=ENV, stdout=full,
                                   stderr=subprocess.PIPE, timeout=10)
            assert r.returncode == 1 and r.stderr, ('DATE_FULL', args, r)
            COUNT += 1
        if A.main:
            main = str(Path(A.main).resolve())
            current([main, 'date'], [])
            current([main, 'date'], ['--utc'], utc=True)
            for args in [['date', '--help'], ['help', 'date']]:
                r = subprocess.run([main] + args, env=ENV,
                                   capture_output=True, timeout=10)
                assert (r.returncode, r.stdout, r.stderr) == (0, help_bytes, b'')
                COUNT += 1
            r = subprocess.run([main, '--help'], env=ENV,
                               capture_output=True, timeout=10)
            assert b'date' in r.stdout and b'Show system time.' in r.stdout
            COUNT += 1
            # Repeated real-main loop: timestamps bracketed by real clock.
            for terminal in [False, True]:
                script = (b'date\ndate -u\n' * 40 +
                          b'date --bogus\nhelp date\ndate -u\nexit\n')
                before = time.time_ns() // 1000000000
                if terminal:
                    import pty
                    import select
                    import termios
                    master, slave = pty.openpty()
                    settings = termios.tcgetattr(slave)
                    settings[3] &= ~termios.ECHO
                    settings[1] &= ~termios.OPOST
                    termios.tcsetattr(slave, termios.TCSANOW, settings)
                    proc = subprocess.Popen([main], stdin=slave, stdout=slave,
                                            stderr=subprocess.PIPE, env=ENV)
                    os.close(slave)
                    try:
                        os.write(master, script)
                        output = b''
                        deadline = time.monotonic() + 10
                        while time.monotonic() < deadline:
                            ready, _, _ = select.select([master], [], [], 0.05)
                            if ready:
                                try:
                                    part = os.read(master, 4096)
                                except OSError as exc:
                                    if exc.errno != 5:
                                        raise
                                    break
                                if not part:
                                    break
                                output += part
                            elif proc.poll() is not None:
                                break
                        _, error = proc.communicate(timeout=2)
                        status = proc.returncode
                    finally:
                        if proc.poll() is None:
                            proc.kill()
                            proc.communicate()
                        os.close(master)
                else:
                    r = subprocess.run([main], input=script, env=ENV,
                                       capture_output=True, timeout=10)
                    output, error, status = r.stdout, r.stderr, r.returncode
                after = time.time_ns() // 1000000000
                bad = subprocess.run([main, 'date', '--bogus'], env=ENV,
                                     capture_output=True, timeout=10)
                expected_error = (b'main> ' * 81 + bad.stderr +
                                  b'main> ' * 3) if terminal else bad.stderr
                assert (status, error) == (0, expected_error), (
                    'DATE_MAIN_LOOP_STATUS_PROMPTS', status, error)
                date_size = 40 * (20 + 21)
                lines = output[:date_size].split(b'\n')[:-1]
                assert len(lines) == 80 and output[date_size:].startswith(help_bytes), (
                    'DATE_LOOP_LAYOUT', output)
                final = output[date_size + len(help_bytes):]
                samples = lines + [final.rstrip(b'\n')]
                assert before <= after and after - before < 10
                for i, line in enumerate(samples):
                    utc = i % 2 == 1 or i == 80
                    expected = {dt.datetime.fromtimestamp(s, dt.timezone.utc)
                                .isoformat(timespec='seconds')[:19].encode() +
                                (b'Z' if utc else b'')
                                for s in range(before, after + 1)}
                    assert line in expected, ('DATE_LOOP_TIME', i, line, expected)
                COUNT += 1
print('date CLI:', COUNT, 'independent bounded-clock/parser/output checks passed')
