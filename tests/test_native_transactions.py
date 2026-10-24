"""Fault the actual native pkg DSO; no legacy store transaction substitution."""
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest
import test_shared_package as shared

ROOT = shared.ROOT


class NativeTransactions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix='native-fault-')
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.release = Path(cls.tmp.name)
        cls.program = cls.release / 'aishell'
        shutil.copy2(shared.PROGRAM, cls.program)
        shutil.copytree(shared.PROGRAM.parent / 'modules', cls.release / 'modules')
        sources = ['src/module_entry.c', 'src/cmd_pkg.c', 'src/module_pkg.c',
                   'src/cmd_metadata.c', 'src/pkg_store.c', 'src/pkg_fs.c',
                   'src/pkg_json.c', 'src/pkg_schema.c', 'tests/native_pkg_faults.c']
        wraps = ['module_pkg_run',
                 'write', 'fsync', 'close', 'renameat', 'renameat2', 'unlinkat']
        flags = shlex.split(os.environ.get('AISHELL_FAULT_CFLAGS', ''))
        command = [
            'cc', '-std=c17', '-Wall', '-Wextra', '-Werror', '-g', '-fPIC',
            '-fvisibility=hidden', '-Isrc',
            '-Ibuild/module-deps/argtable3/include', *flags,
            '-DMODULE_HEADER="cmd_pkg.h"', '-DMODULE_SPEC=cmd_pkg_spec',
            '-DMODULE_BIND=pkg_bind', '-DMODULE_REFRESH=pkg_refresh',
            '-DAISHELL_NATIVE_PACKAGE']
        objects = []
        for index, source in enumerate(sources):
            output = cls.release / (str(index) + '.o')
            # Inject only project allocations, not Argtable internals whose
            # documented OOM behavior terminates the process.
            defines = ([] if source.endswith('native_pkg_faults.c') else
                       ['-D' + name + '=__wrap_' + name
                        for name in ('malloc', 'calloc', 'strdup', 'free')])
            subprocess.run([*command, *defines, '-c', source, '-o', str(output)],
                           cwd=ROOT, check=True)
            objects.append(str(output))
        subprocess.run(['cc', '-shared', *flags, *objects,
            '-Wl,-z,defs,--exclude-libs,ALL,' + ','.join('--wrap=' + w for w in wraps),
            '-Lbuild/module-deps/argtable3/lib', '-largtable3', '-lm',
            '-o', str(cls.release / 'modules/pkg.so')], cwd=ROOT, check=True)

    def setUp(self):
        self.tmp_case = tempfile.TemporaryDirectory(prefix='native-case-')
        self.addCleanup(self.tmp_case.cleanup)
        self.base = Path(self.tmp_case.name)
        self.home = self.base / 'home'
        self.home.mkdir()
        self.env = dict(os.environ, HOME=str(self.home), LC_ALL='C')
        self.source = self.base / 'newer'
        shutil.copytree(shared.PACKAGE, self.source)
        for file in ('pkg.json', 'metadata/commands.json'):
            path = self.source / file
            data = json.loads(path.read_bytes())
            data['version'] = '0.3.0'
            path.write_text(json.dumps(data))
        self.cli('pkg', 'install', str(shared.PACKAGE))
        self.root = self.home / '.mysh/pkgs'
        self.prior = {str(p.relative_to(self.root / 'hello-0.2.0')): p.read_bytes()
                      for p in (self.root / 'hello-0.2.0').rglob('*') if p.is_file()}
        (self.root / '.tmp-unknown').mkdir(mode=0o700)
        (self.root / '.tmp-unknown/keep').write_bytes(b'human bytes')
        (self.root / '.active/tmp-unknown').write_bytes(b'human selection bytes')

    def cli(self, *args, status=0, data=None):
        result = subprocess.run([self.program, *args], input=data,
                                capture_output=True, env=self.env, timeout=15)
        self.assertEqual(result.returncode, status, result)
        self.assertNotIn(b'live=1', result.stderr)
        self.assertNotIn(b'fd=1', result.stderr)
        return result

    def assert_preserved(self, selection='0.2.0'):
        self.assertEqual((self.root / '.active/hello').read_text(), selection)
        self.assertEqual(self.prior, {
            str(p.relative_to(self.root / 'hello-0.2.0')): p.read_bytes()
            for p in (self.root / 'hello-0.2.0').rglob('*') if p.is_file()})
        self.assertEqual((self.root / '.tmp-unknown/keep').read_bytes(), b'human bytes')
        self.assertEqual((self.root / '.active/tmp-unknown').read_bytes(),
                         b'human selection bytes')
        self.assertEqual(sorted(p.name for p in self.root.glob('.tmp-*')),
                         ['.tmp-unknown'])
        self.assertEqual(sorted(p.name for p in (self.root / '.active').glob('tmp-*')),
                         ['tmp-unknown'])

    def fault_install(self, mode, index=None):
        self.env['AISHELL_NATIVE_FAULT'] = mode
        if index is not None:
            self.env['AISHELL_NATIVE_INDEX'] = str(index)
        result = self.cli('pkg', 'install', str(self.source), status=1)
        self.assertIn(b'native-fault: hit=1', result.stderr)
        self.assertIn(b'live=0 fd=0', result.stderr)
        return result

    def test_pre_activation_io_faults_preserve_previous_selection(self):
        for mode in ('payload-write', 'payload-close', 'payload-rename',
                     'selection-write', 'selection-fsync', 'selection-close',
                     'selection-rename'):
            with self.subTest(mode=mode):
                self.fault_install(mode)
                self.assert_preserved()
                newer = self.root / 'hello-0.3.0'
                if newer.exists():
                    # A failed selection intentionally retains an immutable copy.
                    shutil.rmtree(newer)

    def test_allocation_faults_preserve_previous_selection(self):
        baseline = self.cli('pkg', 'install', str(self.source))
        count = int(re.search(rb'allocations=(\d+)', baseline.stderr)[1])
        self.assertGreater(count, 200)
        self.cli('pkg', 'use', 'hello', '0.2.0')
        shutil.rmtree(self.root / 'hello-0.3.0')
        for index in range(1, count + 1):
            with self.subTest(index=index):
                self.fault_install('allocation', index)
                self.assert_preserved()
                retained = self.root / 'hello-0.3.0'
                if retained.exists():
                    self.assertEqual((retained / 'lib/hello.so').read_bytes(),
                                     (self.source / 'lib/hello.so').read_bytes())
                    shutil.rmtree(retained)
        print('native allocation failure points verified:', count)

    def test_stage_collision_preserves_unknown_directory(self):
        proc = subprocess.Popen([self.program], stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                env=self.env)
        unknown = self.root / ('.tmp-' + str(proc.pid) + '-0')
        unknown.mkdir(mode=0o700)
        (unknown / 'keep').write_bytes(b'not owned by this transaction')
        try:
            out, err = proc.communicate(
                f'pkg install {self.source}\nexit\n'.encode(), timeout=10)
            self.assertEqual(proc.returncode, 1, (out, err))
            self.assertEqual((unknown / 'keep').read_bytes(),
                             b'not owned by this transaction')
            self.assertEqual((self.root / '.active/hello').read_text(), '0.2.0')
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.communicate()

    def test_post_commit_close_reports_committed_selection(self):
        result = self.fault_install('post-close')
        self.assert_preserved('0.3.0')
        self.assertIn(b'selection committed', result.stderr)
        self.assertNotIn(b'Installed hello', result.stdout)

    def test_use_pre_activation_faults_preserve_selection(self):
        self.cli('pkg', 'install', str(self.source))
        self.cli('pkg', 'use', 'hello', '0.2.0')
        for mode in ('selection-write', 'selection-fsync',
                     'selection-close', 'selection-rename'):
            with self.subTest(mode=mode):
                self.env['AISHELL_NATIVE_FAULT'] = mode
                result = self.cli('pkg', 'use', 'hello', '0.3.0', status=1)
                self.assertIn(b'hit=1', result.stderr)
                self.assertIn(b'live=0 fd=0', result.stderr)
                self.assert_preserved()

    def test_use_post_commit_close_reports_committed_selection(self):
        self.cli('pkg', 'install', str(self.source))
        self.cli('pkg', 'use', 'hello', '0.2.0')
        self.env['AISHELL_NATIVE_FAULT'] = 'post-close'
        result = self.cli('pkg', 'use', 'hello', '0.3.0', status=1)
        self.assertIn(b'hit=1', result.stderr)
        self.assert_preserved('0.3.0')
        self.assertIn(b'selection committed', result.stderr)


    def test_remove_unlink_failure_preserves_selection(self):
        self.env['AISHELL_NATIVE_FAULT'] = 'selection-unlink'
        result = self.cli('pkg', 'remove', 'hello', status=1)
        self.assertIn(b'hit=1', result.stderr)
        self.assertIn(b'live=0 fd=0', result.stderr)
        self.assert_preserved()

    def test_remove_post_commit_close_is_honest(self):
        self.env['AISHELL_NATIVE_FAULT'] = 'post-close'
        result = self.cli('pkg', 'remove', 'hello', status=1)
        self.assertIn(b'hit=1', result.stderr)
        self.assertIn(b'live=0 fd=0', result.stderr)
        self.assertIn(b'selection committed', result.stderr)
        self.assertFalse((self.root / '.active/hello').exists())
        self.assertTrue((self.root / 'hello-0.2.0/lib/hello.so').exists())


if __name__ == '__main__':
    unittest.main()
