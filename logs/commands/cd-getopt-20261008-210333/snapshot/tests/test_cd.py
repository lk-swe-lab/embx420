"""cd integration expectations against main, never a standalone cd binary."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument("program")
parser.add_argument("--test", default=None)
options = parser.parse_args()
PROGRAM = os.path.abspath(options.program)


class CdTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(
            prefix="cd-test-", dir=os.environ.get("TMPDIR"))
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        (self.root / "child").mkdir()

    def shell(self, text, env=None):
        return subprocess.run([PROGRAM], input=text.encode(), cwd=self.root,
                              env={**os.environ, "LC_ALL": "C", **(env or {})},
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=5)

    def check(self, result, paths, status=0, error=False):
        expected = b"".join(os.fsencode(path) + b"\n" for path in paths)
        self.assertEqual(result.returncode, status, result.stderr)
        self.assertEqual(result.stdout, expected)
        self.assertEqual(bool(result.stderr), error, result.stderr)

    def test_home_default(self):
        self.check(self.shell("cd\npwd\n", {"HOME": str(self.root / "child")}),
                   [self.root / "child"])
        self.check(self.shell("cd\n", {"HOME": ""}), [], 1, True)
        env = {**os.environ, "LC_ALL": "C"}
        env.pop("HOME", None)
        result = subprocess.run([PROGRAM], input=b"cd\n", cwd=self.root,
                                env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=5)
        self.check(result, [], 1, True)
        self.check(self.shell("cd\npwd\n", {"HOME": ""}), [self.root],
                   error=True)

    def test_home_literal_dash(self):
        (self.root / "-").mkdir()
        self.check(self.shell("cd\npwd\n", {"HOME": "-"}), [self.root / "-"])

    def test_syntax_help_no_effects(self):
        (self.root / "-").mkdir()
        (self.root / "-dash").mkdir()
        (self.root / "with space").mkdir()
        for command, destination in [("cd -- -dash", "-dash"),
                                     ("cd ./-", "-"),
                                     ("cd 'with space'", "with space")]:
            self.check(self.shell(command + "\npwd\n"),
                       [self.root / destination])
        for command in ["cd --bad", "cd -", "cd -- -", "cd a b",
                        "cd -P", "cd --help a b", "cd --help --bad"]:
            with self.subTest(command=command):
                self.check(self.shell(command + "\n"), [], 2, True)
                self.check(self.shell(command + "\npwd\n"), [self.root],
                           error=True)
        baseline = self.shell("cd --help\n")
        self.assertEqual(baseline.returncode, 0)
        self.assertEqual(baseline.stderr, b"")
        self.assertTrue(baseline.stdout.startswith(b"Usage: cd"))
        self.assertIn(b"DIRECTORY", baseline.stdout)
        for command in ["cd -h", "cd --help child", "help cd"]:
            result = self.shell(command + "\npwd\n")
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stderr, b"")
            self.assertEqual(result.stdout,
                             baseline.stdout + os.fsencode(self.root) + b"\n")
        self.check(self.shell("cd ''\n"), [], 1, True)
        self.check(self.shell("cd ''\npwd\n"), [self.root], error=True)

    def test_physical_and_failure_preservation(self):
        (self.root / "real").mkdir()
        (self.root / "real" / "nested").mkdir()
        (self.root / "alias").symlink_to("real/nested")
        self.check(self.shell("cd alias\npwd\ncd ..\npwd\n"),
                   [self.root / "real" / "nested", self.root / "real"])
        self.check(self.shell("cd alias/..\npwd\n"), [self.root / "real"])
        (self.root / "file").write_bytes(b"data")
        (self.root / "loop").symlink_to("loop")
        for path in ["absent", "file", "file/child", "loop"]:
            self.check(self.shell(f"cd {path}\n"), [], 1, True)
            self.check(self.shell(f"cd {path}\npwd\n"), [self.root],
                       error=True)
        self.check(self.shell("cd child\npwd\n",
                              {"PWD": "/stale", "OLDPWD": "/old",
                               "CDPATH": "/not-used"}),
                   [self.root / "child"])

    def test_permission_and_path_length_errors(self):
        blocked = self.root / "blocked"
        blocked.mkdir()
        blocked.chmod(0)
        self.addCleanup(blocked.chmod, 0o700)
        if os.geteuid() != 0:
            self.check(self.shell("cd blocked\n"), [], 1, True)
            self.check(self.shell("cd blocked\npwd\n"), [self.root], error=True)
        self.check(self.shell("cd " + "x" * 5000 + "\npwd\n"), [self.root],
                   error=True)
        self.check(self.shell("cd child\npwd\ncd ..\npwd\ncd absent\npwd\n"),
                   [self.root / "child", self.root, self.root], error=True)

    def test_registry_discovery_and_last_status(self):
        result = subprocess.run([PROGRAM, "--help"], cwd=self.root,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=5)
        self.assertEqual(result.returncode, 0)
        self.assertIn(b"  cd ", result.stdout)
        self.assertEqual(result.stderr, b"")
        self.check(self.shell("cd absent\nexit\n"), [], 1, True)
        self.check(self.shell("cd --bad\nexit\n"), [], 2, True)
        self.check(self.shell("cd child\nexit\n"), [])

    def test_absolute_relative_persistence(self):
        child = self.root / "child"
        self.check(self.shell(f"cd {shlex.quote(str(child))}\npwd\n"
                              "cd ..\npwd\n"), [child, self.root])


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(CdTests)
    if options.test:
        suite = unittest.TestSuite([CdTests(options.test)])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(not result.wasSuccessful())
