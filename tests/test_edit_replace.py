#!/usr/bin/env python3
"""Independent exact-byte oracle for the custom wholefile editor."""
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
parser.add_argument('--slice', default='all')
ARGS, REST = parser.parse_known_args()
PROGRAM = str(Path(ARGS.program).resolve())
PREFIX = [ARGS.prefix] if ARGS.prefix else []


class ReplaceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'])
        self.addCleanup(self.temp.cleanup)
        self.cwd = Path(self.temp.name)

    def run_cli(self, *argv, output=None):
        return subprocess.run([PROGRAM, *PREFIX, *argv], cwd=self.cwd,
                              input=b'unread stdin', stdout=output or subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              env={**os.environ, 'LC_ALL': 'C'}, timeout=20)

    def replace(self, source, pattern, replacement, expected, count, *flags):
        file = self.cwd / 'file'
        file.write_bytes(source)
        before = file.stat()
        p = self.run_cli('--json', *flags, 'file', pattern, replacement)
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        self.assertEqual(p.stdout, ('{"path":"file","matches":' + str(count) +
                                   ',"replacements":' + str(count) + '}\n').encode())
        self.assertEqual(file.read_bytes(), expected)
        if count:
            self.assertNotEqual(file.stat().st_ino, before.st_ino)
        else:
            self.assertEqual(file.stat().st_ino, before.st_ino)
            self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
            self.assertEqual(file.stat().st_ctime_ns, before.st_ctime_ns)
        self.assertEqual(sorted(p.name for p in self.cwd.iterdir()), ['file'])

    def test_literal_global(self):
        self.replace(b'banana\nbanana', 'ana', 'X', b'bXna\nbXna', 2,
                     '--fixed-strings')
        self.replace(b'aaaa', 'aa', '', b'', 2, '--fixed-strings')
        self.replace(b'abc', 'z', 'X', b'abc', 0, '--fixed-strings')

    def test_regex_wholefile(self):
        self.replace(b'abbb a\nb ab', 'ab+', '<&\\1>',
                     b'<&\\1> a\nb <&\\1>', 2)
        self.replace(b'ab\ncd\r\nef', 'b.c', 'X', b'aXd\r\nef', 1)
        self.replace(b'ab\nab', '^ab|ab$', 'X', b'X\nX', 2)
        self.replace(b'ababa', 'a|ab', 'X', b'XXX', 3)
        self.replace(b'abc', '[xyz]', '', b'abc', 0)

    def test_zero_width_empty(self):
        for flags in [(), ('--fixed-strings',)]:
            self.replace(b'ab', '', 'X', b'XaXbX', 3, *flags)
            self.replace(b'', '', 'X', b'X', 1, *flags)
            self.replace(b'ab', '', '', b'ab', 3, *flags)
        self.replace(b'aa', 'a*', 'X', b'XX', 2)
        self.replace(b'bbb', 'a*', 'X', b'XbXbXbX', 4)
        self.replace(b'ab\n', '^|$', 'X', b'Xab\nX', 2)
        self.replace(b'ab', '$', 'X', b'abX', 1)
        self.replace(b'ab', '^', 'X', b'Xab', 1)

    def test_ascii_case_modes(self):
        for flags in [(), ('--fixed-strings',)]:
            self.replace(b'Ab aB AB ab', 'ab', 'Z', b'Z Z Z Z', 4, '-i', *flags)
            self.replace(b'Ab aB AB ab', 'ab', 'Z', b'Ab aB AB Z', 1, *flags)
        self.replace(b'A.z a.Z abZ', 'a.z', 'X', b'X X abZ', 2,
                     '-i', '--fixed-strings')
        self.replace(b'A.z a.Z abZ', 'a.z', 'X', b'X X X', 3, '-i')
        self.replace(b'ABC\nabc', '[a-c]+', 'x', b'x\nx', 2, '-i')


    def test_byte_patterns_replacements_and_binary_policy(self):
        self.replace(b'A\x00\xffB\n\rA\x00\xffB', os.fsdecode(b'\xffB'),
                     '\\&$\n\r', b'A\x00\\&$\n\r\n\rA\x00\\&$\n\r', 2,
                     '--fixed-strings')
        self.replace(b'\xff\x80A', '.', os.fsdecode(b'\xfe'), b'\xfe' * 3, 3)
        self.replace(b'a\nb\r\nc', '\nb\r\n', '&\\1', b'a&\\1c', 1)
        self.replace(b'\\1 [.] $', '\\1', 'X', b'X [.] $', 1,
                     '--fixed-strings')
        file = self.cwd / 'file'
        file.write_bytes(b'a\x00b')
        before = file.stat()
        p = self.run_cli('--json', 'file', 'a', 'X')
        self.assertEqual(p.returncode, 1)
        self.assertEqual(json.loads(p.stdout), dict(path='file', matches=0,
                         replacements=0, status='error', message='Invalid argument'))
        self.assertEqual(file.read_bytes(), b'a\x00b')
        self.assertEqual(file.stat().st_ino, before.st_ino)
        self.assertFalse(list(self.cwd.glob('.edit-replace-*')))

    def test_literal_exhaustive_byte_oracle(self):
        import itertools
        alphabet = [0, 10, 13, 65, 255]
        sources = [bytes(x) for n in range(4)
                   for x in itertools.product(alphabet, repeat=n)]
        for source in sources:
            for pattern in [b'', b'A', b'\n', b'AA', b'\xff\r']:
                # Independent offset oracle: no regex or executable reference.
                chunks = []
                offset = count = 0
                while offset <= len(source):
                    end = offset + len(pattern)
                    if source[offset:end] == pattern and end <= len(source):
                        chunks.append(b'&\\\n')
                        count += 1
                        offset = end
                        if not pattern:
                            if offset == len(source):
                                break
                            chunks.append(source[offset:offset+1])
                            offset += 1
                    elif offset < len(source):
                        chunks.append(source[offset:offset+1])
                        offset += 1
                    else:
                        break
                self.replace(source, os.fsdecode(pattern), '&\\\n',
                             b''.join(chunks), count, '--fixed-strings')

    def test_seeded_regex_common_subset(self):
        import random
        rng = random.Random(420)
        # Only independently decidable POSIX subset, not Python-regex oracle.
        for _ in range(180):
            source = bytes(rng.choice(b'ab\n\r\xff') for _ in range(rng.randrange(80)))
            pattern = rng.choice(['a+', '[ab]+', '.'])
            spans = []
            if pattern == '.':
                spans = [(i, i+1) for i in range(len(source))]
            else:
                i = 0
                accepted = b'a' if pattern == 'a+' else b'ab'
                while i < len(source):
                    if source[i] in accepted:
                        start = i
                        while i < len(source) and source[i] in accepted:
                            i += 1
                        spans.append((start, i))
                    else:
                        i += 1
            chunks = []
            previous = 0
            for left, right in spans:
                chunks.extend([source[previous:left], b'Z\n'])
                previous = right
            chunks.append(source[previous:])
            self.replace(source, pattern, 'Z\n', b''.join(chunks), len(spans))

    def test_argument_regex_validation_before_mutation(self):
        file = self.cwd / 'file'
        file.write_bytes(b'a\nb\n')
        cases = [(), ('file',), ('file', 'a'), ('file', 'a', 'x', 'extra'),
                 ('', 'a', 'x'), ('--unknown', 'file', 'a', 'x'),
                 ('--json', '--json', 'file', 'a', 'x'),
                 ('-i', '-i', 'file', 'a', 'x'),
                 ('file', '[' , 'x'), ('file', '\\1', 'x'),
                 ('file', '(a)\\1', 'x'), ('file', '[z-a]', 'x'),
                 ('file', 'a' * 1025, 'x'), ('file', 'a', 'x' * 4097)]
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
        p = self.run_cli('--json', 'missing', 'a', 'x')
        self.assertEqual(p.returncode, 1)
        self.assertEqual(json.loads(p.stdout)['status'], 'error')
        self.assertFalse((self.cwd / 'missing').exists())

    def test_help_and_optionlike_operands(self):
        expected = self.run_cli('--help')
        self.assertEqual((expected.returncode, expected.stderr), (0, b''))
        self.assertTrue(expected.stdout.startswith(
            b'Usage: edit-replace [-hi] [--json] [--fixed-strings] FILE PATTERN REPLACEMENT\n'))
        for phrase in [b'Replace all nonoverlapping', b'Replacement is literal',
                       b'No matches succeed unchanged', b'Regex rejects NUL',
                       b'input 8 MiB', b'no hard libc-regex CPU/memory guarantee']:
            self.assertIn(phrase, expected.stdout)
        for argv in [('-h',), ('--help', 'absent', '['),
                     ('--help', 'absent', '[', 'text'), ('--json', '--help')]:
            p = self.run_cli(*argv)
            self.assertEqual((p.returncode, p.stdout, p.stderr),
                             (0, expected.stdout, b''))
        for argv in [('--help', 'a', 'b', 'c', 'd'), ('--help', '--bad')]:
            p = self.run_cli(*argv)
            self.assertEqual((p.returncode, p.stdout), (2, b''))
        self.assertEqual(list(self.cwd.iterdir()), [])
        for name in ('--json', '-'):
            (self.cwd / name).write_bytes(b'--json --json')
            p = self.run_cli('--fixed-strings', '--', name, '--json', '-i')
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
            self.assertEqual((self.cwd / name).read_bytes(), b'-i -i')

    def test_json_raw_path_and_silent_success(self):
        name = 'a"雪\\name'
        file = self.cwd / name
        file.write_bytes(b'old\n\xffkeep')
        p = self.run_cli('--json', name, 'old', 'new')
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        self.assertEqual(p.stdout, ('{"path":"a\\"雪\\\\name",'
                                   '"matches":1,"replacements":1}\n').encode())
        self.assertEqual(file.read_bytes(), b'new\n\xffkeep')
        raw = os.fsdecode(b'raw-\xff')
        (self.cwd / raw).write_bytes(b'old')
        p = self.run_cli('--json', raw, 'old', 'new')
        self.assertEqual((p.returncode, p.stdout), (2, b''))
        self.assertEqual((self.cwd / raw).read_bytes(), b'old')
        p = self.run_cli(raw, 'old', 'new')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual((self.cwd / raw).read_bytes(), b'new')

    def test_output_failure_post_commit(self):
        file = self.cwd / 'file'
        file.write_bytes(b'old\nkeep\nlast')
        with open('/dev/full', 'wb', buffering=0) as output:
            p = self.run_cli('--json', 'file', 'old', 'new', output=output)
            self.assertEqual(p.returncode, 1)
            self.assertTrue(p.stderr)
            self.assertEqual(file.read_bytes(), b'new\nkeep\nlast')
            p = self.run_cli('--help', output=output)
            self.assertEqual(p.returncode, 1)
            p = self.run_cli('file', 'new', 'last', output=output)
            self.assertEqual(p.returncode, 0)
        self.assertEqual(file.read_bytes(), b'last\nkeep\nlast')

    def test_large_limits_and_buffer_boundaries(self):
        import resource
        for size in (65535, 65536, 65537, 131071):
            self.replace(b'A' * size + b'xy\x00end', 'xy', 'Z',
                         b'A' * size + b'Z\x00end', 1, '--fixed-strings')
        self.replace(b'a' * 8192, 'a', 'x' * 4096, b'x' * (32 * 1024 * 1024),
                     8192, '--fixed-strings')
        file = self.cwd / 'file'
        for source, pattern, replacement in [(b'a' * 8193, 'a', 'x' * 4096),
                                             (b'a' * 8192, '', 'x' * 4096),
                                             (b'a' * (8 * 1024 * 1024 + 1), 'z', '')]:
            file.write_bytes(source)
            before = file.stat()
            p = self.run_cli('--fixed-strings', 'file', pattern, replacement)
            self.assertEqual((p.returncode, p.stdout), (1, b''))
            self.assertTrue(p.stderr)
            self.assertEqual(file.read_bytes(), source)
            self.assertEqual(file.stat().st_ino, before.st_ino)
            self.assertFalse(list(self.cwd.glob('.edit-replace-*')))
        source = b'a' * (8 * 1024 * 1024 - 1) + b'Z'
        file.write_bytes(source)
        linked = subprocess.run(['ldd', PROGRAM], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=5).stdout
        sanitized = b'libasan' in linked or b'libclang_rt.asan' in linked
        def memory_limit():
            size = 24 * 1024 * 1024
            resource.setrlimit(resource.RLIMIT_AS, (size, size))
        p = subprocess.run([PROGRAM, *PREFIX, '--fixed-strings', 'file', 'Z', 'X'],
                           cwd=self.cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           preexec_fn=None if sanitized else memory_limit, timeout=30)
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual(file.read_bytes(), source[:-1] + b'X')
        file.unlink()
        name = 'a' * os.pathconf(self.cwd, 'PC_NAME_MAX')
        (self.cwd / name).write_bytes(b'old')
        p = self.run_cli(name, 'old', 'new')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual((self.cwd / name).read_bytes(), b'new')

    def test_max_operand_limits_and_no_match_parent_permissions(self):
        self.replace(b'a' * 1024, 'a' * 1024, 'x' * 4096, b'x' * 4096, 1,
                     '--fixed-strings')
        file = self.cwd / 'file'
        file.write_bytes(b'unchanged')
        before = file.stat()
        self.cwd.chmod(0o555)
        try:
            p = self.run_cli('--json', 'file', 'absent', 'x')
            self.assertEqual((p.returncode, p.stderr), (0, b''))
            self.assertEqual(json.loads(p.stdout), dict(path='file', matches=0, replacements=0))
            self.assertEqual(file.stat().st_ino, before.st_ino)
            self.assertEqual(file.stat().st_ctime_ns, before.st_ctime_ns)
        finally:
            self.cwd.chmod(0o700)
    def test_permission_failures_unchanged(self):
        if os.geteuid() == 0:
            self.skipTest('Kernel root privileges bypass mode-only access fixtures')
        file = self.cwd / 'file'
        file.write_bytes(b'old\nkeep')
        for mode in (0o400, 0o000):
            file.chmod(mode)
            before = file.stat()
            try:
                p = self.run_cli('--json', 'file', 'old\n', '')
                self.assertEqual(p.returncode, 1)
                self.assertTrue(p.stderr)
                self.assertEqual(json.loads(p.stdout)['status'], 'error')
                self.assertEqual(file.stat().st_ino, before.st_ino)
                self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                self.assertEqual(file.stat().st_mode, before.st_mode)
                self.assertFalse(list(self.cwd.glob('.edit-replace-*')))
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
            p = self.run_cli('locked/file', 'old\n', '')
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
        import socket
        sock = socket.socket(socket.AF_UNIX)
        self.addCleanup(sock.close)
        sock.bind(str(self.cwd / 'socket'))
        for name in ['symlink', 'hardlink', 'file', 'directory', 'fifo', 'socket']:
            with self.subTest(name=name):
                before = file.stat()
                p = self.run_cli(name, 'old\n', '')
                self.assertEqual(p.returncode, 1)
                self.assertEqual(p.stdout, b'')
                self.assertTrue(p.stderr)
                self.assertEqual(file.read_bytes(), b'old\nlast')
                self.assertEqual(file.stat().st_ino, before.st_ino)
                self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                self.assertFalse(list(self.cwd.glob('.edit-replace-*')))
        (self.cwd / 'hardlink').unlink()
        file.chmod(0o4755)
        p = self.run_cli('file', 'old\n', '')
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
            p = self.run_cli('alias/file', 'old\n', '')
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
        os.setxattr(file, 'user.edit_replace_fixture', b'original')
        before = file.stat()
        with file.open('rb') as original:
            p = self.run_cli('file', 'old\n', '')
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
            after = file.stat()
            self.assertNotEqual(after.st_ino, before.st_ino)
            self.assertEqual((after.st_mode, after.st_uid, after.st_gid,
                              after.st_nlink),
                             (before.st_mode, before.st_uid, before.st_gid, 1))
            self.assertNotEqual(after.st_mtime_ns, before.st_mtime_ns)
            self.assertNotIn('user.edit_replace_fixture', os.listxattr(file))
            self.assertEqual(file.read_bytes(), b'keep')
            self.assertEqual(os.fstat(original.fileno()).st_ino, before.st_ino)
            self.assertEqual(original.read(), b'old\nkeep')
            self.assertEqual(os.getxattr(original.fileno(),
                                        'user.edit_replace_fixture'), b'original')



    def test_large_zero_width_resource_boundary(self):
        file = self.cwd / 'file'
        linked = subprocess.run(['ldd', PROGRAM], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=5).stdout
        sanitized = b'libasan' in linked or b'libclang_rt.asan' in linked
        # ASan regexec interceptor unconditionally strlen-checks the full buffer,
        # even with REG_STARTEND. Keep byte/count/deadline assertions unchanged;
        # normal performance guard is 1 MiB, sanitizer stress is 8 KiB.
        source = b'b' * (8192 if sanitized else 1024 * 1024)
        for pattern in ('', 'a*'):
            file.write_bytes(source)
            before = file.stat()
            try:
                p = subprocess.run([PROGRAM, *PREFIX, '--json', 'file', pattern, ''],
                                   cwd=self.cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   env={**os.environ, 'LC_ALL': 'C'}, timeout=4)
            except subprocess.TimeoutExpired:
                self.fail('bounded zero-width execution timed out')
            self.assertEqual((p.returncode, p.stderr), (0, b''))
            self.assertEqual(json.loads(p.stdout), dict(path='file', matches=len(source)+1,
                                                       replacements=len(source)+1))
            self.assertEqual(file.read_bytes(), source)
            self.assertNotEqual(file.stat().st_ino, before.st_ino)


    def test_nonascii_folding_locale_and_identical_matches(self):
        for flags in [(), ('--fixed-strings',)]:
            self.replace('Ä ä A a'.encode(), 'Ä', 'X', 'X ä A a'.encode(), 1,
                         '-i', *flags)
            self.replace(b'a\na', 'a', 'a', b'a\na', 2, *flags)
        file = self.cwd / 'file'
        file.write_bytes('Ä ä A a'.encode())
        p = subprocess.run([PROGRAM, *PREFIX, '--json', '-i', 'file', 'Ä', 'X'],
                           cwd=self.cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           env={**os.environ, 'LC_ALL': 'C.UTF-8'}, timeout=10)
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        self.assertEqual(json.loads(p.stdout)['matches'], 1)
        self.assertEqual(file.read_bytes(), 'X ä A a'.encode())

    def test_no_match_output_failure_preserves_source(self):
        file = self.cwd / 'file'
        file.write_bytes(b'unchanged')
        before = file.stat()
        with open('/dev/full', 'wb', buffering=0) as output:
            p = self.run_cli('--json', 'file', 'absent', 'new', output=output)
        self.assertEqual(p.returncode, 1)
        self.assertTrue(p.stderr)
        self.assertEqual(file.read_bytes(), b'unchanged')
        self.assertEqual(file.stat().st_ino, before.st_ino)
        self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
        self.assertEqual(file.stat().st_ctime_ns, before.st_ctime_ns)
        self.assertEqual(sorted(p.name for p in self.cwd.iterdir()), ['file'])


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ReplaceTests)
    if ARGS.slice != 'all':
        suite = unittest.TestSuite(t for t in suite if ARGS.slice in t.id())
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(not result.wasSuccessful())
