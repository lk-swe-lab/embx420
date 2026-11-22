"""Check the complete demo from another directory, with its real pauses."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DemoTests(unittest.TestCase):
    def test_real_demo_and_cleanup(self):
        inputs = [ROOT / "build/lean-shell",
                  *sorted((ROOT / "build/modules").glob("*.so"))]
        before = {path: hashlib.sha256(path.read_bytes()).digest()
                  for path in inputs}
        with tempfile.TemporaryDirectory(prefix="lean-demo-test-") as folder:
            result = subprocess.run(
                [str(ROOT / "demo.sh")], capture_output=True, text=True,
                cwd=folder, env=dict(os.environ, TMPDIR=folder, HOME=folder),
                timeout=90,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            for marker in ("Single command", "Two stages", "Three stages",
                           "Parent directory", "Syntax recovery",
                           "Process trace", "PASS: pipe demo"):
                self.assertIn(marker, result.stdout)
            self.assertIn("red\nblue\n", result.stdout)
            self.assertEqual(result.stderr, "")
            self.assertEqual(list(Path(folder).iterdir()), [])
        after = {path: hashlib.sha256(path.read_bytes()).digest()
                 for path in inputs}
        self.assertEqual(after, before)


if __name__ == "__main__":
    unittest.main()
