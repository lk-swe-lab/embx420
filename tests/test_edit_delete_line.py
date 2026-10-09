#!/usr/bin/env python3
"""Exact-byte fixtures for custom single-line deletion; no GNU oracle."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('--program', required=True)
parser.add_argument('--prefix', default='')
args, rest = parser.parse_known_args()
PROGRAM = str(Path(args.program).resolve())
PREFIX = [args.prefix] if args.prefix else []


class DeleteLineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'])
        self.addCleanup(self.temp.cleanup)
        self.cwd = Path(self.temp.name)

    def run_cli(self, *argv, output=None):
        return subprocess.run([PROGRAM, *PREFIX, *argv], cwd=self.cwd,
                              input=b'unread stdin', stdout=output or subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              env={**os.environ, 'LC_ALL': 'C'}, timeout=10)

    def delete(self, source, number, expected, filename='file'):
        path = self.cwd / filename
        path.write_bytes(source)
        before = path.stat()
        proc = self.run_cli(filename, str(number))
        self.assertEqual((proc.returncode, proc.stdout, proc.stderr), (0, b'', b''))
        self.assertEqual(path.read_bytes(), expected)
        self.assertNotEqual(path.stat().st_ino, before.st_ino)
        self.assertEqual(sorted(p.name for p in self.cwd.iterdir()), [filename])

    def test_exact_byte_cases(self):
        cases = [(b'a\r\nb\r\nc', 2, b'a\r\nc'),
                 (b'a\nb', 1, b'b'), (b'a\nb', 2, b'a\n'),
                 (b'\n\n', 1, b'\n'), (b'\n\n', 2, b'\n'),
                 (b'bare\r', 1, b''), (b'\r\n', 1, b''),
                 (b'only\n', 1, b''), (b'\x00\xff\nold\n\x01', 2,
                  b'\x00\xff\n\x01'), (b'a\r\nb\nc\r\n', 3, b'a\r\nb\n')]
        for source, n, expected in cases:
            with self.subTest(source=source, n=n):
                self.delete(source, n, expected)

    def test_buffer_boundaries(self):
        for size in (65534, 65535, 65536, 65537, 131072):
            with self.subTest(size=size):
                self.delete(b'A' * size + b'\r\nB\nend', 1, b'B\nend')
                self.delete(b'A' * size + b'\nB\nend', 2, b'A' * size + b'\nend')
                self.delete(b'A\n' + b'B' * size, 2, b'A\n')

    def test_seeded_byte_properties(self):
        import random
        rng = random.Random(420)
        for _ in range(150):
            count = rng.randrange(1, 12)
            parts = [bytes(rng.choice([0, 1, 13, 65, 127, 255])
                           for _ in range(rng.randrange(1, 90)))
                     for _ in range(count)]
            ending = rng.choice([b'', b'\n'])
            n = rng.randrange(1, count + 1)
            source = b'\n'.join(parts) + ending
            left = sum(len(p) + 1 for p in parts[:n-1])
            right = left + len(parts[n-1]) + (1 if n < count or ending else 0)
            self.delete(source, n, source[:left] + source[right:])

    def test_out_of_range_empty_and_missing(self):
        for source, number in [(b'', 1), (b'a\n', 2), (b'a', 2), (b'\n', 2),
                               (b'a\nb\n', 3), (b'a', 18446744073709551615)]:
            file = self.cwd / 'file'
            file.write_bytes(source)
            before = file.stat()
            p = self.run_cli('--json', 'file', str(number))
            self.assertEqual(p.returncode, 1)
            self.assertTrue(p.stderr)
            self.assertEqual(p.stdout, ('{"path":"file","line":' + str(number) +
                             ',"status":"error","message":"Numerical result out of range"}\n').encode())
            self.assertEqual(file.read_bytes(), source)
            self.assertEqual(file.stat().st_ino, before.st_ino)
            self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
            self.assertFalse(list(self.cwd.glob('.edit-delete-line-*')))
        p = self.run_cli('--json', 'missing', '1')
        self.assertEqual(p.returncode, 1)
        self.assertEqual(__import__('json').loads(p.stdout)['status'], 'error')
        self.assertFalse((self.cwd / 'missing').exists())

    def test_argument_validation_before_mutation(self):
        file = self.cwd / 'file'
        file.write_bytes(b'a\nb\n')
        invalid = ['', '0', '-1', '+1', ' 1', '1 ', '0x1', '1k', '1.0',
                   '18446744073709551616', '9' * 80]
        cases = [('file', n) for n in invalid]
        cases += [(), ('file',), ('file', '1', 'extra'), ('', '1'),
                  ('--unknown', 'file', '1'), ('--json', '--json', 'file', '1')]
        for argv in cases:
            with self.subTest(argv=argv):
                before = file.stat()
                p = self.run_cli(*argv)
                self.assertEqual((p.returncode, p.stdout), (2, b''))
                self.assertTrue(p.stderr)
                self.assertEqual(file.read_bytes(), b'a\nb\n')
                self.assertEqual(file.stat().st_ino, before.st_ino)
                self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                self.assertEqual(sorted(p.name for p in self.cwd.iterdir()), ['file'])

    def test_help_arity_optionlike_paths(self):
        expected = (
            'Usage: edit-delete-line [-h] [--json] FILE N\n\n'
            '  -h, --help           Show help without editing.\n'
            '  --json               Emit one UTF-8 JSON result.\n'
            '  FILE                 Existing regular file.\n'
            '  N                    Existing 1-based line to delete.\n\n'
            'Delete line N including its terminator using complete-file staging.\n'
            'LF delimits lines; a nonempty unterminated suffix is one line.\n'
            'Preserve all other bytes; empty files have no deletable lines.\n'
            'Reject final symlinks, hard links and unsupported file metadata.\n'
            "Use -- for option-like operands. '-' is a literal filename.\n"
            'Exit status: 0 success, 1 operational failure, 2 argument error.\n'
        ).encode()
        for argv in [('-h',), ('--help',), ('--help', 'absent', '0'),
                     ('--help', 'absent'), ('--json', '--help')]:
            p = self.run_cli(*argv)
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, expected, b''))
        for argv in [('--help', 'a', '1', 'extra'), ('--help', '--bad')]:
            p = self.run_cli(*argv)
            self.assertEqual((p.returncode, p.stdout), (2, b''))
        self.assertEqual(list(self.cwd.iterdir()), [])
        for name in ('--json', '-'):
            (self.cwd / name).write_bytes(b'old\nkeep')
            p = self.run_cli('--', name, '1')
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
            self.assertEqual((self.cwd / name).read_bytes(), b'keep')

    def test_json_and_raw_paths(self):
        file = self.cwd / 'a"雪\\name'
        file.write_bytes(b'old\n\xff\x00keep')
        p = self.run_cli('--json', file.name, '01')
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        self.assertEqual(p.stdout, ('{"path":"a\\"雪\\\\name","line":1,'
                                  '"status":"ok","message":null}\n').encode())
        self.assertEqual(file.read_bytes(), b'\xff\x00keep')
        raw = os.fsdecode(b'raw-\xff')
        (self.cwd / raw).write_bytes(b'old\nkeep')
        p = self.run_cli('--json', raw, '1')
        self.assertEqual((p.returncode, p.stdout), (2, b''))
        self.assertEqual((self.cwd / raw).read_bytes(), b'old\nkeep')
        p = self.run_cli(raw, '1')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual((self.cwd / raw).read_bytes(), b'keep')

    def test_output_failure_post_commit(self):
        file = self.cwd / 'file'
        file.write_bytes(b'old\nkeep\nlast')
        with open('/dev/full', 'wb', buffering=0) as output:
            p = self.run_cli('--json', 'file', '1', output=output)
            self.assertEqual(p.returncode, 1)
            self.assertTrue(p.stderr)
            self.assertEqual(file.read_bytes(), b'keep\nlast')
            p = self.run_cli('--help', output=output)
            self.assertEqual(p.returncode, 1)
            self.assertTrue(p.stderr)
            p = self.run_cli('file', '1', output=output)
            self.assertEqual(p.returncode, 0)
        self.assertEqual(file.read_bytes(), b'last')

    def test_large_bounded_memory_and_max_name(self):
        import hashlib
        import resource
        file = self.cwd / 'large'
        block = bytes(range(256)) * 256
        expected = hashlib.sha256()
        with file.open('wb') as output:
            output.write(b'old\n')
            for _ in range(512):
                output.write(block)
                expected.update(block)

        def memory_limit():
            limit = 24 * 1024 * 1024
            resource.setrlimit(resource.RLIMIT_AS, (limit, limit))

        linked = subprocess.run(['ldd', PROGRAM], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=5).stdout
        sanitized = b'libasan' in linked or b'libclang_rt.asan' in linked
        p = subprocess.run([PROGRAM, *PREFIX, 'large', '1'], cwd=self.cwd,
                           stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, timeout=30,
                           preexec_fn=None if sanitized else memory_limit)
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        actual = hashlib.sha256()
        with file.open('rb') as result:
            while chunk := result.read(65536):
                actual.update(chunk)
        self.assertEqual(actual.digest(), expected.digest())
        self.assertFalse(list(self.cwd.glob('.edit-delete-line-*')))
        file.unlink()
        self.delete(b'old\nkeep', 1, b'keep', 'a' * os.pathconf(self.cwd, 'PC_NAME_MAX'))

    def test_permission_failures_unchanged(self):
        if os.geteuid() == 0:
            self.skipTest('Kernel root privileges bypass mode-only access fixtures')
        file = self.cwd / 'file'
        file.write_bytes(b'old\nkeep')
        for mode in (0o400, 0o000):
            file.chmod(mode)
            before = file.stat()
            try:
                p = self.run_cli('--json', 'file', '1')
                self.assertEqual(p.returncode, 1)
                self.assertTrue(p.stderr)
                self.assertEqual(json.loads(p.stdout)['status'], 'error')
                self.assertEqual(file.stat().st_ino, before.st_ino)
                self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                self.assertEqual(file.stat().st_mode, before.st_mode)
                self.assertFalse(list(self.cwd.glob('.edit-delete-line-*')))
            finally:
                file.chmod(0o600)
            self.assertEqual(file.read_bytes(), b'old\nkeep')
        locked = self.cwd / 'locked'
        locked.mkdir()
        target = locked / 'file'
        target.write_bytes(b'old\nkeep')
        before = target.stat()
        locked.chmod(0o555)
        try:
            p = self.run_cli('locked/file', '1')
            self.assertEqual((p.returncode, p.stdout), (1, b''))
            self.assertTrue(p.stderr)
            self.assertEqual(target.read_bytes(), b'old\nkeep')
            self.assertEqual(target.stat().st_ino, before.st_ino)
            self.assertEqual(sorted(p.name for p in locked.iterdir()), ['file'])
        finally:
            locked.chmod(0o755)

    def test_unsafe_targets_unchanged(self):
        file = self.cwd / 'file'
        file.write_bytes(b'old\nlast')
        (self.cwd / 'symlink').symlink_to('file')
        os.link(file, self.cwd / 'hardlink')
        (self.cwd / 'directory').mkdir()
        os.mkfifo(self.cwd / 'fifo')
        for name in ['symlink', 'hardlink', 'file', 'directory', 'fifo']:
            with self.subTest(name=name):
                before = file.stat()
                p = self.run_cli(name, '1')
                self.assertEqual(p.returncode, 1)
                self.assertEqual(p.stdout, b'')
                self.assertTrue(p.stderr)
                self.assertEqual(file.read_bytes(), b'old\nlast')
                self.assertEqual(file.stat().st_ino, before.st_ino)
                self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                self.assertFalse(list(self.cwd.glob('.edit-delete-line-*')))
        (self.cwd / 'hardlink').unlink()
        file.chmod(0o4755)
        p = self.run_cli('file', '1')
        self.assertEqual(p.returncode, 1)
        self.assertEqual(file.read_bytes(), b'old\nlast')
        self.assertTrue((self.cwd / 'symlink').is_symlink())

    def test_modes_and_parent_symlink(self):
        (self.cwd / 'real').mkdir()
        (self.cwd / 'alias').symlink_to('real', target_is_directory=True)
        file = self.cwd / 'real' / 'file'
        file.write_bytes(b'old\nkeep')
        for mode in (0o600, 0o640, 0o751, 0o777):
            file.write_bytes(b'old\nkeep')
            file.chmod(mode)
            before = file.stat()
            p = self.run_cli('alias/file', '1')
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
            after = file.stat()
            self.assertEqual(after.st_mode & 0o7777, mode)
            self.assertEqual((after.st_uid, after.st_gid), (before.st_uid, before.st_gid))
            self.assertEqual(file.read_bytes(), b'keep')
        self.assertTrue((self.cwd / 'alias').is_symlink())

    def test_replacement_inode_and_metadata(self):
        file = self.cwd / 'file'
        file.write_bytes(b'old\nkeep')
        file.chmod(0o640)
        os.utime(file, ns=(1, 1))
        os.setxattr(file, 'user.edit_delete_line_fixture', b'original')
        before = file.stat()
        with file.open('rb') as original:
            p = self.run_cli('file', '1')
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
            after = file.stat()
            self.assertNotEqual(after.st_ino, before.st_ino)
            self.assertEqual((after.st_mode, after.st_uid, after.st_gid,
                              after.st_nlink),
                             (before.st_mode, before.st_uid, before.st_gid, 1))
            self.assertNotEqual(after.st_mtime_ns, before.st_mtime_ns)
            self.assertNotIn('user.edit_delete_line_fixture', os.listxattr(file))
            self.assertEqual(file.read_bytes(), b'keep')
            self.assertEqual(os.fstat(original.fileno()).st_ino, before.st_ino)
            self.assertEqual(original.read(), b'old\nkeep')
            self.assertEqual(os.getxattr(original.fileno(),
                                        'user.edit_delete_line_fixture'), b'original')

    def test_unterminated_last_line(self):
        self.delete(b'first\nsecond', 2, b'first\n')
        self.delete(b'only', 1, b'')

    def test_middle_line_staged(self):
        self.delete(b'first\nsecond\nthird\n', 2, b'first\nthird\n')


if __name__ == '__main__':
    unittest.main(argv=[__file__, *rest], verbosity=2)
