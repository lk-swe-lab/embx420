"""Stat contract checks with explicit GNU format adapter in reference mode."""
import argparse
import json
import os
from pathlib import Path
import socket
import signal
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument("program")
parser.add_argument("--reference", action="store_true")
options = parser.parse_args()
PROGRAM = os.path.abspath(options.program)
FORMAT = ("File: %n\nType: %F\nSize: %s bytes\nMode: %04a\n"
          "Access: %X\nModify: %Y\nChange: %Z\n")


class StatTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="stat-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def run_stat(self, args):
        adapter = ["--printf=" + FORMAT] if options.reference else []
        return subprocess.run([PROGRAM, *adapter, *args], cwd=self.root,
                              env={**os.environ, "LC_ALL": "C"},
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=5)

    def expected(self, path, kind, follow=False):
        metadata = os.stat(self.root / path, follow_symlinks=follow)
        return {"path": path, "type": kind, "size": metadata.st_size,
                "mode": f"{metadata.st_mode & 0o7777:04o}",
                "mtime": metadata.st_mtime_ns // 1000000000,
                "atime": metadata.st_atime_ns // 1000000000,
                "ctime": metadata.st_ctime_ns // 1000000000}

    def human(self, fields):
        return ("File: {path}\nType: {type}\nSize: {size} bytes\n"
                "Mode: {mode}\nAccess: {atime}\nModify: {mtime}\n"
                "Change: {ctime}\n").format(**fields).encode(
                    "utf-8", "surrogateescape")

    def check(self, args, expected, status=0):
        result = self.run_stat(args)
        print(f"expected={expected[:140]!r}, got={result.stdout[:140]!r}")
        self.assertEqual(result.returncode, status, result.stderr)
        self.assertEqual(result.stdout, expected)
        self.assertEqual(bool(result.stderr), status != 0)

    def test_file_metadata_without_reading(self):
        for name, contents in (("file", b"a\x00b\xff"), ("empty", b""),
                               ('quote"\\\n\té', b"data"), ("-", b"dash"),
                               ("-L", b"option")):
            if options.reference and name == "-":
                continue  # GNU uses stdin; project deliberately uses a filename.
            with self.subTest(name=name):
                path = self.root / name
                path.write_bytes(contents)
                path.chmod(0o640)
                os.utime(path, (1000000000, 1100000000))
                kind = "regular file" if contents else "regular empty file"
                before = path.stat()
                self.check(["--", name], self.human(self.expected(name, kind)))
                after = path.stat()
                self.assertEqual(after.st_atime_ns, before.st_atime_ns)
                self.assertEqual(after.st_mtime_ns, before.st_mtime_ns)
                self.assertEqual(path.read_bytes(), contents)

    def test_types_and_dereference(self):
        (self.root / "dir").mkdir()
        os.mkfifo(self.root / "fifo")
        sock = socket.socket(socket.AF_UNIX)
        self.addCleanup(sock.close)
        sock.bind(str(self.root / "socket"))
        (self.root / "link").symlink_to("dir")
        (self.root / "dangling").symlink_to("missing")
        for name, kind in (("dir", "directory"), ("fifo", "fifo"),
                           ("socket", "socket"), ("link", "symbolic link"),
                           ("dangling", "symbolic link")):
            self.check([name], self.human(self.expected(name, kind)))
        for flag in ("-L", "--dereference"):
            self.check([flag, "link"], self.human(
                self.expected("link", "directory", follow=True)))
            self.check([flag, "dangling"], b"", 1)
        self.check(["missing"], b"", 1)

    @unittest.skipIf(options.reference, "project argument and help contract")
    def test_arguments(self):
        for args in ([], ["a", "b"], ["--unknown"], ["--version"],
                     ["--help", "--unknown"], ["--help", "a", "b"]):
            self.check(args, b"", 2)
        for args in (["--help"], ["-h", "missing"]):
            result = self.run_stat(args)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stderr, b"")
            for word in (b"Usage:", b"--dereference", b"--json", b"FILE"):
                self.assertIn(word, result.stdout)

    @unittest.skipIf(options.reference, "project JSON contract")
    def test_json(self):
        name = 'quote"\\\n\t\x01é'
        path = self.root / name
        path.write_bytes(b"hello")
        path.chmod(0o4751)
        os.utime(path, (-1, 1234567890))
        expected = self.expected(name, "regular file")
        result = self.run_stat(["--json", name])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, b"")
        self.assertEqual(json.loads(result.stdout), expected)
        self.assertTrue(result.stdout.endswith(b"\n"))

    @unittest.skipIf(options.reference, "project encoding contract")
    def test_invalid_utf8(self):
        name = os.fsdecode(b"bad-\xff")
        (self.root / name).touch()
        self.check([name], self.human(self.expected(name, "regular empty file")))
        self.check(["--json", name], b"", 1)

    def test_metadata_boundaries(self):
        path = self.root / "sparse"
        with path.open("wb") as stream:
            stream.truncate((1 << 33) + 7)
        path.chmod(0o1754)
        os.utime(path, (-2, -1))
        self.check(["sparse"], self.human(
            self.expected("sparse", "regular file")))
        self.check(["/dev/null"], self.human(
            self.expected("/dev/null", "character special file")))

    def test_lookup_failures(self):
        (self.root / "file").touch()
        (self.root / "loop").symlink_to("loop")
        for path in ("", "file/child", "missing/child"):
            self.check(["--", path], b"", 1)
        self.check(["-L", "loop"], b"", 1)
        if not options.reference:
            for path in ("", "file/child", "missing/child"):
                self.check(["--json", "--", path], b"", 1)

    @unittest.skipIf(options.reference, "project JSON contract")
    def test_json_types_and_generated_paths(self):
        (self.root / "dir").mkdir()
        os.mkfifo(self.root / "fifo")
        sock = socket.socket(socket.AF_UNIX)
        self.addCleanup(sock.close)
        sock.bind(str(self.root / "socket"))
        (self.root / "link").symlink_to("dir")
        cases = [("dir", "directory"), ("fifo", "fifo"),
                 ("socket", "socket"), ("link", "symbolic link"),
                 ("/dev/null", "character special file")]
        for number in range(24):
            name = f"case-{number}-" + chr(number + 1) + "é😀"
            path = self.root / name
            path.write_bytes(bytes(range(number)))
            path.chmod((number * 73) & 0o777)
            os.utime(path, (-number - 1, number * 123456))
            cases.append((name, "regular file" if number else
                          "regular empty file"))
        for name, kind in cases:
            with self.subTest(name=name):
                result = self.run_stat(["--json", "--", name])
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stderr, b"")
                self.assertEqual(json.loads(result.stdout),
                                 self.expected(name, kind))
        result = self.run_stat(["--json", "-L", "link"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, b"")
        self.assertEqual(json.loads(result.stdout),
                         self.expected("link", "directory", follow=True))

    @unittest.skipIf(options.reference, "project output errors")
    def test_broken_pipe_inherits_signal_policy(self):
        (self.root / "file").touch()
        self.assertEqual(signal.getsignal(signal.SIGPIPE), signal.SIG_IGN)
        for restore in (True, False):
            for args in (["file"], ["--json", "file"], ["--help"]):
                reader, writer = os.pipe()
                os.close(reader)
                try:
                    result = subprocess.run(
                        [PROGRAM, *args], cwd=self.root, stdout=writer,
                        stderr=subprocess.PIPE, timeout=5,
                        restore_signals=restore)
                finally:
                    os.close(writer)
                self.assertEqual(result.returncode,
                                 -signal.SIGPIPE if restore else 1)
                self.assertEqual(bool(result.stderr), not restore)

    @unittest.skipIf(options.reference, "project output errors")
    def test_output_failure(self):
        (self.root / "file").touch()
        with open("/dev/full", "wb") as output:
            for args in (["file"], ["--json", "file"], ["--help"]):
                result = subprocess.run([PROGRAM, *args], cwd=self.root,
                                        stdout=output, stderr=subprocess.PIPE,
                                        timeout=5)
                self.assertEqual(result.returncode, 1)
                self.assertTrue(result.stderr)


if __name__ == "__main__":
    unittest.main(argv=[__file__], verbosity=2)
