"""Independent pwd expectations; reference mode excludes project additions."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument("program")
parser.add_argument("--reference", action="store_true")
options = parser.parse_args()
PROGRAM = os.path.abspath(options.program)


class PwdTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pwd-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()

    def run_pwd(self, args=(), cwd=None, **kwargs):
        return subprocess.run([PROGRAM, *args], cwd=cwd or self.root,
                              env={**os.environ, "LC_ALL": "C", "PWD": "/wrong"},
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=5, **kwargs)

    def check(self, result, expected, status=0):
        print(f"expected={expected[:120]!r}, got={result.stdout[:120]!r}")
        self.assertEqual(result.stdout, expected)
        self.assertEqual(result.returncode, status, result.stderr)
        if status == 0:
            self.assertEqual(result.stderr, b"")
        else:
            self.assertTrue(result.stderr)

    def test_default_and_physical(self):
        for args in ([], ["-P"], ["--physical"], ["--"]):
            with self.subTest(args=args):
                self.check(self.run_pwd(args), os.fsencode(self.root) + b"\n")

    def test_symlink_ignores_pwd(self):
        target = self.root / "actual"
        target.mkdir()
        link = self.root / "alias"
        link.symlink_to(target)
        self.check(self.run_pwd(cwd=link), os.fsencode(target) + b"\n")

    def test_large_path(self):
        target = self.root
        for i in range(12):
            target /= str(i) + "x" * 80
            target.mkdir()
        self.check(self.run_pwd(cwd=target), os.fsencode(target) + b"\n")

    def test_unusual_bytes(self):
        target = os.fsencode(self.root) + b'/quote"\\\n\t-\xff'
        os.mkdir(target)
        self.check(self.run_pwd(cwd=target), target + b"\n")

    def test_removed_cwd(self):
        target = self.root / "removed"
        target.mkdir()
        self.check(self.run_pwd(cwd=target, preexec_fn=lambda: os.rmdir(target)),
                   b"", 1)

    @unittest.skipIf(options.reference, "project help and argument contract")
    def test_help_and_arguments(self):
        for flag in ("-h", "--help"):
            result = self.run_pwd([flag])
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stderr, b"")
            for word in (b"Usage:", b"--physical", b"--json", b"--help"):
                self.assertIn(word, result.stdout)
        for args in (["extra"], ["--", "extra"], ["-L"], ["--version"],
                     ["--unknown"], ["--help", "extra"]):
            self.check(self.run_pwd(args), b"", 2)

    @unittest.skipIf(options.reference, "project JSON contract")
    def test_json(self):
        target = self.root / 'a"\\\n\t\u00e9'
        target.mkdir()
        expected = json.dumps({"cwd": str(target)}, ensure_ascii=False,
                              separators=(",", ":")).encode() + b"\n"
        self.check(self.run_pwd(["--json", "-P"], cwd=target), expected)

    @unittest.skipIf(options.reference, "project UTF-8 contract")
    def test_json_invalid_utf8(self):
        target = os.fsencode(self.root) + b"/bad-\xff"
        os.mkdir(target)
        self.check(self.run_pwd(["--json"], cwd=target), b"", 1)

    @unittest.skipIf(options.reference, "project output error contract")
    def test_output_failure(self):
        with open("/dev/full", "wb") as output:
            for args in ([], ["--json"], ["--help"]):
                result = subprocess.run([PROGRAM, *args], cwd=self.root,
                                        stdout=output, stderr=subprocess.PIPE,
                                        timeout=5)
                self.assertEqual(result.returncode, 1)
                self.assertTrue(result.stderr)


if __name__ == "__main__":
    unittest.main(argv=[__file__], verbosity=2)
