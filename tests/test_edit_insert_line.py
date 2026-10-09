#!/usr/bin/env python3
"""Independent exact-byte expectations for the custom editing command."""
import argparse
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('--program', required=True)
parser.add_argument('--prefix', default='')
args, rest = parser.parse_known_args()
PROGRAM = str(Path(args.program).resolve())
PREFIX = [args.prefix] if args.prefix else []


class InsertLineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'])
        self.addCleanup(self.temp.cleanup)
        self.cwd = Path(self.temp.name)

    def run_cli(self, *argv, output=None):
        return subprocess.run([PROGRAM, *PREFIX, *argv], cwd=self.cwd,
                              input=b'unread stdin', stdout=output or subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              env={**os.environ, 'LC_ALL': 'C'}, timeout=10)

    def insert(self, source, number, text, expected, filename='file'):
        path = self.cwd / filename
        path.write_bytes(source)
        before = path.stat()
        proc = self.run_cli(filename, str(number), text)
        self.assertEqual((proc.returncode, proc.stdout, proc.stderr), (0, b'', b''))
        self.assertEqual(path.read_bytes(), expected)
        self.assertNotEqual(path.stat().st_ino, before.st_ino)
        self.assertEqual(sorted(p.name for p in self.cwd.iterdir()), [filename])

    def test_json_exact_success_errors_and_utf8(self):
        file = self.cwd / 'a"雪\\name'
        file.write_bytes(b'old\nkeep')
        p = self.run_cli('--json', file.name, '01', '\udcff')
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        expected = ('{"path":"a\\"雪\\\\name","line":1,"status":"ok",'
                    '"message":null}\n').encode()
        self.assertEqual(p.stdout, expected)
        self.assertEqual(file.read_bytes(), b'\xff\nold\nkeep')
        p = self.run_cli('--json', file.name, '5', 'x')
        self.assertEqual(p.returncode, 1)
        self.assertTrue(p.stderr)
        self.assertEqual(json.loads(p.stdout),
                         {'path': file.name, 'line': 5, 'status': 'error',
                          'message': os.strerror(34)})
        raw = os.fsdecode(b'raw-\xff')
        (self.cwd / raw).write_bytes(b'old')
        p = self.run_cli('--json', raw, '1', 'x')
        self.assertEqual((p.returncode, p.stdout), (2, b''))
        self.assertEqual((self.cwd / raw).read_bytes(), b'old')
        p = self.run_cli(raw, '1', 'x')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual((self.cwd / raw).read_bytes(), b'x\nold')

    def test_help_arity_optionlike_paths(self):
        expected = (
            'Usage: edit-insert-line [-h] [--json] FILE N TEXT\n\n'
            '  -h, --help           Show help without editing.\n'
            '  --json               Emit one UTF-8 JSON result.\n'
            '  FILE                 Existing regular file.\n'
            '  N                    Insert before 1-based N or append.\n'
            '  TEXT                 Inserted content, no CR or LF.\n\n'
            'Insert TEXT plus LF before line N using complete-file staging.\n'
            'N=count+1 appends; empty files accept N=1.\n'
            'Preserve existing bytes; unterminated append adds an LF separator.\n'
            'Reject final symlinks, hard links and unsupported file metadata.\n'
            "Use -- for option-like operands. '-' is a literal filename.\n"
            'Exit status: 0 success, 1 operational failure, 2 argument error.\n'
        ).encode()
        for argv in [('-h',), ('--help',), ('--help', 'absent', '0', 'x'),
                     ('--json', '--help')]:
            p = self.run_cli(*argv)
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, expected, b''))
        p = self.run_cli('--help', 'a', '1', 'x', 'extra')
        self.assertEqual(p.returncode, 2)
        self.assertEqual(list(self.cwd.iterdir()), [])
        for name in ('--json', '-'):
            (self.cwd / name).write_bytes(b'old\nkeep')
            p = self.run_cli('--', name, '1', '--help')
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
            self.assertEqual((self.cwd / name).read_bytes(), b'--help\nold\nkeep')

    def test_output_failure_post_commit(self):
        file = self.cwd / 'file'
        file.write_bytes(b'old\nkeep')
        with open('/dev/full', 'wb', buffering=0) as output:
            p = self.run_cli('--json', 'file', '1', 'new', output=output)
            self.assertEqual(p.returncode, 1)
            self.assertTrue(p.stderr)
            self.assertEqual(file.read_bytes(), b'new\nold\nkeep')
            p = self.run_cli('--help', output=output)
            self.assertEqual(p.returncode, 1)
            self.assertTrue(p.stderr)
            p = self.run_cli('file', '1', 'silent', output=output)
            self.assertEqual(p.returncode, 0)
        self.assertEqual(file.read_bytes(), b'silent\nnew\nold\nkeep')

    def test_permission_failures_unchanged(self):
        if os.geteuid() == 0:
            self.skipTest('Kernel root privileges bypass mode-only access fixtures')
        file = self.cwd / 'file'
        file.write_bytes(b'old\nkeep')
        for mode in (0o400, 0o000):
            file.chmod(mode)
            before = file.stat()
            try:
                p = self.run_cli('--json', 'file', '1', 'new')
                self.assertEqual(p.returncode, 1)
                self.assertTrue(p.stderr)
                self.assertEqual(json.loads(p.stdout)['status'], 'error')
                self.assertEqual(file.stat().st_ino, before.st_ino)
                self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                self.assertEqual(file.stat().st_mode, before.st_mode)
                self.assertFalse(list(self.cwd.glob('.edit-insert-line-*')))
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
            p = self.run_cli('locked/file', '1', 'new')
            self.assertEqual((p.returncode, p.stdout), (1, b''))
            self.assertTrue(p.stderr)
            self.assertEqual(target.read_bytes(), b'old\nkeep')
            self.assertEqual(target.stat().st_ino, before.st_ino)
            self.assertEqual(sorted(p.name for p in locked.iterdir()), ['file'])
        finally:
            locked.chmod(0o755)

    def test_large_bounded_memory_and_max_name(self):
        import hashlib
        import resource
        file = self.cwd / 'large'
        block = bytes(range(256)) * 256
        expected = hashlib.sha256(b'new\nold\n')
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
        p = subprocess.run([PROGRAM, *PREFIX, 'large', '1', 'new'], cwd=self.cwd,
                           stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, timeout=30,
                           preexec_fn=None if sanitized else memory_limit)
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        actual = hashlib.sha256()
        with file.open('rb') as result:
            while chunk := result.read(65536):
                actual.update(chunk)
        self.assertEqual(actual.digest(), expected.digest())
        self.assertFalse(list(self.cwd.glob('.edit-insert-line-*')))
        file.unlink()
        maximum = os.pathconf(self.cwd, 'PC_NAME_MAX')
        self.insert(b'old\nkeep', 1, 'new', b'new\nold\nkeep', 'a' * maximum)

    def test_seeded_byte_properties(self):
        rng = random.Random(420)
        for _ in range(150):
            count = rng.randrange(0, 12)
            parts = [bytes(rng.choice([0, 1, 13, 65, 127, 255])
                           for _ in range(rng.randrange(1, 90)))
                     for _ in range(count)]
            ending = rng.choice([b'', b'\n']) if count else b''
            n = rng.randrange(1, count + 2)
            source = b'\n'.join(parts) + ending
            if n <= count:
                offset = sum(len(p) + 1 for p in parts[:n-1])
                expected = source[:offset] + b'replacement\n' + source[offset:]
            else:
                expected = source + (b'\n' if source and not ending else b'')
                expected += b'replacement\n'
            self.insert(source, n, 'replacement', expected)

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
                p = self.run_cli(name, '1', 'new')
                self.assertEqual(p.returncode, 1)
                self.assertEqual(p.stdout, b'')
                self.assertTrue(p.stderr)
                self.assertEqual(file.read_bytes(), b'old\nlast')
                self.assertEqual(file.stat().st_ino, before.st_ino)
                self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                self.assertFalse(list(self.cwd.glob('.edit-insert-line-*')))
        (self.cwd / 'hardlink').unlink()
        file.chmod(0o4755)
        p = self.run_cli('file', '1', 'new')
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
            p = self.run_cli('alias/file', '1', 'new')
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
            after = file.stat()
            self.assertEqual(after.st_mode & 0o7777, mode)
            self.assertEqual((after.st_uid, after.st_gid), (before.st_uid, before.st_gid))
            self.assertEqual(file.read_bytes(), b'new\nold\nkeep')
        self.assertTrue((self.cwd / 'alias').is_symlink())

    def test_argument_validation_before_mutation(self):
        file = self.cwd / 'file'
        file.write_bytes(b'a\nb\n')
        invalid = ['', '0', '-1', '+1', ' 1', '1 ', '0x1', '1k', '1.0',
                   '18446744073709551616', '9' * 80]
        cases = [('file', n, 'bad') for n in invalid]
        cases += [(), ('file',), ('file', '1'), ('file', '1', 'bad', 'extra'),
                  ('', '1', 'x'), ('file', '1', 'bad\n'), ('file', '1', '\r'),
                  ('--unknown', 'file', '1', 'x')]
        for argv in cases:
            with self.subTest(argv=argv):
                before = file.stat()
                p = self.run_cli(*argv)
                self.assertEqual(p.returncode, 2)
                self.assertEqual(p.stdout, b'')
                self.assertTrue(p.stderr)
                self.assertEqual(file.read_bytes(), b'a\nb\n')
                self.assertEqual(file.stat().st_ino, before.st_ino)
                self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                self.assertEqual(sorted(p.name for p in self.cwd.iterdir()), ['file'])

    def test_out_of_range_and_missing(self):
        for source, number in [(b'', 2), (b'a\n', 3), (b'a', 3), (b'\n', 3),
                               (b'a\nb\n', 4), (b'a', 18446744073709551615)]:
            with self.subTest(source=source, number=number):
                file = self.cwd / 'file'
                file.write_bytes(source)
                before = file.stat()
                p = self.run_cli('file', str(number), 'new')
                self.assertEqual(p.returncode, 1)
                self.assertEqual(p.stdout, b'')
                self.assertTrue(p.stderr)
                self.assertEqual(file.read_bytes(), source)
                self.assertEqual(file.stat().st_ino, before.st_ino)
                self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                self.assertFalse(list(self.cwd.glob('.edit-insert-line-*')))
        p = self.run_cli('missing', '1', 'x')
        self.assertEqual(p.returncode, 1)
        self.assertFalse((self.cwd / 'missing').exists())

    def test_crlf_and_unterminated(self):
        cases = [(b'a\r\nb\r\nc', 2, 'X', b'a\r\nX\nb\r\nc'),
                 (b'a\nb', 2, '', b'a\n\nb'),
                 (b'\n\n', 1, 'first', b'first\n\n\n'),
                 (b'bare\r', 1, 'last', b'last\nbare\r'),
                 (b'\x00\xff\nold\n\x01', 2, 'new',
                  b'\x00\xff\nnew\nold\n\x01')]
        for source, n, text, expected in cases:
            with self.subTest(source=source):
                self.insert(source, n, text, expected)

    def test_boundaries(self):
        for size in (65535, 65536, 65537, 131072):
            with self.subTest(size=size):
                self.insert(b'A' * size + b'\r\nB\nend', 1, 'x', b'x\n' + b'A' * size + b'\r\nB\nend')
                text = 'x' * min(size, 65537)
                self.insert(b'A' * size + b'\nB\nend', 2, text,
                             b'A' * size + b'\n' + text.encode() + b'\nB\nend')

    def test_empty_and_append(self):
        for source, n, expected in [
                (b'', 1, b'new\n'), (b'a\n', 2, b'a\nnew\n'),
                (b'a', 2, b'a\nnew\n'),
                (b'a\r\nb\r\n', 3, b'a\r\nb\r\nnew\n'),
                (b'bare\r', 2, b'bare\r\nnew\n'),
                (b'\n\n', 3, b'\n\nnew\n')]:
            with self.subTest(source=source):
                self.insert(source, n, 'new', expected)

        self.insert(b'', 1, '', b'\n')
        self.insert(b'a', 2, '', b'a\n\n')

    def test_middle_line_staged(self):
        self.insert(b'first\nsecond\nthird\n', 2, 'new text',
                     b'first\nnew text\nsecond\nthird\n')


if __name__ == '__main__':
    unittest.main(argv=[__file__, *rest], verbosity=2)
