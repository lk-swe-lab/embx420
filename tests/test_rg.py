#!/usr/bin/env python3
"""Independent rg byte fixtures; explicit disposable paths only."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('program')
parser.add_argument('--reference', action='store_true')
parser.add_argument('--slice', type=int, default=99)
args = parser.parse_args()
PROGRAM = str(Path(args.program).resolve())


class RgTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'])
        self.addCleanup(self.temp.cleanup)
        self.cwd = Path(self.temp.name)
        (self.cwd / 'f').write_bytes(b'alpha\nBeta alpha\nnone\nalpha')

    def run_rg(self, options):
        prefix = ['--no-config', '--color=never', '--no-heading', '-H'] if args.reference else []
        return subprocess.run([PROGRAM, *prefix, *options], cwd=self.cwd,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env={**os.environ, 'LC_ALL': 'C'}, timeout=5)

    def check(self, options, out=b'', status=0):
        result = self.run_rg(options)
        self.assertEqual((result.returncode, result.stdout, result.stderr),
                         (status, out, b''))

    def test_01_explicit_ere_line_numbers(self):
        self.check(['-n', '^alpha$', 'f'], b'f:1:alpha\nf:4:alpha\n')
        self.check(['(Beta|none)', 'f'], b'f:Beta alpha\nf:none\n')
        self.check(['absent', 'f'], status=1)
        before = (self.cwd / 'f').stat()
        self.check(['alpha', 'f'], b'f:alpha\nf:Beta alpha\nf:alpha\n')
        after = (self.cwd / 'f').stat()
        self.assertEqual((before.st_ino, before.st_mode, before.st_mtime_ns),
                         (after.st_ino, after.st_mode, after.st_mtime_ns))
        self.assertEqual((self.cwd / 'f').read_bytes(), b'alpha\nBeta alpha\nnone\nalpha')

    def test_02_literal_case_and_word(self):
        (self.cwd / 'f').write_bytes(b'Alpha alpha2 _alpha alpha!\na.b aXb\n')
        self.check(['-i', '-w', 'alpha', 'f'], b'f:Alpha alpha2 _alpha alpha!\n')
        self.check(['--fixed-strings', 'a.b', 'f'], b'f:a.b aXb\n')
        self.check(['-i', '--fixed-strings', 'ALPHA', 'f'],
                   b'f:Alpha alpha2 _alpha alpha!\n')
        self.check(['-w', 'alpha2', 'f'], b'f:Alpha alpha2 _alpha alpha!\n')
        self.check(['-w', '_alp', 'f'], status=1)
        (self.cwd / 'f').write_bytes(b'Xab ab! cab\n')
        self.check(['-w', 'a|ab', 'f'], b'f:Xab ab! cab\n')

    @unittest.skipIf(args.reference, 'project JSON schema and POSIX matching')
    def test_03_json_matches_bytes_empty_and_overlap(self):
        (self.cwd / 'f').write_bytes('é aaa\n"\\\t\r\n'.encode())
        result = self.run_rg(['--json', 'aa', 'f'])
        self.assertEqual((result.returncode, result.stderr), (0, b''))
        self.assertEqual(result.stdout,
                         b'{"file":"f","line":1,"column":4,"index":3,"text":"\xc3\xa9 aaa"}\n')
        (self.cwd / 'f').write_bytes(b'aba aba\n\n')
        result = self.run_rg(['--json', 'aba', 'f'])
        self.assertEqual([json.loads(x) for x in result.stdout.splitlines()], [
            dict(file='f', line=1, column=1, index=0, text='aba aba'),
            dict(file='f', line=1, column=5, index=4, text='aba aba')])
        (self.cwd / 'f').write_bytes(b'ab\n\n')
        for options in (['--json', ''], ['--json', '--fixed-strings', '']):
            result = self.run_rg([*options, 'f'])
            self.assertEqual((result.returncode, result.stderr), (0, b''))
            self.assertEqual([json.loads(x) for x in result.stdout.splitlines()], [
                dict(file='f', line=1, column=1, index=0, text='ab'),
                dict(file='f', line=1, column=2, index=1, text='ab'),
                dict(file='f', line=1, column=3, index=2, text='ab'),
                dict(file='f', line=2, column=1, index=3, text='')])
        result = self.run_rg(['--json', '^|$', 'f'])
        self.assertEqual([json.loads(x)['column'] for x in result.stdout.splitlines()], [1, 3, 1])
        (self.cwd / 'f').write_bytes(b'a ab\n')
        result = self.run_rg(['--json', 'a|ab', 'f'])
        self.assertEqual([json.loads(x)['column'] for x in result.stdout.splitlines()], [1, 3])
        (self.cwd / 'f').write_bytes(b'"\\\t\r\n')
        self.check(['--json', '.', 'f'],
                   b'{"file":"f","line":1,"column":1,"index":0,"text":"\\"\\\\\\t\\r"}\n'
                   b'{"file":"f","line":1,"column":2,"index":1,"text":"\\"\\\\\\t\\r"}\n'
                   b'{"file":"f","line":1,"column":3,"index":2,"text":"\\"\\\\\\t\\r"}\n'
                   b'{"file":"f","line":1,"column":4,"index":3,"text":"\\"\\\\\\t\\r"}\n')
        (self.cwd / 'f').write_bytes(b'')
        self.check(['--json', '', 'f'], status=1)

    def test_04_context_union_groups(self):
        (self.cwd / 'f').write_bytes(b'a\nx\nb\nx\nc\nd\ne\nf\nx\ng\n')
        self.check(['-n', '-C', '1', '^x$', 'f'],
                   b'f-1-a\nf:2:x\nf-3-b\nf:4:x\nf-5-c\n--\n'
                   b'f-8-f\nf:9:x\nf-10-g\n')
        self.check(['-C', '0', '^x$', 'f'], b'f:x\nf:x\nf:x\n')
        self.check(['-C', '100', '^x$', 'f'],
                   b'f-a\nf:x\nf-b\nf:x\nf-c\nf-d\nf-e\nf-f\nf:x\nf-g\n')
        if not args.reference:
            one = self.run_rg(['--json', '^x$', 'f'])
            two = self.run_rg(['--json', '-n', '-C', '1', '^x$', 'f'])
            self.assertEqual((one.returncode, one.stdout, one.stderr),
                             (two.returncode, two.stdout, two.stderr))

    @unittest.skipIf(args.reference, 'project traversal and binary policy')
    def test_05_recursive_order_binary_and_links(self):
        (self.cwd / 'tree').mkdir()
        (self.cwd / 'tree' / 'z').write_bytes(b'hit\n')
        (self.cwd / 'tree' / 'a').mkdir()
        (self.cwd / 'tree' / 'a' / 'x').write_bytes(b'hit\n')
        (self.cwd / 'tree' / '.hidden').write_bytes(b'hit\n')
        (self.cwd / 'tree' / 'binary').write_bytes(b'hit\n' + b'x' * 9000 + b'\0')
        (self.cwd / 'tree' / 'loop').symlink_to('.')
        (self.cwd / 'tree' / 'alias').symlink_to('z')
        os.mkfifo(self.cwd / 'tree' / 'fifo')
        self.check(['hit', 'tree'],
                   b'tree/.hidden:hit\ntree/a/x:hit\ntree/z:hit\n')
        self.check(['hit', 'tree/binary'], status=1)
        self.check(['hit', 'tree/z', 'tree/z'], b'tree/z:hit\ntree/z:hit\n')
        for path in ('tree/loop', 'tree/loop/', 'tree/alias', 'tree/fifo'):
            result = self.run_rg(['hit', path])
            self.assertEqual((result.returncode, result.stdout), (2, b''))
            self.assertTrue(result.stderr.startswith(b'rg:'))
        self.assertTrue((self.cwd / 'tree/alias').is_symlink())
        self.assertEqual((self.cwd / 'tree/z').read_bytes(), b'hit\n')
        self.assertEqual(sorted(p.name for p in (self.cwd / 'tree').iterdir()),
                         ['.hidden', 'a', 'alias', 'binary', 'fifo', 'loop', 'z'])

    @unittest.skipIf(args.reference, 'project status, encoding and limits')
    def test_06_errors_encoding_and_limits(self):
        for options in ([], ['alpha'], ['[', 'f'], ['-C', '-1', 'alpha', 'f'],
                        ['-C', 'junk', 'alpha', 'f'], ['-C', '2147483648', 'alpha', 'f'],
                        ['-n', '-n', 'alpha', 'f'], ['--bad'],
                        ['--help', '--bad'], ['a\nb', 'f'], ['alpha', '']):
            with self.subTest(options=options):
                result = self.run_rg(options)
                self.assertEqual((result.returncode, result.stdout), (2, b''))
                self.assertTrue(result.stderr)
        for path in ('missing', 'missing/child'):
            result = self.run_rg(['alpha', path, 'f'])
            self.assertEqual((result.returncode, result.stdout),
                             (2, b'f:alpha\nf:Beta alpha\nf:alpha\n'))
            self.assertTrue(result.stderr)
        (self.cwd / 'f').write_bytes(b'alpha\n\xff\n')
        result = self.run_rg(['--json', 'alpha', 'f'])
        self.assertEqual((result.returncode, result.stdout), (2, b''))
        self.assertTrue(result.stderr)
        self.check(['alpha', 'f'], b'f:alpha\n')
        (self.cwd / 'f').write_bytes(b'alpha\n\0\xff')
        self.check(['--json', 'alpha', 'f'], status=1)
        with (self.cwd / 'big').open('wb') as file:
            file.truncate(64 * 1024 * 1024 + 1)
        result = self.run_rg(['x', 'big'])
        self.assertEqual((result.returncode, result.stdout), (2, b''))
        self.assertTrue(result.stderr)
        with (self.cwd / 'f').open('wb') as file:
            file.write(b'x' * 20000 + b'MARK\r')
        self.check(['MARK$', 'f'], status=1)
        self.check(['MARK\r$', 'f'], b'f:' + b'x' * 20000 + b'MARK\r\n')
        (self.cwd / '-').write_bytes(b'-hit\n')
        self.check(['--fixed-strings', '--', '-hit', '-'], b'-:-hit\n')
        with open('/dev/full', 'wb') as full:
            result = subprocess.run([PROGRAM, 'x', 'f'], cwd=self.cwd,
                                    stdout=full, stderr=subprocess.PIPE, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertTrue(result.stderr)

    @unittest.skipIf(args.reference, 'project decimal context contract')
    def test_07_context_decimal_only(self):
        for value in ('0x1', '01k', '1k', '+1', ' 1', '2147483648'):
            result = self.run_rg(['-C', value, 'alpha', 'f'])
            self.assertEqual((result.returncode, result.stdout), (2, b''))
            self.assertTrue(result.stderr)
        self.check(['-C', '000', 'alpha', 'f'],
                   b'f:alpha\nf:Beta alpha\nf:alpha\n')

    @unittest.skipIf(args.reference, 'project byte matching and recursion limits')
    def test_08_boundaries_names_help_and_depth(self):
        first = self.run_rg(['--help'])
        second = self.run_rg(['--help', 'unused', 'unopened'])
        self.assertEqual((first.returncode, first.stdout, first.stderr),
                         (second.returncode, second.stdout, second.stderr))
        self.assertEqual((first.returncode, first.stderr), (0, b''))
        for flag in (b'-n', b'-i', b'-w', b'-C', b'--fixed-strings', b'--json'):
            self.assertIn(flag, first.stdout)
        for pattern in ('[', '(', '[z-a]', 'a{2,1}'):
            result = self.run_rg([pattern, 'f'])
            self.assertEqual((result.returncode, result.stdout), (2, b''))
            self.assertTrue(result.stderr)
        (self.cwd / 'f').write_bytes(b'aaa')
        result = self.run_rg(['--json', 'a|aa', 'f'])
        self.assertEqual([json.loads(x)['column'] for x in result.stdout.splitlines()], [1, 3])
        (self.cwd / 'f').write_bytes('éÉ é_a é!'.encode())
        self.check(['-i', '--fixed-strings', 'É', 'f'], 'f:éÉ é_a é!\n'.encode())
        self.check(['-w', '--fixed-strings', 'é', 'f'], 'f:éÉ é_a é!\n'.encode())
        badname = os.fsdecode(b'bad\xff')
        (self.cwd / badname).write_bytes(b'hit\n')
        result = self.run_rg(['--json', 'hit', badname])
        self.assertEqual((result.returncode, result.stdout), (2, b''))
        self.check(['hit', badname], b'bad\xff:hit\n')
        (self.cwd / 'empty').mkdir()
        self.check(['hit', 'empty'], status=1)
        current = self.cwd / 'deep'
        current.mkdir()
        for _ in range(65):
            current = current / 'd'
            current.mkdir()
        result = self.run_rg(['hit', 'deep'])
        self.assertEqual((result.returncode, result.stdout), (2, b''))
        self.assertTrue(result.stderr)
        with (self.cwd / 'edge').open('wb') as file:
            file.truncate(64 * 1024 * 1024)
        self.check(['hit', 'edge'], status=1)

    @unittest.skipIf(args.reference, 'independent fixed-string JSON property')
    def test_09_seeded_fixed_string_properties(self):
        import random
        rng = random.Random(7341)
        alphabet = b'abAB._ '
        for _ in range(120):
            lines = [bytes(rng.choice(alphabet) for _ in range(rng.randrange(30)))
                     for _ in range(rng.randrange(1, 8))]
            pattern = bytes(rng.choice(alphabet) for _ in range(rng.randrange(1, 4)))
            ignore_case, word = rng.choice([False, True]), rng.choice([False, True])
            payload = b'\n'.join(lines) + b'\n'
            (self.cwd / 'f').write_bytes(payload)
            expected = []
            offset = 0
            for number, line in enumerate(lines, 1):
                pos = 0
                subject = line.lower() if ignore_case else line
                needle = pattern.lower() if ignore_case else pattern
                while True:
                    start = subject.find(needle, pos)
                    if start < 0:
                        break
                    end = start + len(pattern)
                    wordbytes = b'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_'
                    if word and ((start and line[start - 1] in wordbytes)
                                 or (end < len(line) and line[end] in wordbytes)):
                        pos = start + 1
                        continue
                    expected.append(dict(file='f', line=number, column=start + 1,
                                         index=offset + start, text=line.decode()))
                    pos = end
                offset += len(line) + 1
            options = ['--json', '--fixed-strings']
            options += ['-i'] if ignore_case else []
            options += ['-w'] if word else []
            result = self.run_rg([*options, pattern.decode(), 'f'])
            self.assertEqual((result.returncode, result.stderr),
                             (0 if expected else 1, b''))
            self.assertEqual([json.loads(x) for x in result.stdout.splitlines()], expected)
            self.assertEqual((self.cwd / 'f').read_bytes(), payload)

    @unittest.skipIf(args.reference, 'project help and inherited SIGPIPE policy')
    def test_10_help_and_broken_output(self):
        with open('/dev/full', 'wb') as full:
            result = subprocess.run([PROGRAM, '--help'], cwd=self.cwd,
                                    stdout=full, stderr=subprocess.PIPE, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertTrue(result.stderr)
        import signal
        reader, writer = os.pipe()
        os.close(reader)
        try:
            result = subprocess.run([PROGRAM, 'alpha', 'f'], cwd=self.cwd,
                                    stdout=writer, stderr=subprocess.PIPE, timeout=5,
                                    preexec_fn=lambda: signal.signal(signal.SIGPIPE, signal.SIG_IGN))
            self.assertEqual(result.returncode, 2)
            self.assertTrue(result.stderr)
        finally:
            os.close(writer)


if __name__ == '__main__':
    suite = unittest.TestSuite()
    for name in unittest.defaultTestLoader.getTestCaseNames(RgTests):
        if int(name.split('_')[1]) <= args.slice:
            suite.addTest(RgTests(name))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(not result.wasSuccessful())
