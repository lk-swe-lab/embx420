#!/usr/bin/env python3
"""Independent touch fixtures; reference excludes project-only semantics."""
import argparse
import json
import os
import random
from pathlib import Path
import socket
import stat
import subprocess
import tempfile
import time
import unittest
from typing import BinaryIO

parser = argparse.ArgumentParser()
parser.add_argument('program')
parser.add_argument('--reference', action='store_true')
parser.add_argument('--case')
args = parser.parse_args()
PROGRAM = str(Path(args.program).resolve())
OLD = 946684800000000000

class TouchTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='touch-', dir=os.environ['TMPDIR'])
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def call(self, *operands, mask=0o027, stdout: int | BinaryIO = subprocess.PIPE):
        return subprocess.run([PROGRAM, *operands], cwd=self.root,
                              env={**os.environ, 'LC_ALL': 'C'},
                              stdout=stdout, stderr=subprocess.PIPE,
                              timeout=5, umask=mask)

    def ok(self, result):
        self.assertEqual((result.returncode, result.stdout, result.stderr),
                         (0, b'', b''))

    def recent(self, path, before, after):
        info = path.stat()
        # Accommodate filesystem flooring to one-second precision.
        for timestamp in (info.st_atime_ns, info.st_mtime_ns):
            self.assertGreaterEqual(timestamp, before - 1000000000)
            self.assertLessEqual(timestamp, after)
        return info

    def test_existing(self):
        target = self.root / 'binary'
        data = bytes(range(256)) * 17 + b'\x00tail'
        target.write_bytes(data)
        target.chmod(0o4510)
        os.utime(target, ns=(OLD, OLD + 123456789))
        prior = target.stat()
        before = time.time_ns()
        self.ok(self.call('binary'))
        after = time.time_ns()
        info = self.recent(target, before, after)
        self.assertEqual((info.st_ino, info.st_uid, info.st_gid, info.st_mode,
                          info.st_size),
                         (prior.st_ino, prior.st_uid, prior.st_gid,
                          prior.st_mode, len(data)))
        self.assertEqual(target.read_bytes(), data)
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), ['binary'])

    def test_creation_multi_partial(self):
        before = time.time_ns()
        self.ok(self.call('new', 'second'))
        after = time.time_ns()
        for name in ('new', 'second'):
            target = self.root / name
            info = self.recent(target, before, after)
            self.assertTrue(stat.S_ISREG(info.st_mode))
            self.assertEqual(stat.S_IMODE(info.st_mode), 0o640)
            self.assertEqual(target.read_bytes(), b'')
        result = self.call('first', 'absent/child', 'last')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b'')
        self.assertIn(b'absent/child', result.stderr)
        self.assertTrue((self.root / 'first').is_file())
        self.assertTrue((self.root / 'last').is_file())
        self.assertFalse((self.root / 'absent').exists())

    @unittest.skipIf(args.reference, 'project parsing contract')
    def test_arguments_help_dash(self):
        for operands in ((), ('--bad', 'forbidden'), ('forbidden', '--bad'),
                         ('forbidden', ''), ('--help', '--bad', 'forbidden'),
                         ('--json', '--json', 'forbidden')):
            result = self.call(*operands)
            self.assertEqual(result.returncode, 2, operands)
            self.assertEqual(result.stdout, b'')
            self.assertNotEqual(result.stderr, b'')
            self.assertEqual(list(self.root.iterdir()), [])
        help_output = None
        for operands in (('-h',), ('--help',), ('forbidden', '--help'),
                         ('--json', '--help', 'forbidden', 'other')):
            result = self.call(*operands)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stderr, b'')
            self.assertIn(b'Usage: touch', result.stdout)
            self.assertIn(b'--json', result.stdout)
            self.assertIn(b'FILE', result.stdout)
            self.assertIn(b'Create missing files empty.', result.stdout)
            self.assertIn(b'Update atime and mtime; existing contents remain '
                          b'unchanged.', result.stdout)
            if help_output is None:
                help_output = result.stdout
            self.assertEqual(result.stdout, help_output)
            self.assertEqual(list(self.root.iterdir()), [])
        self.ok(self.call('--', '-', '-file'))
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), ['-', '-file'])

    def test_metadata_targets(self):
        directory = self.root / 'dir'
        directory.mkdir()
        regular = self.root / 'regular'
        regular.write_bytes(b'preserve')
        os.link(regular, self.root / 'hard')
        os.symlink('regular', self.root / 'link')
        os.mkfifo(self.root / 'fifo')
        sock = socket.socket(socket.AF_UNIX)
        sock.bind(str(self.root / 'socket'))
        self.addCleanup(sock.close)
        names = ('dir', 'regular', 'hard', 'link', 'fifo', 'socket')
        for name in names:
            os.utime(self.root / name, ns=(OLD, OLD))
        os.utime(self.root / 'link', ns=(OLD, OLD), follow_symlinks=False)
        link_prior = (self.root / 'link').lstat()
        before = time.time_ns()
        self.ok(self.call(*names))
        after = time.time_ns()
        for name in names:
            self.recent(self.root / name, before, after)
        self.assertEqual(regular.read_bytes(), b'preserve')
        self.assertEqual((self.root / 'hard').stat().st_ino, regular.stat().st_ino)
        link_info = (self.root / 'link').lstat()
        # Kernel link traversal itself can update symlink atime (GNU too).
        self.assertEqual(link_info.st_mtime_ns, link_prior.st_mtime_ns)

    @unittest.skipIf(args.reference, 'dangling link deliberate difference')
    def test_dangling_loop_and_trailing_slash(self):
        os.symlink('missing', self.root / 'dangling')
        os.symlink('loop', self.root / 'loop')
        result = self.call('dangling', 'loop', 'slash/', 'later')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b'')
        for name in (b'dangling', b'loop', b'slash/'):
            self.assertIn(name, result.stderr)
        self.assertFalse((self.root / 'missing').exists())
        self.assertFalse((self.root / 'slash').exists())
        self.assertTrue((self.root / 'later').is_file())
        self.assertTrue((self.root / 'dangling').is_symlink())

    @unittest.skipIf(args.reference, 'project JSON contract')
    def test_json_order_escaping_errors(self):
        names = ['a"\\\n\t\x01', 'λ', 'last']
        result = self.call('--json', *names)
        self.assertEqual((result.returncode, result.stderr), (0, b''))
        expected = [{'path': name, 'status': 'ok', 'message': None}
                    for name in names]
        self.assertEqual(result.stdout,
                         b'[{"path":"a\\"\\\\\\n\\t\\u0001","status":"ok","message":null},'
                         + '{"path":"λ","status":"ok","message":null},'.encode()
                         + b'{"path":"last","status":"ok","message":null}]\n')
        self.assertEqual(json.loads(result.stdout), expected)
        result = self.call('--json', 'one', 'absent/no', 'two')
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'absent/no', result.stderr)
        self.assertEqual(json.loads(result.stdout), [
            {'path': 'one', 'status': 'ok', 'message': None},
            {'path': 'absent/no', 'status': 'error',
             'message': os.strerror(2)},
            {'path': 'two', 'status': 'ok', 'message': None}])

    @unittest.skipIf(args.reference, 'project UTF-8 preflight')
    def test_json_utf8_before_effects(self):
        result = self.call('--json', 'forbidden', b'bad-\xff')
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, b'')
        self.assertNotEqual(result.stderr, b'')
        self.assertEqual(list(self.root.iterdir()), [])
        self.ok(self.call(b'raw-\xff'))
        self.assertTrue(os.path.isfile(os.fsencode(self.root) + b'/raw-\xff'))

    def test_permissions_umask(self):
        target = self.root / 'owned'
        target.write_bytes(b'owner can timestamp without write permission')
        target.chmod(0)
        os.utime(target, ns=(OLD, OLD))
        before = time.time_ns()
        self.ok(self.call('owned'))
        info = self.recent(target, before, time.time_ns())
        self.assertEqual(stat.S_IMODE(info.st_mode), 0)
        target.chmod(0o600)
        self.assertEqual(target.read_bytes(),
                         b'owner can timestamp without write permission')
        for mask in (0, 0o077, 0o777):
            name = f'mask-{mask}'
            self.ok(self.call(name, mask=mask))
            self.assertEqual(stat.S_IMODE((self.root / name).stat().st_mode),
                             0o666 & ~mask)

    @unittest.skipIf(os.geteuid() == 0, 'root bypasses directory permissions')
    def test_permission_denied_continue(self):
        parent = self.root / 'locked'
        parent.mkdir()
        parent.chmod(0o500)
        try:
            result = self.call('locked/forbidden', 'later')
            self.assertEqual((result.returncode, result.stdout), (1, b''))
            self.assertIn(b'locked/forbidden', result.stderr)
            self.assertFalse((parent / 'forbidden').exists())
            self.assertTrue((self.root / 'later').is_file())
        finally:
            parent.chmod(0o700)

    def test_many_independent_seeded_fixtures(self):
        rng = random.Random(420)
        names = [f'name-{i}' for i in range(128)]
        contents = {}
        modes = {}
        for i, name in enumerate(names):
            if i % 2:
                contents[name] = rng.randbytes(i * 11)
                modes[name] = rng.choice([0o400, 0o600, 0o755])
                path = self.root / name
                path.write_bytes(contents[name])
                path.chmod(modes[name])
                os.utime(path, ns=(OLD, OLD))
        before = time.time_ns()
        self.ok(self.call('--', *names, names[0], names[-1]))
        after = time.time_ns()
        self.assertEqual(sorted(p.name for p in self.root.iterdir()),
                         sorted(names))
        for name in names:
            path = self.root / name
            info = self.recent(path, before, after)
            self.assertEqual(stat.S_IMODE(info.st_mode), modes.get(name, 0o640))
            self.assertEqual(path.read_bytes(), contents.get(name, b''))

    @unittest.skipIf(args.reference, 'project output status')
    def test_output_failure_retains_effects(self):
        with open('/dev/full', 'wb', buffering=0) as full:
            result = self.call('--json', 'first', 'last', stdout=full)
            self.assertEqual(result.returncode, 1)
            self.assertNotEqual(result.stderr, b'')
            self.assertEqual((self.root / 'first').read_bytes(), b'')
            self.assertEqual((self.root / 'last').read_bytes(), b'')
            result = self.call('--help', stdout=full)
            self.assertEqual(result.returncode, 1)

if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(TouchTests)
    if args.case:
        suite = unittest.TestSuite(t for t in suite if args.case in t.id())
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(not result.wasSuccessful())
