#!/usr/bin/env python3
"""Independent byte/inode expectations; only disposable scratch fixtures."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('program')
parser.add_argument('--reference', action='store_true')
parser.add_argument('--slice', type=int, default=4)
args = parser.parse_args()
PROGRAM = str(Path(args.program).resolve())


class MoveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'])
        self.addCleanup(self.temp.cleanup)
        self.cwd = Path(self.temp.name)

    def run_mv(self, *arguments, **kwargs):
        return subprocess.run([PROGRAM, *arguments], cwd=self.cwd,
                              input=b'', capture_output=True, timeout=5,
                              env={**os.environ, 'LC_ALL': 'C'}, **kwargs)

    def test_file_rename(self):
        payload = b'\x00\xffhello\r\n'
        src = self.cwd / 'source'
        src.write_bytes(payload)
        src.chmod(0o640)
        os.utime(src, ns=(123456789000, 987654321000))
        before = src.stat()
        p = self.run_mv('source', 'dest')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertFalse(src.exists(), 'rename must remove source name')
        dst = self.cwd / 'dest'
        self.assertEqual(dst.read_bytes(), payload)
        after = dst.stat()
        self.assertEqual((after.st_ino, after.st_dev, after.st_mode,
                          after.st_uid, after.st_gid, after.st_mtime_ns),
                         (before.st_ino, before.st_dev, before.st_mode,
                          before.st_uid, before.st_gid, before.st_mtime_ns))
        self.assertEqual(sorted(x.name for x in self.cwd.iterdir()), ['dest'])
    @unittest.skipUnless(args.slice >= 2, 'later vertical slice')
    def test_directory_destination_and_links(self):
        (self.cwd / 'target').mkdir()
        (self.cwd / 'tree').mkdir()
        (self.cwd / 'tree' / 'child').write_bytes(b'child\x00')
        ino = (self.cwd / 'tree').stat().st_ino
        p = self.run_mv('tree/', 'target/')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertFalse((self.cwd / 'tree').exists())
        self.assertTrue((self.cwd / 'target/tree').exists(),
                        'directory DEST must receive source basename')
        self.assertEqual((self.cwd / 'target/tree').stat().st_ino, ino)
        self.assertEqual((self.cwd / 'target/tree/child').read_bytes(), b'child\x00')
        os.symlink('absent', self.cwd / 'dangling')
        p = self.run_mv('dangling', 'target')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual(os.readlink(self.cwd / 'target/dangling'), 'absent')
        self.assertFalse(os.path.lexists(self.cwd / 'dangling'))
        os.symlink('target', self.cwd / 'alias')
        (self.cwd / 'file').write_bytes(b'data')
        p = self.run_mv('file', 'alias')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual((self.cwd / 'target/file').read_bytes(), b'data')
        self.assertTrue((self.cwd / 'alias').is_symlink())

    @unittest.skipUnless(args.slice >= 2, 'later vertical slice')
    def test_links_and_special_source_without_follow(self):
        (self.cwd / 'referent').write_bytes(b'untouched')
        os.symlink('referent', self.cwd / 'link')
        ino = (self.cwd / 'link').lstat().st_ino
        p = self.run_mv('link', 'moved')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual(os.readlink(self.cwd / 'moved'), 'referent')
        self.assertEqual((self.cwd / 'moved').lstat().st_ino, ino)
        self.assertEqual((self.cwd / 'referent').read_bytes(), b'untouched')
        os.mkfifo(self.cwd / 'fifo')
        ino = (self.cwd / 'fifo').lstat().st_ino
        p = self.run_mv('fifo', 'newfifo')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual((self.cwd / 'newfifo').lstat().st_ino, ino)
    @unittest.skipUnless(args.slice >= 3, 'later vertical slice')
    def test_force_overwrites_entry_not_referent(self):
        (self.cwd / 'source').write_bytes(b'new')
        (self.cwd / 'referent').write_bytes(b'old')
        os.symlink('referent', self.cwd / 'dest')
        ino = (self.cwd / 'source').stat().st_ino
        p = self.run_mv('-f', 'source', 'dest')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertFalse((self.cwd / 'source').exists())
        self.assertFalse((self.cwd / 'dest').is_symlink())
        self.assertEqual((self.cwd / 'dest').read_bytes(), b'new')
        self.assertEqual((self.cwd / 'dest').stat().st_ino, ino)
        self.assertEqual((self.cwd / 'referent').read_bytes(), b'old')
        (self.cwd / 'source').mkdir()
        (self.cwd / 'container').mkdir()
        (self.cwd / 'container/source').mkdir()
        ino = (self.cwd / 'source').stat().st_ino
        p = self.run_mv('-f', 'source', 'container')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual((self.cwd / 'container/source').stat().st_ino, ino)

    @unittest.skipUnless(args.slice >= 3 and not args.reference,
                         'project overwrite policy')
    def test_refuse_existing_and_same_inode(self):
        src = self.cwd / 'source'
        src.write_bytes(b'new')
        dst = self.cwd / 'dest'
        dst.write_bytes(b'old')
        for target in ['dest', 'source']:
            p = self.run_mv('source', target)
            self.assertEqual(p.returncode, 1, 'existing target must be refused')
            self.assertEqual(p.stdout, b'')
            self.assertTrue(p.stderr.startswith(b'mv: '))
            self.assertEqual(src.read_bytes(), b'new')
            self.assertEqual(dst.read_bytes(), b'old')
        os.link(src, self.cwd / 'hardlink')
        p = self.run_mv('-f', 'source', 'hardlink')
        self.assertEqual(p.returncode, 1, 'same inode must be refused')
        self.assertEqual(src.stat().st_nlink, 2)
        os.symlink('missing', self.cwd / 'dangling')
        p = self.run_mv('source', 'dangling')
        self.assertEqual(p.returncode, 1)
        self.assertEqual(os.readlink(self.cwd / 'dangling'), 'missing')
        self.assertEqual(src.read_bytes(), b'new')
    @unittest.skipUnless(args.slice >= 4 and not args.reference,
                         'project parser/JSON')
    def test_help_and_argument_preflight(self):
        help_result = self.run_mv('--help')
        self.assertEqual(help_result.returncode, 0)
        self.assertEqual(help_result.stderr, b'')
        self.assertIn(b'Usage: mv', help_result.stdout)
        self.assertIn(b'Existing targets require -f', help_result.stdout)
        self.assertEqual(self.run_mv('-h').stdout, help_result.stdout)
        (self.cwd / 'source').write_bytes(b'preserve')
        for operands in [[], ['source'], ['source', 'x', 'extra'],
                         ['--bogus'], ['--help', '--bogus'],
                         ['', 'x'], ['source', ''], ['-n', 'source', 'x'],
                         ['--json', b'\xff', 'x'],
                         ['--json', 'source', b'\xff']]:
            p = self.run_mv(*operands)
            self.assertEqual(p.returncode, 2, operands)
            self.assertEqual(p.stdout, b'')
            self.assertTrue(p.stderr.startswith(b'mv: '))
            self.assertEqual((self.cwd / 'source').read_bytes(), b'preserve')
            self.assertEqual(sorted(x.name for x in self.cwd.iterdir()), ['source'])

    @unittest.skipUnless(args.slice >= 4 and not args.reference,
                         'project JSON')
    def test_json_status_original_paths_and_encoding(self):
        import json
        name = 'quote"\n\t\\é'
        (self.cwd / name).write_bytes(b'payload')
        (self.cwd / 'container').mkdir()
        p = self.run_mv('--json', name, 'container')
        expected = {'source': name, 'destination': 'container', 'status': 'ok'}
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stderr, b'')
        self.assertEqual(json.loads(p.stdout), expected)
        self.assertEqual(p.stdout.count(b'\n'), 1)
        self.assertEqual((self.cwd / 'container' / name).read_bytes(), b'payload')
        p = self.run_mv('--json', 'missing', 'dest')
        self.assertEqual(p.returncode, 1)
        self.assertEqual(json.loads(p.stdout),
                         {'source': 'missing', 'destination': 'dest', 'status': 'error'})
        fd = os.open(os.fsencode(self.cwd) + b'/\xff',
                     os.O_CREAT | os.O_WRONLY, 0o600)
        os.close(fd)
        p = self.run_mv('--', b'\xff', b'\xfe')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertTrue(os.path.exists(os.fsencode(self.cwd) + b'/\xfe'))

    @unittest.skipUnless(args.slice >= 4, 'later vertical slice')
    def test_errors_and_no_unexpected_namespace_effects(self):
        (self.cwd / 'source').write_bytes(b'preserve')
        (self.cwd / 'dir').mkdir()
        (self.cwd / 'dir/source').mkdir()
        (self.cwd / 'tree').mkdir()
        (self.cwd / 'tree/child').write_bytes(b'keep')
        (self.cwd / 'container').mkdir()
        (self.cwd / 'container/tree').mkdir()
        (self.cwd / 'container/tree/child').write_bytes(b'old')
        for operands in [('missing', 'dest'), ('source', 'missing/dest'),
                         ('source/', 'dest'), ('source', 'dest/'),
                         ('-f', 'source', 'dir'), ('-f', 'tree', 'source'),
                         ('-f', 'tree', 'container'), ('-f', 'tree', 'tree/sub')]:
            before = {str(x.relative_to(self.cwd)): x.lstat().st_ino
                      for x in self.cwd.rglob('*')}
            p = self.run_mv(*operands)
            self.assertNotEqual(p.returncode, 0, operands)
            self.assertEqual(p.stdout, b'')
            self.assertTrue(p.stderr)
            after = {str(x.relative_to(self.cwd)): x.lstat().st_ino
                     for x in self.cwd.rglob('*')}
            self.assertEqual(after, before, operands)
            self.assertEqual((self.cwd / 'source').read_bytes(), b'preserve')
            self.assertEqual((self.cwd / 'tree/child').read_bytes(), b'keep')
            self.assertEqual((self.cwd / 'container/tree/child').read_bytes(), b'old')

    @unittest.skipUnless(args.slice >= 4 and not args.reference,
                         'deliberate source trailing-link refusal')
    def test_trailing_link_dot_self_and_missing_target(self):
        (self.cwd / 'tree').mkdir()
        (self.cwd / 'tree/child').write_bytes(b'keep')
        os.symlink('tree', self.cwd / 'link')
        for operands in [('link/', 'new'), ('.', 'new'), ('..', 'new'),
                         ('tree/.', 'new'), ('tree/..', 'new'),
                         ('-f', 'tree', 'tree')]:
            p = self.run_mv(*operands)
            self.assertEqual(p.returncode, 1, operands)
            self.assertTrue((self.cwd / 'tree').exists())
            self.assertFalse((self.cwd / 'new').exists())
            self.assertEqual(os.readlink(self.cwd / 'link'), 'tree')

    @unittest.skipUnless(args.slice >= 4, 'later vertical slice')
    def test_unusual_option_names_and_hardlinks(self):
        src = self.cwd / '-f'
        src.write_bytes(b'keep')
        os.link(src, self.cwd / 'other')
        ino = src.stat().st_ino
        p = self.run_mv('--', '-f', '-destination')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual((self.cwd / '-destination').stat().st_ino, ino)
        self.assertEqual((self.cwd / 'other').stat().st_nlink, 2)
        self.assertFalse(src.exists())

    @unittest.skipUnless(args.slice >= 4 and not args.reference,
                         'project output failure')
    def test_postmutation_output_failure_and_help(self):
        (self.cwd / 'source').write_bytes(b'keep')
        with open('/dev/full', 'wb') as out:
            p = subprocess.run([PROGRAM, '--json', 'source', 'dest'],
                               cwd=self.cwd, stdout=out, stderr=subprocess.PIPE,
                               timeout=5)
            self.assertEqual(p.returncode, 1)
            self.assertIn(b'mv:', p.stderr)
            self.assertFalse((self.cwd / 'source').exists())
            self.assertEqual((self.cwd / 'dest').read_bytes(), b'keep')
            p = subprocess.run([PROGRAM, '--help'], stdout=out,
                               stderr=subprocess.PIPE, timeout=5)
            self.assertEqual(p.returncode, 1)

    @unittest.skipUnless(args.slice >= 4, 'later vertical slice')
    def test_permission_failure(self):
        self.assertNotEqual(os.geteuid(), 0, 'permissions require nonroot fixture')
        (self.cwd / 'source').write_bytes(b'keep')
        (self.cwd / 'locked').mkdir()
        (self.cwd / 'locked').chmod(0o500)
        try:
            p = self.run_mv('source', 'locked')
            self.assertEqual(p.returncode, 1)
            self.assertEqual(p.stdout, b'')
            self.assertTrue(p.stderr)
            self.assertEqual((self.cwd / 'source').read_bytes(), b'keep')
            self.assertEqual(list((self.cwd / 'locked').iterdir()), [])
        finally:
            (self.cwd / 'locked').chmod(0o700)
    @unittest.skipUnless(args.slice >= 4 and not args.reference,
                         'rename-only real cross-device failure')
    def test_real_exdev_preserves_source_and_existing_target(self):
        # A disposable memfd's proc link is on procfs, unlike fixture target.
        fd = os.memfd_create('mv-disposable-source')
        try:
            os.write(fd, b'keep')
            source = f'/proc/{os.getpid()}/fd/{fd}'
            before = os.lstat(source)
            (self.cwd / 'dest').write_bytes(b'old')
            ino = (self.cwd / 'dest').stat().st_ino
            p = self.run_mv('-f', source, 'dest')
            self.assertEqual(p.returncode, 1)
            self.assertEqual(p.stdout, b'')
            self.assertIn(b'Invalid cross-device link', p.stderr)
            self.assertEqual(os.pread(fd, 4, 0), b'keep')
            self.assertEqual(os.lstat(source).st_ino, before.st_ino)
            self.assertEqual((self.cwd / 'dest').read_bytes(), b'old')
            self.assertEqual((self.cwd / 'dest').stat().st_ino, ino)
        finally:
            os.close(fd)

    @unittest.skipUnless(args.slice >= 4, 'later vertical slice')
    def test_seeded_raw_name_namespace_properties(self):
        import random
        rng = random.Random(420)
        for i in range(100):
            name = 's' + ''.join(rng.choice('ab \n\té"\\') for _ in range(18))
            dest = 'd' + str(i)
            data = bytes(rng.randrange(256) for _ in range(rng.randrange(0, 8193)))
            (self.cwd / name).write_bytes(data)
            before = (self.cwd / name).stat()
            p = self.run_mv('--', name, dest)
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
            self.assertFalse((self.cwd / name).exists())
            self.assertEqual((self.cwd / dest).read_bytes(), data)
            self.assertEqual((self.cwd / dest).stat().st_ino, before.st_ino)
            (self.cwd / dest).unlink()
            self.assertEqual(list(self.cwd.iterdir()), [])
    @unittest.skipUnless(args.slice >= 4, 'later vertical slice')
    def test_force_overwrites_cyclic_final_link(self):
        (self.cwd / 'source').write_bytes(b'new')
        os.symlink('loop', self.cwd / 'loop')
        p = self.run_mv('-f', 'source', 'loop')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertFalse((self.cwd / 'source').exists())
        self.assertFalse((self.cwd / 'loop').is_symlink())
        self.assertEqual((self.cwd / 'loop').read_bytes(), b'new')
        (self.cwd / 'source').write_bytes(b'new2')
        os.symlink('loop/child', self.cwd / 'bad-prefix-link')
        p = self.run_mv('-f', 'source', 'bad-prefix-link')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertEqual((self.cwd / 'bad-prefix-link').read_bytes(), b'new2')
        self.assertEqual((self.cwd / 'loop').read_bytes(), b'new')
        (self.cwd / 'source').write_bytes(b'preserve')
        p = self.run_mv('-f', 'source', 'loop/child')
        self.assertEqual(p.returncode, 1)
        self.assertEqual(p.stdout, b'')
        self.assertTrue(p.stderr)
        self.assertEqual((self.cwd / 'source').read_bytes(), b'preserve')
        self.assertEqual((self.cwd / 'loop').read_bytes(), b'new')


if __name__ == '__main__':
    unittest.main(argv=['test_mv'])
