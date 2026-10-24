"""Integrated package runtime; schema-1 exec/fork assertions are superseded.

Native callbacks now run in the parent. Keep borrowed-state and descriptor
ownership assertions in the C harness; version/trust/path checks live in the
shared integration suite, not in a parallel executable-package runtime.
"""
import subprocess
import sys
import unittest
from test_shared_package import SharedPackages, PACKAGE, ROOT


class PackageShell(SharedPackages):
    def test_parent_callback_ownership(self):
        self.cli('pkg', 'install', str(PACKAGE))
        result = subprocess.run([ROOT / 'build/test_package_shell_module'],
                                env=self.env, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result)


if __name__ == '__main__':
    if len(sys.argv) > 1 and not sys.argv[1].startswith('-'):
        sys.argv.pop(1)
    unittest.main()