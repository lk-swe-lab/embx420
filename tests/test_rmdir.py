#!/usr/bin/env python3
"""Independent byte/namespace oracles; fixtures only under scratch."""
import argparse
import errno
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import unittest

P = argparse.ArgumentParser()
P.add_argument('program')
P.add_argument('--reference', action='store_true')
P.add_argument('--slice', type=int, default=5)
A = P.parse_args()
PROGRAM = os.fsencode(Path(A.program).resolve())


class RmdirTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'])
        self.addCleanup(self.temp.cleanup)
        self.cwd = Path(self.temp.name)

    def run_cli(self, *args, **kwargs):
        return subprocess.run([PROGRAM, *map(os.fsencode, args)],
                              cwd=self.cwd, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, timeout=5,
                              env={**os.environ, 'LC_ALL': 'C'}, **kwargs)

    def test_empty(self):
        (self.cwd / 'empty').mkdir()
        p = self.run_cli('empty')
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
        self.assertFalse((self.cwd / 'empty').exists(), 'RMDIR_NOT_REMOVED')

    def test_nonempty_types_missing_continue(self):
        (self.cwd / 'full').mkdir()
        (self.cwd / 'full' / 'keep').write_bytes(b'keep\0\xff')
        (self.cwd / 'file').write_bytes(b'file')
        os.mkfifo(self.cwd / 'fifo')
        sock = socket.socket(socket.AF_UNIX)
        self.addCleanup(sock.close)
        sock.bind(str(self.cwd / 'socket'))
        (self.cwd / 'last').mkdir()
        before = {name: os.lstat(self.cwd / name) for name in
                  ['file', 'fifo', 'socket', 'full/keep']}
        p = self.run_cli('full', 'file', 'fifo', 'socket', 'missing', 'last')
        self.assertEqual(p.returncode, 1)
        self.assertEqual(p.stdout, b'')
        self.assertTrue(p.stderr)
        self.assertFalse((self.cwd / 'last').exists())
        self.assertEqual((self.cwd / 'full/keep').read_bytes(), b'keep\0\xff')
        for name, st in before.items():
            now = os.lstat(self.cwd / name)
            self.assertEqual((now.st_ino, now.st_mode, now.st_mtime_ns),
                             (st.st_ino, st.st_mode, st.st_mtime_ns))

    def test_explicit_order_duplicate_slashes_bytes(self):
        (self.cwd / 'a/b').mkdir(parents=True)
        p = self.run_cli('a//b///', 'a', 'a')
        self.assertEqual(p.returncode, 1)
        self.assertEqual(p.stdout, b'')
        self.assertTrue(p.stderr)
        self.assertFalse((self.cwd / 'a').exists())
        for name in [b'-dash', b'with \n\t"\\', b'bad\xff', b'.hidden']:
            os.mkdir(os.fsencode(self.cwd) + b'/' + name)
            p = self.run_cli('--', name)
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))
            self.assertFalse(os.path.exists(os.fsencode(self.cwd)+b'/'+name))

    @unittest.skipIf(A.reference or A.slice < 2, 'project parser/JSON slice')
    def test_help_arguments_json(self):
        p = self.run_cli('--help')
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        self.assertIn(b'Remove explicit empty directories', p.stdout)
        self.assertEqual(self.run_cli('-h').stdout, p.stdout)
        self.assertEqual(self.run_cli('--help', 'unused', 'more').stdout,p.stdout)
        for args in [[], ['-p'], ['--parents'], ['-v'], ['--version'],
                     ['--ignore-fail-on-non-empty'], ['--help', '--bad'], ['']]:
            x = self.run_cli(*args)
            self.assertEqual((x.returncode, x.stdout), (2, b''), args)
            self.assertTrue(x.stderr)
        names = ['one\n"\\', 'two', 'one\n"\\', 'missing']
        for n in names[:2]:
            (self.cwd / n).mkdir()
        p = self.run_cli('--json', *names)
        self.assertEqual(p.returncode, 1)
        expected = [{'path': n, 'status': 'ok' if i<2 else 'error',
                     'errno': 0 if i<2 else errno.ENOENT}
                    for i, n in enumerate(names)]
        self.assertEqual(json.loads(p.stdout), expected)
        self.assertEqual(p.stdout, (json.dumps(expected,separators=(',',':'))+'\n').encode())
        self.assertTrue(p.stderr)

    @unittest.skipIf(A.reference or A.slice < 2, 'JSON Unicode/capacity')
    def test_json_unicode_capacity_and_malformed_scalars(self):
        names = ['entry-' + chr(c) for c in
                 [1, 8, 9, 10, 12, 13, 31, 34, 92, 127, 128, 2047,
                  2048, 55295, 57344, 65535, 65536, 1114111]]
        names += ['many-' + str(i) for i in range(300)]
        for name in names:
            (self.cwd / name).mkdir()
        p = self.run_cli('--json', *names)
        expected = [{'path': n, 'status': 'ok', 'errno': 0} for n in names]
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        self.assertEqual(p.stdout, (json.dumps(expected, ensure_ascii=False,
                          separators=(',', ':')) + '\n').encode())
        self.assertFalse(any((self.cwd / n).exists() for n in names))
        (self.cwd / 'valid').mkdir()
        for invalid in [b'\x80', b'\xc0\xaf', b'\xed\xa0\x80', b'\xe2\x82',
                        b'\xf0\x8f\xbf\xbf', b'\xf4\x90\x80\x80',
                        b'\xf5\x80\x80\x80', b'\xf0\x90\x80']:
            p = self.run_cli('--json', 'valid', invalid)
            self.assertEqual((p.returncode, p.stdout), (2, b''))
            self.assertTrue((self.cwd / 'valid').exists())

    @unittest.skipIf(A.reference or A.slice < 2, 'project encoding slice')
    def test_validate_all_before_effects_and_output(self):
        (self.cwd / 'valid').mkdir()
        for tail in [b'', b'bad\xff']:
            p = self.run_cli('--json', 'valid', tail)
            self.assertEqual((p.returncode, p.stdout), (2, b''))
            self.assertTrue((self.cwd / 'valid').exists())
        with open(self.cwd / 'readonly', 'wb') as f:
            f.write(b'keep')
        with open(self.cwd / 'readonly', 'rb') as f:
            p = subprocess.run([PROGRAM, b'--json', b'valid'],cwd=self.cwd,
                               stdout=f,stderr=subprocess.PIPE,timeout=5)
        self.assertEqual(p.returncode, 1)
        self.assertTrue((self.cwd / 'valid').exists())
        p = subprocess.run([PROGRAM, b'--json', b'valid'], cwd=self.cwd,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                           preexec_fn=lambda: os.close(1), timeout=5)
        self.assertEqual(p.returncode, 1)
        self.assertTrue((self.cwd / 'valid').exists())
        with open('/dev/full', 'wb', buffering=0) as f:
            p = subprocess.run([PROGRAM,b'--json',b'valid'],cwd=self.cwd,
                               stdout=f,stderr=subprocess.PIPE,timeout=5)
        self.assertEqual(p.returncode, 1)
        self.assertFalse((self.cwd / 'valid').exists())
        with open('/dev/full', 'wb', buffering=0) as f:
            p = subprocess.run([PROGRAM,b'--help'],stdout=f,
                               stderr=subprocess.PIPE,timeout=5)
        self.assertEqual(p.returncode,1)

    @unittest.skipIf(A.reference or A.slice < 2, 'project output signal policy')
    def test_broken_pipe_inherited_signal_policy(self):
        import signal
        for restore, expected in [(True, -signal.SIGPIPE), (False, 1)]:
            (self.cwd / 'empty').mkdir()
            read_fd, write_fd = os.pipe()
            os.close(read_fd)
            try:
                p = subprocess.run([PROGRAM, b'--json', b'empty'],
                                   cwd=self.cwd, stdout=write_fd,
                                   stderr=subprocess.PIPE, timeout=5,
                                   restore_signals=restore)
            finally:
                os.close(write_fd)
            self.assertEqual(p.returncode, expected)
            self.assertFalse((self.cwd / 'empty').exists())

    @unittest.skipIf(A.slice < 3, 'resolution slice')
    def test_final_links_and_dot_paths(self):
        (self.cwd / 'target').mkdir()
        for name,target in [('link','target'),('dangling','absent'),
                            ('cycle','cycle'),('filelink','file')]:
            os.symlink(target,self.cwd/name)
        (self.cwd / 'file').write_bytes(b'keep')
        for name in ['link','link/','link///','dangling','dangling/',
                     'cycle','cycle/','filelink','filelink/',
                     'target/.','target/..','.','..','/','///']:
            p = self.run_cli(name)
            self.assertEqual(p.returncode,1,name)
            self.assertEqual(p.stdout,b'')
            self.assertTrue(p.stderr)
            self.assertTrue((self.cwd/'target').is_dir())
        for name in ['link','dangling','cycle','filelink']:
            self.assertTrue((self.cwd/name).is_symlink())

    @unittest.skipIf(A.slice < 3, 'resolution slice')
    def test_intermediate_links_parent_components(self):
        (self.cwd/'real/a').mkdir(parents=True)
        (self.cwd/'real/b').mkdir()
        os.symlink('real',self.cwd/'via')
        p = self.run_cli('via/a/../b','via/./a')
        self.assertEqual((p.returncode,p.stdout,p.stderr),(0,b'',b''))
        self.assertTrue((self.cwd/'via').is_symlink())
        self.assertTrue((self.cwd/'real').exists())
        self.assertFalse((self.cwd/'real/a').exists())
        self.assertFalse((self.cwd/'real/b').exists())
        p = self.run_cli('file/child','missing/../real')
        self.assertEqual(p.returncode,1)

    @unittest.skipIf(A.reference or A.slice < 3, 'cwd protection differs from GNU')
    def test_cwd_alias_refusal(self):
        p = self.run_cli(str(self.cwd))
        self.assertEqual((p.returncode,p.stdout),(1,b''))
        self.assertTrue(self.cwd.exists())

    @unittest.skipIf(A.slice < 3, 'byte property slice')
    def test_every_single_byte_component(self):
        root = os.fsencode(self.cwd)
        for byte in range(1, 256):
            if byte == 47:
                continue
            name = b'entry-' + bytes([byte])
            os.mkdir(root + b'/' + name)
            p = self.run_cli('--', name)
            self.assertEqual((p.returncode, p.stdout, p.stderr),
                             (0, b'', b''), name)
            self.assertFalse(os.path.exists(root + b'/' + name))
        name = 'x' * 255
        (self.cwd / name).mkdir()
        p = self.run_cli(name)
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, b'', b''))

    @unittest.skipIf(A.slice < 3, 'permissions slice')
    def test_real_permissions_and_name_limits(self):
        (self.cwd/'locked/child').mkdir(parents=True)
        os.chmod(self.cwd/'locked',0o500)
        self.addCleanup(os.chmod,self.cwd/'locked',0o700)
        self.assertNotEqual(os.geteuid(),0,'non-root permission fixture required')
        p = self.run_cli('locked/child')
        self.assertEqual((p.returncode,p.stdout),(1,b''))
        self.assertTrue((self.cwd/'locked/child').exists())
        p = self.run_cli('x'*256)
        self.assertEqual((p.returncode,p.stdout),(1,b''))
        (self.cwd/'no-access-needed').mkdir(mode=0)
        p = self.run_cli('no-access-needed')
        self.assertEqual((p.returncode,p.stdout,p.stderr),(0,b'',b''))


unittest.main(argv=['rmdir-tests'],verbosity=2)
