"""Check real empty-host and catalog demonstration output."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CatalogDemo(unittest.TestCase):
    def test_complete_demo(self):
        with tempfile.TemporaryDirectory(prefix="catalog-demo-test-") as folder:
            base = Path(folder)
            output = base / "capture"
            output.mkdir()
            result = subprocess.run(
                ["python3", str(ROOT / "demo/catalog-demo.py"),
                 "--output-dir", str(output), "--catalog",
                 os.environ.get("AISHELL_TEST_CATALOG",
                                str(ROOT / "build/catalog-0.3.1/catalog.json"))],
                cwd=base,
                env=dict(os.environ, TMPDIR=folder), capture_output=True,
                text=True, timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((output / "catalog-demo.json").exists())
            record = json.loads((output / "catalog-demo.json").read_text())
            self.assertEqual(record["status"], "PASS")
            self.assertTrue(record["cleanup_verified"])
            labels = {entry["label"] for entry in record["events"]}
            self.assertTrue({"available", "empty", "selected", "live",
                             "installed-not-loaded", "host-only"} <= labels)
            self.assertEqual(sorted(p.name for p in base.iterdir()), ["capture"])


if __name__ == "__main__":
    unittest.main()
