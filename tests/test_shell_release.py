"""Release artifact contract tests; run after make build/main package-hello."""

import importlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
BUILDER = ROOT / "scripts/build_shell_release.py"
sys.path.insert(0, str(ROOT / "scripts"))
builder = importlib.import_module("build_shell_release")


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.output = self.base / "out"

    def build_args(self):
        return SimpleNamespace(
            output_root=self.output,
            manifest=ROOT / "packages/aishell/release.json",
            demo_package=ROOT / "build/packages/hello-0.1.0",
            binary=ROOT / "build/main",
        )

    def build(self, *args, status=0):
        result = subprocess.run(
            [sys.executable, str(BUILDER), "--output-root", str(self.output), *args],
            capture_output=True,
            timeout=30,
            check=False,
        )
        self.assertEqual(result.returncode, status, result)
        return result

    def test_complete_relocatable_release(self):
        self.build()
        release = self.output / "aishell-0.1.0"
        identity = json.loads((ROOT / "packages/aishell/release.json").read_text())
        self.assertEqual(json.loads((release / "release.json").read_text()), identity)
        catalog = json.loads((release / "metadata/commands.json").read_text())
        expected = {
            "release.json",
            "bin/aishell",
            "README.md",
            "SYSTEM.md",
            "metadata/commands.json",
        }
        expected.update(
            "docs/" + e["name"] + ".md"
            for e in catalog["commands"]
            if e["origin"] == "builtin"
        )
        packaged_demo = next((release / "examples").iterdir())
        demo_record = json.loads((packaged_demo / "pkg.json").read_text())
        demo_prefix = packaged_demo.relative_to(release).as_posix() + "/"
        expected.update(demo_prefix + p for p in ["pkg.json", *demo_record["files"]])
        self.assertEqual(
            {
                p.relative_to(release).as_posix()
                for p in release.rglob("*")
                if p.is_file()
            },
            expected,
        )
        self.assertEqual(
            (release / "bin/aishell").read_bytes(), (ROOT / "build/main").read_bytes()
        )
        for relative in expected:
            path = release / relative
            self.assertFalse(path.is_symlink())
            self.assertEqual(
                path.stat().st_mode & 0o777,
                0o755 if "/bin/" in "/" + relative else 0o644,
            )
            if "/bin/" not in "/" + relative:
                self.assertNotIn(str(ROOT).encode(), path.read_bytes())
        home = self.base / "home"
        home.mkdir(mode=0o700)
        poison = self.base / "poison"
        poison.mkdir()
        for name in ("pwd", "hello", "aishell"):
            fake = poison / name
            fake.write_text("#!/bin/sh\nprintf POISON\nexit 99\n")
            fake.chmod(0o755)
        installed = self.base / "installed"
        shutil.copytree(release, installed)
        shutil.rmtree(self.output)
        env = {"HOME": str(home), "PATH": str(poison), "LC_ALL": "C"}
        binary = installed / "bin/aishell"

        def cli(*args, data=None, status=0):
            result = subprocess.run(
                [str(binary), *args],
                input=data,
                env=env,
                cwd=home,
                capture_output=True,
                timeout=10,
                check=False,
            )
            self.assertEqual(result.returncode, status, result)
            if status == 0:
                self.assertEqual(result.stderr, b"")
            self.assertNotIn(b"POISON", result.stdout)
            return result.stdout

        self.assertEqual(json.loads(cli("--commands-json")), catalog)
        for entry in catalog["commands"]:
            if entry["origin"] == "builtin":
                page = installed / ("docs/" + entry["name"] + ".md")
                self.assertIn(cli("help", entry["name"]).decode(), page.read_text())
                self.assertIn(entry["summary"], page.read_text())
        self.assertEqual(cli("pwd"), os.fsencode(home) + b"\n")
        demo = next((installed / "examples").iterdir())
        manifest = json.loads((demo / "pkg.json").read_text())
        cli("pkg", "install", str(demo))
        self.assertIn(manifest["version"].encode(), cli("pkg", "info", "hello"))
        self.assertEqual(cli("hello", "--name", "release"), b"Hello, release!\n")
        cli("pkg", "remove", "hello")
        cli("hello", status=127)
        # A synthetic next-version record exercises remove/install updates;
        # executable bytes remain the real bundled hello, not a fake runtime.
        updated = self.base / "hello-update"
        shutil.copytree(demo, updated)
        manifest["version"] = "0.1.1"
        (updated / "pkg.json").write_text(json.dumps(manifest))
        metadata = json.loads((updated / "metadata/commands.json").read_text())
        metadata["version"] = manifest["version"]
        (updated / "metadata/commands.json").write_text(json.dumps(metadata))
        cli("pkg", "install", str(updated))
        self.assertEqual(cli("pkg", "list"), b"hello 0.1.1\n")
        # Manual shell update: copy a new tree, select its binary.
        newer = self.base / "next-release"
        shutil.copytree(installed, newer)
        binary = newer / "bin/aishell"
        shutil.rmtree(installed)
        self.assertEqual(cli("pkg", "list"), b"hello 0.1.1\n")
        self.assertEqual(cli("hello"), b"Hello, world!\n")
        cli("pkg", "remove", "hello")
        self.assertEqual(cli("pkg", "list"), b"")
        self.assertEqual(cli(data=b"pwd\nexit\n"), os.fsencode(home) + b"\n")
        requirements = (newer / "SYSTEM.md").read_text()
        self.assertIn("GLIBC_", requirements)
        self.assertNotIn(str(ROOT), requirements)
        self.assertNotIn(str(ROOT), (newer / "README.md").read_text())

    def test_immutable_exact_repeat_and_unlisted_preservation(self):
        self.build()
        release = self.output / "aishell-0.1.0"
        inode = release.stat().st_ino
        self.build()
        self.assertEqual(release.stat().st_ino, inode)
        note = release / "human-note"
        note.write_text("keep")
        self.build(status=1)
        self.assertEqual(note.read_text(), "keep")
        self.assertEqual(release.stat().st_ino, inode)
        self.assertEqual(sorted(p.name for p in self.output.iterdir()), [release.name])

    def test_missing_binary_no_partial_publication(self):
        self.build("--binary", str(self.base / "missing"), status=1)
        self.assertFalse(self.output.exists())

    def test_mismatched_generator_is_rejected_before_publication(self):
        fake = self.base / "generator"
        fake.write_text(
            "#!" + sys.executable + "\nimport json, sys\n"
            'if sys.argv[1] == "--commands-json":\n'
            " print(json.dumps({'schema_version': 1, 'commands': ["
            "{'name': 'pwd', 'origin': 'builtin', 'summary': 'FAKE'}]}))\n"
            "else:\n print('FAKE HELP')\n"
        )
        fake.chmod(0o755)
        result = self.build("--generator", str(fake), status=2)
        self.assertIn(b"unrecognized arguments: --generator", result.stderr)
        self.assertFalse(self.output.exists())

    def test_generator_failure_no_partial_publication(self):
        original = builder.run

        def fail_catalog(program, *args, **kwargs):
            if args == ("--commands-json",):
                raise subprocess.CalledProcessError(1, str(program))
            return original(program, *args, **kwargs)

        with (
            patch.object(builder, "run", fail_catalog),
            self.assertRaises(subprocess.CalledProcessError),
        ):
            builder.build(self.build_args())
        self.assertFalse(self.output.exists())

    def test_empty_generated_help_is_not_published(self):
        original = builder.run

        def empty_help(program, *args, **kwargs):
            if args[0] == "help":
                return b""
            return original(program, *args, **kwargs)

        with (
            patch.object(builder, "run", empty_help),
            self.assertRaisesRegex(ValueError, "empty builtin help"),
        ):
            builder.build(self.build_args())
        self.assertFalse(self.output.exists())

    def test_existing_output_types_bytes_and_modes_preserved(self):
        for kind in ("bytes", "mode", "symlink", "hardlink", "fifo", "directory"):
            with self.subTest(kind=kind):
                self.output = self.base / kind
                self.build()
                release = self.output / "aishell-0.1.0"
                target = release / "docs/pkg.md"
                if kind == "bytes":
                    target.write_text("human change")
                elif kind == "mode":
                    target.chmod(0o600)
                elif kind == "symlink":
                    target.unlink()
                    target.symlink_to(self.base / "outside")
                elif kind == "hardlink":
                    os.link(target, self.base / "outside-hardlink")
                elif kind == "fifo":
                    target.unlink()
                    os.mkfifo(target)
                else:
                    (release / "unlisted-empty").mkdir()
                before = target.lstat()
                self.build(status=1)
                after = target.lstat()
                self.assertEqual(
                    (before.st_ino, before.st_mode, before.st_size),
                    (after.st_ino, after.st_mode, after.st_size),
                )
                if kind == "bytes":
                    self.assertEqual(target.read_text(), "human change")

    def test_release_identity_drives_directory_and_guide(self):
        source = self.base / "source"
        source.mkdir()
        record = json.loads((ROOT / "packages/aishell/release.json").read_text())
        record["version"] = "0.2.0"
        manifest = source / "release.json"
        manifest.write_text(json.dumps(record))
        self.build("--manifest", str(manifest))
        release = self.output / "aishell-0.2.0"
        self.assertEqual((release / "release.json").read_bytes(), manifest.read_bytes())
        self.assertIn("aishell 0.2.0", (release / "README.md").read_text())
        self.assertFalse((self.output / "aishell-0.1.0").exists())

    def test_private_write_and_publication_failures_clean_stage(self):
        from unittest.mock import Mock

        args = self.build_args()
        original = Path.write_bytes

        def fail_binary(path, data):
            if path.name == "aishell" and path.parent.name == "bin":
                raise OSError("injected staging write failure")
            return original(path, data)

        with (
            patch.object(Path, "write_bytes", fail_binary),
            self.assertRaisesRegex(OSError, "staging write"),
        ):
            builder.build(args)
        self.assertEqual(list(self.output.iterdir()), [])
        fake_libc = Mock()
        fake_libc.renameat2.return_value = -1
        with (
            patch.object(builder.ctypes, "CDLL", return_value=fake_libc),
            self.assertRaisesRegex(OSError, "publication failed"),
        ):
            builder.build(args)
        self.assertEqual(list(self.output.iterdir()), [])

    def test_changed_binary_cannot_replace_same_version(self):
        self.build()
        released = self.output / "aishell-0.1.0/bin/aishell"
        original = released.read_bytes()
        changed = self.base / "changed-shell"
        changed.write_bytes(original + b"release-test-trailer")
        changed.chmod(0o755)
        self.build("--binary", str(changed), status=1)
        self.assertEqual(released.read_bytes(), original)

    def test_source_contained_in_output_is_rejected_before_generation(self):
        source = self.base / "source"
        source.mkdir()
        manifest = source / "release.json"
        shutil.copyfile(ROOT / "packages/aishell/release.json", manifest)
        self.output = self.base
        result = self.build("--manifest", str(manifest), status=1)
        self.assertIn(b"ancestry overlap", result.stderr)
        self.assertEqual(sorted(p.name for p in self.base.iterdir()), ["source"])

    def test_source_overlap_before_generator(self):
        self.output = ROOT / "packages/aishell/forbidden"
        self.build(status=1)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
