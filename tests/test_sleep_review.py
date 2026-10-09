#!/usr/bin/env python3
"""Independent selected-grammar, exact request, timing and hang oracles."""
import argparse
from fractions import Fraction
import itertools
import os
from pathlib import Path
import random
import re
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('root', help='Checkout containing built standalone/main/module test')
parser.add_argument('--mutations', action='store_true',
                    help='Compile five defect controls in private temporary storage')
args = parser.parse_args()
root = Path(args.root).resolve()
env = {'PATH': '/usr/bin:/bin', 'HOME': '/nonexistent', 'LC_ALL': 'C',
       'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1',
       'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
if os.environ.get('TMPDIR'):
    env['TMPDIR'] = os.environ['TMPDIR']

values = {''.join(x) for n in range(5)
          for x in itertools.product('019.+-e ', repeat=n)}
rng = random.Random(929357)
for digits in range(1, 10):
    for _ in range(200):
        whole = rng.choice([0, 1, 2147483646, rng.randrange(2147483647)])
        field = rng.randrange(10**digits)
        values.add(str(whole) + '.' + str(field).zfill(digits))
values.update(['NaN', 'nan', 'INF', 'Inf', 'inf', 'Infinity', '0s', '0m',
               '0h', '0d', '1e-9', '0x0', '0.1234567890', '0.١', '１２',
               '\t0', '0\t', '-0', '-1', '+1', '1.', '0.1junk',
               '2147483647.000000000', '2147483647.000000001',
               '2147483648', '9'*1000])
for count in range(20, 35):
    values.update(['0'*count, '0'*count+'1', '0'*count+'.1'])
records = []
valid = invalid = 0
for token in sorted(values):
    accepted = (len(token.encode()) <= 32 and
                re.fullmatch(r'[0-9]+(?:\.[0-9]{1,9})?', token) is not None)
    duration = Fraction(token) if accepted else Fraction(0)
    accepted = accepted and duration <= 2147483647
    if accepted:
        valid += 1
    else:
        invalid += 1
    expectation = '0 ' + str(int(duration*10**9)) if accepted else '2 0'
    records.append(token + '|' + expectation + '\n')
p = subprocess.run([str(root/'build/test_sleep_review_module')],
                   input=''.join(records).encode(), env=env,
                   capture_output=True, timeout=5)
assert p.returncode == 0 and not p.stdout, (p.returncode, p.stdout, p.stderr)
print(p.stderr.decode(), end='')
print('Independent grammar fixtures:', valid, 'valid;', invalid, 'invalid')

# Formerly hung inside dependency getopt without advancing optind. Exercise
# standalone and the real shell entrypoint, including errors before help.
for binary, prefix in [('sleep', []), ('main', ['sleep'])]:
    for token in ['- -', '-  -', '-h-', '-hh-', '-\t-', '-\n-', '-h-help']:
        for posix in [False, True]:
            selected = env | ({'POSIXLY_CORRECT': '1'} if posix else {})
            p = subprocess.run([str(root/'build'/binary), *prefix, token],
                               env=selected, capture_output=True, timeout=2)
            assert p.returncode == 2 and not p.stdout and p.stderr, (binary, token, p)
    for argv in [['--help', '-h-'], ['--', '-h-']]:
        p = subprocess.run([str(root/'build'/binary), *prefix, *argv],
                           env=env, capture_output=True, timeout=2)
        assert p.returncode == 2 and not p.stdout and p.stderr
print('Malformed short clusters: standalone/main/POSIX/help/-- passed')

# Independently specified, retained GNU-compatible arguments, silence/status
# and monotonic lower bounds. Never infer timing from comparing two commands.
fixtures = [(['0'], 0), (['000.000000000'], 0),
            (['0.000000001'], 1), (['0.000001'], 1000),
            (['0.002'], 2000000), (['--', '0.015'], 15000000)]
for program, prefix, label in [('/usr/bin/sleep', [], 'GNU'),
                             (str(root/'build/sleep'), [], 'standalone'),
                             (str(root/'build/main'), ['sleep'], 'main')]:
    for argv, lower in fixtures:
        before = time.monotonic_ns()
        p = subprocess.run([program, *prefix, *argv], env=env,
                           capture_output=True, timeout=3)
        elapsed = time.monotonic_ns() - before
        assert (p.returncode, p.stdout, p.stderr) == (0, b'', b''), (label, argv, p)
        assert lower <= elapsed < 3000000000, (label, argv, elapsed, lower)
        print(label, argv, 'actual_ns', elapsed, 'lower_ns', lower)
if args.mutations:
    import tempfile
    source = (root/'src/cmd_sleep.c').read_text()
    changes = {
        'precision': ('duration->tv_nsec = nanoseconds;',
                      'duration->tv_nsec = nanoseconds / 10;',
                      b'decimal exact precision'),
        'eintr': ('duration = remaining;', '(void)remaining;',
                  b'EINTR next request must equal returned remainder'),
        'getopt': ('optind = saved_optind;', '(void)saved_optind;',
                   b'exported getopt unchanged'),
        'argv': ('arg_parse(next, copy, table)', 'arg_parse(next, argv, table)',
                 b'borrowed argv unchanged'),
        'validation': ("if (*p != '\\0' || length > 32 ||", "if (length > 32 ||",
                       b'validation status'),
    }
    with tempfile.TemporaryDirectory(prefix='sleep-review-') as directory:
        private = Path(directory)
        for name, (old, new, marker) in changes.items():
            assert source.count(old) == 1, name
            mutated = private/(name+'.c')
            mutated.write_text(source.replace(old, new))
            binary = private/name
            compiled = subprocess.run([
                'cc', '-std=c17', '-O0', '-g', '-I'+str(root/'src'),
                '-I'+str(root/'build/argtable3/include'),
                str(root/'tests/test_sleep_review_module.c'), str(mutated),
                '-Wl,--wrap=arg_parse,--wrap=nanosleep',
                '-L'+str(root/'build/argtable3/lib'), '-largtable3', '-lm',
                '-o', str(binary)], env=env, capture_output=True, timeout=10)
            assert compiled.returncode == 0, compiled.stderr
            negative = subprocess.run([str(binary)], input=''.join(records).encode(),
                                      env=env, capture_output=True, timeout=5)
            assert negative.returncode == 90 and marker in negative.stderr, (
                name, negative.returncode, negative.stderr)
            print('Mutation', name, negative.returncode,
                  negative.stderr.decode().strip())
    p = subprocess.run([str(root/'build/test_sleep_review_module')],
                       input=''.join(records).encode(), env=env,
                       capture_output=True, timeout=5)
    assert p.returncode == 0, p.stderr
    print('Unchanged production recheck:', p.stderr.decode(), end='')
print('sleep independent review checks passed')
