"""Shared-module acceptance tests; run against the real host."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROGRAM = Path(os.environ.get('AISHELL_TEST_PROGRAM', ROOT / 'build/aishell'))


class Modules(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.home = Path(self.tmp.name)
        self.env = {'HOME': str(self.home), 'PATH': '/nonexistent', 'LC_ALL': 'C'}

    def run_shell(self, *args, data=None, program=None):
        return subprocess.run([str(program or PROGRAM), *args], input=data,
                              capture_output=True, env=self.env,
                              cwd=self.home, timeout=10)

    def test_host_contains_no_commands(self):
        symbols = subprocess.check_output(['nm', str(PROGRAM)], text=True)
        for forbidden in ('cmd_cat_spec', 'cmd_pkg_spec', 'legacy_main',
                          'print_histogram', 'pkg_store_install'):
            self.assertNotIn(forbidden, symbols)
        dynamic = subprocess.check_output(['nm', '-D', str(PROGRAM)], text=True)
        self.assertNotIn('register_command', dynamic)
        self.assertNotIn('pthread', dynamic)

    def test_process_state_and_catalog(self):
        result = self.run_shell(data=b'cd /\npwd\nmissing\nexit\npwd\n')
        self.assertEqual(result.returncode, 127)
        self.assertEqual(result.stdout, b'/\n')
        catalog = json.loads(self.run_shell('--commands-json').stdout)
        names = [c['name'] for c in catalog['commands']]
        self.assertEqual(names, sorted(set(names)))
        self.assertTrue({'help', 'exit', 'cd', 'pkg', 'traverse'} <= set(names))
        self.assertEqual(len(names), 22)
        self.assertEqual(self.run_shell('help', 'cd').stdout,
                         self.run_shell('cd', '--help').stdout)

    def test_relocatable_no_pkg_recovery(self):
        release = self.home / 'relocated'
        release.mkdir()
        shutil.copy2(PROGRAM, release / 'aishell')
        shutil.copytree(PROGRAM.parent / 'modules', release / 'modules')
        (release / 'modules/pkg.so').unlink()
        result = self.run_shell('pwd', program=release / 'aishell')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, os.fsencode(self.home) + b'\n')
        self.assertIn(b'pkg.so', result.stderr)
        self.assertIn(b'restore', result.stderr)


if __name__ == '__main__':
    unittest.main()
