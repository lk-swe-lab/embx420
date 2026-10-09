#!/usr/bin/env python3
"""Independent byte and filesystem expectations for the selected cp contract."""
import argparse
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
program = str(Path(args.program).resolve())

class CopyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'])
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def run_cp(self, *argv, status=0):
        p = subprocess.run([program, *argv], cwd=self.root,
                           env={**os.environ, 'LC_ALL': 'C'},
                           capture_output=True, timeout=15)
        self.assertEqual(p.returncode, status, (argv, p.stdout, p.stderr))
        if status == 0:
            self.assertEqual(p.stderr, b'')
        else:
            self.assertTrue(p.stderr)
        return p.stdout

    def test_1_regular_bytes_sizing_and_same_inode(self):
        source, dest = self.root/'source', self.root/'dest'
        for size in (0, 1, 4095, 4096, 4097, 1048575, 1048576,
                     1048577, 8388641):
            payload = (bytes(range(256)) * (size//256+1))[:size]
            source.write_bytes(payload)
            dest.write_bytes(b'old trailing bytes' * 900)
            inode = dest.stat().st_ino
            self.assertEqual(self.run_cp('source', 'dest'), b'')
            self.assertEqual(dest.read_bytes(), payload)
            self.assertEqual(source.read_bytes(), payload)
            self.assertEqual(dest.stat().st_ino, inode)
        os.link(source, self.root/'alias')
        for name in ('source', 'alias'):
            self.run_cp('source', name, status=1)
            self.assertEqual(source.read_bytes(), payload)
        dest.unlink()
        self.run_cp('source', 'dest')
        self.assertEqual(dest.read_bytes(), payload)

    def test_2_parser_help_json_and_no_mutations(self):
        if args.reference:
            return
        import json
        (self.root/'source').write_bytes(b'\0\xffpayload')
        (self.root/'dest').write_bytes(b'untouched')
        for argv in ((), ('source',), ('source', 'dest', 'extra'),
                     ('--bad', 'source', 'dest'), ('', 'dest'),
                     ('source', ''), ('--json', '\udcff', 'dest')):
            self.run_cp(*argv, status=2)
            self.assertEqual((self.root/'dest').read_bytes(), b'untouched')
        help_bytes = self.run_cp('--help')
        self.assertIn(b'Usage: cp', help_bytes)
        self.assertEqual(help_bytes.splitlines()[0],
                         b'Usage: cp [-hrf] [--json] SOURCE DEST')
        self.assertEqual(self.run_cp('-h', 'source', 'dest'), help_bytes)
        self.assertEqual((self.root/'dest').read_bytes(), b'untouched')
        self.assertEqual(json.loads(self.run_cp('--json', 'source', 'dest')),
                         {'source': 'source', 'destination': 'dest',
                          'status': 'ok'})
        self.assertEqual(json.loads(self.run_cp('--json', 'missing', 'dest',
                                               status=1)),
                         {'source': 'missing', 'destination': 'dest',
                          'status': 'error'})
        (self.root/'-').write_bytes(b'literal')
        self.run_cp('--', '-', 'dest')
        self.assertEqual((self.root/'dest').read_bytes(), b'literal')
        with open('/dev/full', 'wb') as out:
            p = subprocess.run([program, '--json', 'source', 'dest'],
                               cwd=self.root, stdout=out,
                               stderr=subprocess.PIPE, timeout=5)
            self.assertEqual(p.returncode, 1)
            self.assertTrue(p.stderr)

    def test_3_directory_links_types_and_modes(self):
        source = self.root/'source'
        source.write_bytes(b'new\0bytes')
        source.chmod(0o751)
        (self.root/'directory').mkdir()
        self.run_cp('source', 'directory')
        self.assertEqual((self.root/'directory/source').read_bytes(), b'new\0bytes')
        (self.root/'link').symlink_to('source')
        self.run_cp('link', 'copy')
        self.assertFalse((self.root/'copy').is_symlink())
        (self.root/'destlink').symlink_to('copy')
        self.run_cp('source', 'destlink')
        self.assertTrue((self.root/'destlink').is_symlink())
        (self.root/'dangling').symlink_to('absent')
        self.run_cp('dangling', 'copy', status=1)
        self.run_cp('source', 'dangling', status=1)
        self.assertFalse((self.root/'absent').exists())
        os.mkfifo(self.root/'fifo')
        cases = [('directory', 'copy'), ('missing', 'copy')]
        if not args.reference:
            cases += [('fifo', 'copy'), ('source', 'fifo')]
        for argv in cases:
            self.run_cp(*argv, status=1)
        (self.root/'copy').chmod(0o640)
        os.link(self.root/'copy', self.root/'harddest')
        inode = (self.root/'copy').stat().st_ino
        self.run_cp('source', 'copy')
        self.assertEqual((self.root/'harddest').read_bytes(), b'new\0bytes')
        self.assertEqual((self.root/'copy').stat().st_ino, inode)
        self.assertEqual((self.root/'copy').stat().st_mode & 0o777, 0o640)
        p = subprocess.run([program, 'source', 'mode'], cwd=self.root,
                           capture_output=True, umask=0o027, timeout=5)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual((self.root/'mode').stat().st_mode & 0o777, 0o750)
        self.run_cp('-f', 'source', 'source', status=1)
        self.assertEqual(source.read_bytes(), b'new\0bytes')
        if not args.reference:
            self.run_cp('-f', 'source', 'dangling', status=1)
            self.assertFalse((self.root/'absent').exists())

    def test_4_recursive_links_guards_and_partial_errors(self):
        tree = self.root/'tree'
        (tree/'nested/empty').mkdir(parents=True)
        (tree/'.hidden').write_bytes(b'dot')
        (tree/'nested/data').write_bytes(b'\xff\0tree')
        (tree/'link').symlink_to('nested/data')
        (tree/'dangling').symlink_to('absent')
        (tree/'cycle').symlink_to('.')
        tree.chmod(0o751)
        self.run_cp('-r', 'tree', 'new')
        self.assertEqual((self.root/'new/nested/data').read_bytes(), b'\xff\0tree')
        self.assertEqual((self.root/'new/.hidden').read_bytes(), b'dot')
        self.assertTrue((self.root/'new/nested/empty').is_dir())
        for name, target in [('link', 'nested/data'), ('dangling', 'absent'),
                             ('cycle', '.')]:
            self.assertEqual(os.readlink(self.root/'new'/name), target)
        (self.root/'container').mkdir()
        self.run_cp('--recursive', 'tree/', 'container')
        self.assertTrue((self.root/'container/tree/nested/empty').is_dir())
        (self.root/'new/nested/data').write_bytes(b'old')
        self.run_cp('-r', 'tree', 'new-parent' )
        self.run_cp('-r', 'tree', 'tree/inside', status=1)
        if not args.reference:
            self.assertFalse((tree/'inside').exists())
        self.run_cp('-r', 'tree', 'tree', status=1)
        (self.root/'alias').symlink_to('tree')
        self.run_cp('-r', 'tree', 'alias/inside2', status=1)
        if not args.reference:
            self.assertFalse((tree/'inside2').exists())
        # Link source is copied as a link; same link must never be unlinked.
        self.run_cp('-r', 'tree/link', 'linkcopy')
        self.assertEqual(os.readlink(self.root/'linkcopy'), 'nested/data')
        self.run_cp('-r', 'tree/link', 'tree/link', status=1)
        self.assertEqual(os.readlink(tree/'link'), 'nested/data')
        if not args.reference:
            os.mkfifo(tree/'bad')
            self.run_cp('-r', 'tree', 'partial', status=1)
            self.assertEqual((self.root/'partial/.hidden').read_bytes(), b'dot')
            self.assertFalse((self.root/'partial/bad').exists())
            (self.root/'dirlink').symlink_to('new')
            self.run_cp('-r', 'tree/nested', 'dirlink', status=1)
        p = subprocess.run([program, '-r', 'tree', 'masked'], cwd=self.root,
                           capture_output=True, umask=0o027, timeout=5)
        self.assertEqual(p.returncode, 0 if args.reference else 1)
        self.assertEqual((self.root/'masked').stat().st_mode & 0o777, 0o750)

    def test_5_permissions_force_and_seeded_no_mutations(self):
        source, dest = self.root/'source', self.root/'dest'
        source.write_bytes(b'force content')
        dest.write_bytes(b'original')
        dest.chmod(0o400)
        inode = dest.stat().st_ino
        self.run_cp('source', 'dest', status=1)
        self.assertEqual(dest.read_bytes(), b'original')
        self.assertEqual(dest.stat().st_ino, inode)
        os.link(dest, self.root/'original-hardlink')
        self.run_cp('-f', 'source', 'dest')
        self.assertEqual((self.root/'original-hardlink').read_bytes(), b'original')
        self.assertEqual(dest.read_bytes(), b'force content')
        self.assertNotEqual(dest.stat().st_ino, inode)
        source.chmod(0)
        self.addCleanup(source.chmod, 0o600)
        self.run_cp('source', 'dest', status=1)
        self.assertEqual(dest.read_bytes(), b'force content')
        source.chmod(0o600)
        locked = self.root/'locked'
        locked.mkdir()
        (locked/'dest').write_bytes(b'unchanged')
        (locked/'dest').chmod(0o400)
        locked.chmod(0o500)
        self.addCleanup(locked.chmod, 0o700)
        self.run_cp('-f', 'source', 'locked/dest', status=1)
        self.assertEqual((locked/'dest').read_bytes(), b'unchanged')
        if not args.reference:
            (self.root/'alias').symlink_to('dest')
            dest.chmod(0o400)
            self.run_cp('-f', 'source', 'alias', status=1)
            self.assertEqual(dest.read_bytes(), b'force content')
            self.assertTrue((self.root/'alias').is_symlink())
            dest.chmod(0o600)
        import random
        rng = random.Random(420)
        for i in range(40):
            name = f'{i}_space "quote"\t-é'
            payload = rng.randbytes(rng.randrange(0, 10000))
            p = self.root/name
            p.write_bytes(payload)
            before = p.stat()
            self.run_cp('--', name, 'dest')
            self.assertEqual(dest.read_bytes(), payload)
            self.assertEqual(p.read_bytes(), payload)
            after = p.stat()
            self.assertEqual((after.st_ino, after.st_mtime_ns, after.st_mode),
                             (before.st_ino, before.st_mtime_ns, before.st_mode))
        if not args.reference:
            import json
            source.write_bytes(b'json')
            name = 'new "quoted"\t-é'
            self.assertEqual(json.loads(self.run_cp('--json', 'source', name)),
                             {'source': 'source', 'destination': name,
                              'status': 'ok'})

    def test_6_recursive_merge_readonly_modes_and_depth(self):
        source = self.root/'tree'
        (source/'nested').mkdir(parents=True)
        (source/'nested/a').write_bytes(b'copied')
        source.chmod(0o500)
        self.addCleanup(source.chmod, 0o700)
        self.run_cp('-r', 'tree', 'copy')
        copied = self.root/'copy'
        self.addCleanup(copied.chmod, 0o700)
        self.assertEqual(copied.stat().st_mode & 0o777, 0o500)
        self.assertEqual((copied/'nested/a').read_bytes(), b'copied')
        source.chmod(0o700)
        # Existing destination directories preserve their mode on merges.
        container = self.root/'container'
        (container/'tree/nested').mkdir(parents=True)
        (container/'tree/retained').write_bytes(b'existing')
        (container/'tree/nested/a').write_bytes(b'older longer data')
        (container/'tree').chmod(0o750)
        self.run_cp('-r', 'tree', 'container')
        self.assertEqual((container/'tree/nested/a').read_bytes(), b'copied')
        self.assertEqual((container/'tree/retained').read_bytes(), b'existing')
        self.assertEqual((container/'tree').stat().st_mode & 0o777, 0o750)
        if not args.reference:
            deep = self.root/'deep'
            deep.mkdir()
            current = deep
            for i in range(129):
                current = current/'d'
                current.mkdir()
            self.run_cp('-r', 'deep', 'deepcopy', status=1)
            with open('/dev/full', 'wb') as output:
                p = subprocess.run([program, '--help'], stdout=output,
                                   stderr=subprocess.PIPE, timeout=5)
                self.assertEqual(p.returncode, 1)

    def test_7_large_bounded_memory_and_resource_errors(self):
        import hashlib
        import resource
        import signal
        source = self.root/'large'
        length = 64 * 1024 * 1024 + 1
        with source.open('wb') as f:
            f.truncate(length)
            for offset in (0, 1048575, 1048576, length-1):
                f.seek(offset)
                f.write(b'X')
        def memory_limit():
            resource.setrlimit(resource.RLIMIT_AS, (32*1024*1024, 32*1024*1024))
        # Sanitizer runtime needs a huge virtual reservation, not a copy leak.
        # Run bounded-AS independently only for non-sanitized binary.
        sanitized = b'libasan' in subprocess.run(
            ['ldd', program], capture_output=True, timeout=5).stdout
        p = subprocess.run([program, 'large', 'copy'], cwd=self.root,
                           capture_output=True, timeout=30,
                           preexec_fn=None if sanitized else memory_limit)
        self.assertEqual(p.returncode, 0, p.stderr)
        def digest(path):
            h = hashlib.sha256()
            with path.open('rb') as f:
                while chunk := f.read(1048576):
                    h.update(chunk)
            return h.digest()
        self.assertEqual((self.root/'copy').stat().st_size, length)
        self.assertEqual(digest(source), digest(self.root/'copy'))
        if not args.reference:
            (self.root/'dest').write_bytes(b'original')
            def file_limit():
                resource.setrlimit(resource.RLIMIT_FSIZE, (1024, 1024))
                signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            p = subprocess.run([program, 'large', 'dest'], cwd=self.root,
                               capture_output=True, timeout=5,
                               preexec_fn=file_limit)
            self.assertEqual(p.returncode, 1, p.stderr)
            self.assertEqual((self.root/'dest').read_bytes(), b'original')

suite = unittest.defaultTestLoader.loadTestsFromTestCase(CopyTests)
suite = unittest.TestSuite(t for t in suite
                          if int(t._testMethodName.split('_')[1]) <= args.slice)
result = unittest.TextTestRunner(verbosity=2).run(suite)
raise SystemExit(not result.wasSuccessful())
