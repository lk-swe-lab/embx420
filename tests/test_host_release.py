"""Check the separate, package-free host artifact."""
import importlib.util
import os
from pathlib import Path
import shutil
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location(
    "host_release", ROOT / "scripts/build_host_release.py")
assert SPEC is not None and SPEC.loader is not None
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


class HostRelease(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="host-release-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.args = SimpleNamespace(binary=ROOT / "build/aishell",
                                    output_root=self.base / "out",
                                    version="0.3.0")

    def test_host_has_no_bundled_packages(self):
        target = builder.build(self.args)
        self.assertTrue((target / "bin/aishell").is_file())
        self.assertEqual((target / "bin/aishell").read_bytes(),
                         self.args.binary.read_bytes())
        self.assertEqual(sorted(str(p.relative_to(target))
                                for p in target.rglob("*") if p.is_file()),
                         ["README.md", "bin/aishell", "release.json"])

    def test_identical_rebuild_and_changed_output(self):
        target = builder.build(self.args)
        self.assertEqual(builder.build(self.args), target)
        (target / "README.md").write_text("Keep these bytes.")
        with self.assertRaises(ValueError):
            builder.build(self.args)
        self.assertEqual((target / "README.md").read_text(), "Keep these bytes.")

    def test_empty_destination_is_not_replaced(self):
        target = self.args.output_root / "aishell-0.3.0"
        target.mkdir(parents=True)
        with self.assertRaises(ValueError):
            builder.build(self.args)
        self.assertEqual(list(target.iterdir()), [])

    def test_linked_input_is_rejected(self):
        link = self.base / "host"
        link.symlink_to(self.args.binary)
        self.args.binary = link
        with self.assertRaises(ValueError):
            builder.build(self.args)

    def test_hardlink_and_fifo_are_rejected(self):
        source = self.base / "copy"
        shutil.copy2(self.args.binary, source)
        os.link(source, self.base / "other-link")
        fifo = self.base / "fifo"
        os.mkfifo(fifo)
        for path in (source, fifo):
            with self.subTest(path=path.name):
                self.args.binary = path
                with self.assertRaises(ValueError):
                    builder.build(self.args)

    def test_bad_versions_and_output_ancestor(self):
        for version in ("../escape", "01.0.0", "65536.0.0", "0.3"):
            with self.subTest(version=version):
                self.args.version = version
                with self.assertRaises(ValueError):
                    builder.build(self.args)
        self.args.version = "0.3.0"
        real = self.base / "real"
        real.mkdir()
        link = self.base / "link"
        link.symlink_to(real, target_is_directory=True)
        self.args.output_root = link / "out"
        with self.assertRaises(ValueError):
            builder.build(self.args)

    def test_publication_failure_leaves_no_release(self):
        with patch.object(builder, "publish_directory",
                          side_effect=OSError("fault")):
            with self.assertRaises(OSError):
                builder.build(self.args)
        self.assertEqual(list(self.args.output_root.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
