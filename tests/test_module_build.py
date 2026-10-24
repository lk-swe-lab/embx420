"""Source-derived inventory and incremental native module regressions."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import build_modules


class ModuleBuild(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)

    def test_rebuild_rejects_unknown_output_without_changes(self):
        output = self.base / 'modules'
        shutil.copytree(ROOT / 'build/modules', output)
        (output / 'ghost.so').write_bytes(b'unknown user bytes')
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in output.iterdir()}
        result = subprocess.run([
            sys.executable, str(ROOT / 'scripts/build_modules.py'),
            '--output', str(output)], cwd=ROOT, capture_output=True)
        self.assertNotEqual(result.returncode, 0, 'stale ghost survived rebuild')
        self.assertIn(b'unexpected module output', result.stderr)
        self.assertEqual(before, {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in output.iterdir()})

    def test_hello_dependency_closure(self):
        shutil.copytree(ROOT / 'src', self.base / 'src')
        shutil.copytree(ROOT / 'scripts', self.base / 'scripts')
        shutil.copy2(ROOT / 'Makefile', self.base / 'Makefile')
        target = self.base / 'build/example-modules/hello.so'
        target.parent.mkdir(parents=True)
        target.write_bytes(b'existing output')
        for header in ('src/cmd_spec.h', 'src/module_abi.h',
                       'src/cmd_lines.h', 'src/cmd_hello.h'):
            with self.subTest(header=header):
                result = subprocess.run([
                    'make', '-n', '-W', header,
                    'build/example-modules/hello.so'], cwd=self.base,
                    capture_output=True, text=True, check=True)
                self.assertIn('scripts/build_modules.py --hello', result.stdout)

    def test_release_rejects_unknown_module(self):
        host = self.base / 'host'
        host.mkdir()
        shutil.copy2(ROOT / 'build/aishell', host / 'aishell')
        shutil.copytree(ROOT / 'build/modules', host / 'modules')
        shutil.copy2(ROOT / 'build/example-modules/hello.so',
                     host / 'modules/hello.so')
        result = subprocess.run([
            sys.executable, str(ROOT / 'scripts/build_shell_release.py'),
            '--binary', str(host / 'aishell'), '--output-root',
            str(self.base / 'release')], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'standard module inventory', result.stderr)

    def test_release_rejects_missing_standard_modules(self):
        for name in ('pkg', 'cd', 'exit', 'cat'):
            with self.subTest(name=name):
                host = self.base / name
                host.mkdir()
                shutil.copy2(ROOT / 'build/aishell', host / 'aishell')
                shutil.copytree(ROOT / 'build/modules', host / 'modules')
                (host / 'modules' / (name + '.so')).unlink()
                result = subprocess.run([
                    sys.executable, str(ROOT / 'scripts/build_shell_release.py'),
                    '--binary', str(host / 'aishell'), '--output-root',
                    str(self.base / ('release-' + name))], capture_output=True)
                self.assertNotEqual(result.returncode, 0, name + ' accepted')
                self.assertIn(b'standard module inventory', result.stderr)
                self.assertFalse((self.base / ('release-' + name)).exists())


if __name__ == '__main__':
    unittest.main()
