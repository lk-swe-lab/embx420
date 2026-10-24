"""Independent ls fixtures; GNU adapter only for compatible raw listings."""
import argparse
import json
import os
from pathlib import Path
import signal
import socket
import stat
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument("program")
parser.add_argument("--reference", action="store_true")
parser.add_argument("tests", nargs="*")
options = parser.parse_args()
PROGRAM = os.path.abspath(options.program)


class LsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(
            prefix="ls-test-", dir=os.environ.get("TMPDIR"))
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def run_ls(self, args):
        adapter = ["-1", "--quoting-style=literal", "--color=never"] if options.reference else []
        return subprocess.run([PROGRAM, *adapter, *args], cwd=self.root,
                              env={**os.environ, "LC_ALL": "C"},
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=5)

    def check(self, args, expected, status=0):
        result = self.run_ls(args)
        print(f"args={repr(args)[:160]} expected={expected[:160]!r} "
              f"got={result.stdout[:160]!r}")
        self.assertEqual(result.returncode, status, result.stderr)
        self.assertEqual(result.stdout, expected)
        self.assertEqual(bool(result.stderr), status != 0)

    def test_names(self):
        for name in ("z", "A", "file10", "file2", ".hidden", "é"):
            (self.root / name).write_bytes(b"unchanged\x00\xff")
        expected = b"A\nfile10\nfile2\nz\n\xc3\xa9\n"
        self.check([], expected)
        self.check(["."], expected)
        (self.root / "empty").mkdir()
        self.check(["empty"], b"")
        self.check(["./A"], b"./A\n")
        for name in ("z", "A", "file10", "file2", ".hidden", "é"):
            self.assertEqual((self.root / name).read_bytes(), b"unchanged\x00\xff")

    def test_hidden_and_symlinks(self):
        (self.root / ".hidden").touch()
        (self.root / "visible").touch()
        (self.root / "dir").mkdir()
        (self.root / "dir" / "child").touch()
        (self.root / "link").symlink_to("dir")
        (self.root / "dangling").symlink_to("missing")
        expected = b".\n..\n.hidden\ndangling\ndir\nlink\nvisible\n"
        self.check(["-a"], expected)
        self.check(["--all", "-1"], expected)
        self.check(["dangling"], b"dangling\n")
        if not options.reference:
            self.check(["link"], b"link\n")
        self.check(["link/"], b"child\n")

    @unittest.skipIf(options.reference, "project parser contract")
    def test_arguments_and_lookup(self):
        for args in (["--bad"], ["a", "b"], ["--help", "--bad"],
                     ["--help", "a", "b"], ["-L"], ["--version"]):
            self.check(args, b"", 2)
        for args in (["-h"], ["--help", "missing"]):
            result = self.run_ls(args)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stderr, b"")
            for word in (b"Usage: ls", b"--help", b"FILE"):
                self.assertIn(word, result.stdout)
        (self.root / "file").touch()
        for path in ("missing", "", "file/child", "missing/child"):
            self.check(["--", path], b"", 1)
        (self.root / "-a").touch()
        (self.root / "-").touch()
        self.check(["--", "-a"], b"-a\n")
        self.check(["-"], b"-\n")

    @unittest.skipIf(options.reference, "project long layout")
    def test_long_metadata(self):
        (self.root / "file").write_bytes(b"abc\x00\xff")
        (self.root / "file").chmod(0o4751)
        os.utime(self.root / "file", (-2, -1))
        (self.root / "dir").mkdir()
        (self.root / "link").symlink_to("dir")
        (self.root / "dangling").symlink_to("missing")
        os.mkfifo(self.root / "fifo")
        names = ["dangling", "dir", "fifo", "file", "link"]
        def row(name):
            info = (self.root / name).lstat()
            return (f"{stat.filemode(info.st_mode)}\t{info.st_uid}\t"
                    f"{info.st_gid}\t{info.st_size}\t"
                    f"{info.st_mtime_ns // 1000000000}\t{name}\n").encode()
        self.check(["-l"], b"".join(row(n) for n in names))
        self.check(["-l", "file"], row("file"))
        self.check(["-l", "link"], row("link"))
        before = (self.root / "file").stat()
        self.check(["-l", "file"], row("file"))
        after = (self.root / "file").stat()
        self.assertEqual(before.st_atime_ns, after.st_atime_ns)
        self.assertEqual(before.st_mtime_ns, after.st_mtime_ns)

    @unittest.skipIf(options.reference, "project JSON contract")
    def test_json(self):
        name = 'quote"\\\n\t\x01é😀'
        (self.root / name).write_bytes(b"binary\x00\xff")
        os.utime(self.root / name, (-2, -1))
        (self.root / "dir").mkdir()
        (self.root / "link").symlink_to("dir")
        (self.root / "dangling").symlink_to("missing")
        os.mkfifo(self.root / "fifo")
        types = {name: "regular", "dir": "directory", "link": "symlink",
                 "dangling": "symlink", "fifo": "fifo"}
        expected = []
        for n in sorted(types, key=os.fsencode):
            info = (self.root / n).lstat()
            expected.append({"name": n, "type": types[n], "size": info.st_size,
                             "mtime": info.st_mtime_ns // 1000000000})
        for args in (["--json"], ["-l", "--json"], ["--json", "-l"]):
            result = self.run_ls(args)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, b"")
            self.assertEqual(json.loads(result.stdout), expected)
            self.assertTrue(result.stdout.endswith(b"\n"))
        (self.root / "empty").mkdir()
        self.check(["--json", "empty"], b"[]\n")
        result = self.run_ls(["--json", "link"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout),
                         [next(x for x in expected if x["name"] == "link")])

    @unittest.skipIf(options.reference, "project encoding policy")
    def test_binary_names(self):
        name = os.fsdecode(b"z-bad-\xff")
        (self.root / name).touch()
        (self.root / "a-good").touch()
        self.check([], b"a-good\nz-bad-\xff\n")
        self.check(["--json"], b"", 1)
        self.check(["--json", "--", name], b"", 1)
        (self.root / name).unlink()
        (self.root / os.fsdecode(b".bad-\xff")).touch()
        result = self.run_ls(["--json"])
        self.assertEqual(result.returncode, 0)
        self.assertEqual([x["name"] for x in json.loads(result.stdout)], ["a-good"])
        self.check(["--json", "-a"], b"", 1)

    @unittest.skipIf(options.reference, "project output contract")
    def test_output_failure(self):
        (self.root / "file").touch()
        for args in ([], ["-l"], ["--json"], ["--help"]):
            with open("/dev/full", "wb") as full:
                result = subprocess.run([PROGRAM, *args], cwd=self.root,
                                        stdout=full, stderr=subprocess.PIPE,
                                        timeout=5)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertTrue(result.stderr)
            for restore in (True, False):
                reader, writer = os.pipe()
                os.close(reader)
                try:
                    result = subprocess.run([PROGRAM, *args], cwd=self.root,
                                            stdout=writer, stderr=subprocess.PIPE,
                                            restore_signals=restore, timeout=5)
                finally:
                    os.close(writer)
                self.assertEqual(result.returncode,
                                 -signal.SIGPIPE if restore else 1)
                self.assertEqual(bool(result.stderr), not restore)

    def test_many_and_unusual_names(self):
        names = [f"entry-{i:04d}" for i in range(260)]
        names += ['a"\\\n\t\x01é😀', "x" * 240, os.fsdecode(b"raw-\xff")]
        for name in reversed(names):
            (self.root / name).write_bytes(b"preserve")
        expected = b"".join(os.fsencode(n) + b"\n"
                            for n in sorted(names, key=os.fsencode))
        self.check([], expected)
        self.assertEqual(sorted(os.listdir(self.root), key=os.fsencode),
                         sorted(names, key=os.fsencode))

    @unittest.skipIf(options.reference, "project metadata boundaries")
    def test_json_all_and_special_modes(self):
        for number, mode in enumerate((0o0000, 0o7777, 0o7000, 0o1754,
                                       0o2750, 0o4751)):
            path = self.root / f"mode-{number}"
            with path.open("wb") as stream:
                stream.truncate((1 << 33) + number)
            path.chmod(mode)
            os.utime(path, (-2, -1))
            info = path.stat()
            expected = (f"{stat.filemode(info.st_mode)}\t{info.st_uid}\t"
                        f"{info.st_gid}\t{info.st_size}\t-1\t{path.name}\n")
            self.check(["-l", path.name], expected.encode())
        result = self.run_ls(["--json", "-a"])
        self.assertEqual(result.returncode, 0, result.stderr)
        entries = json.loads(result.stdout)
        names = [".", ".."] + [f"mode-{i}" for i in range(6)]
        self.assertEqual([e["name"] for e in entries], names)
        for entry in entries:
            info = (self.root / entry["name"]).lstat()
            self.assertEqual(entry, {"name": entry["name"],
                                    "type": "directory" if entry["name"].startswith(".") else "regular",
                                    "size": info.st_size,
                                    "mtime": info.st_mtime_ns // 1000000000})

    @unittest.skipIf(options.reference, "project permission status")
    def test_permission_failure(self):
        path = self.root / "denied"
        path.mkdir()
        path.chmod(0)
        try:
            if os.geteuid() == 0:
                self.skipTest("root bypasses DAC permission checks")
            self.check(["denied"], b"", 1)
            self.check(["--json", "denied"], b"", 1)
        finally:
            path.chmod(0o700)

    def test_directory_relative_lookup(self):
        # The entry path exceeds PATH_MAX when joined, but directory and
        # entry arguments remain individually valid. Exercise dirfd lookup.
        parts = ["d" * 120 for _ in range(32)]
        fd = os.open(self.root, os.O_RDONLY | os.O_DIRECTORY)
        try:
            for part in parts:
                os.mkdir(part, dir_fd=fd)
                next_fd = os.open(part, os.O_RDONLY | os.O_DIRECTORY, dir_fd=fd)
                os.close(fd)
                fd = next_fd
            name = "n" * 240
            file_fd = os.open(name, os.O_CREAT | os.O_WRONLY, 0o600, dir_fd=fd)
            os.close(file_fd)
            self.check(["/".join(parts)], name.encode() + b"\n")
            if not options.reference:
                info = os.stat(name, dir_fd=fd)
                result = self.run_ls(["--json", "/".join(parts)])
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(json.loads(result.stdout), [{
                    "name": name, "type": "regular", "size": 0,
                    "mtime": info.st_mtime_ns // 1000000000}])
        finally:
            os.close(fd)

    @unittest.skipIf(options.reference, "project JSON type and invariant contract")
    def test_json_types_and_no_mutations(self):
        sock = socket.socket(socket.AF_UNIX)
        self.addCleanup(sock.close)
        sock.bind(str(self.root / "socket"))
        (self.root / "link").symlink_to("socket")
        types = {"socket": "socket", "link": "symlink"}
        for i in range(24):
            name = f"name-{i:02d}-" + chr(i + 1) + "é😀"
            path = self.root / name
            path.write_bytes(bytes(range(i)))
            path.chmod((i * 73) & 0o777)
            os.utime(path, (-2, i * 17))
            types[name] = "regular"
        def snapshot():
            return {name: (os.lstat(self.root / name).st_ino,
                           os.lstat(self.root / name).st_mode,
                           os.lstat(self.root / name).st_size,
                           os.lstat(self.root / name).st_mtime_ns,
                           os.lstat(self.root / name).st_ctime_ns,
                           os.lstat(self.root / name).st_atime_ns)
                    for name in types}
        before = snapshot()
        result = self.run_ls(["--json"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, b"")
        expected = []
        for name in sorted(types, key=os.fsencode):
            info = (self.root / name).lstat()
            expected.append({"name": name, "type": types[name],
                             "size": info.st_size,
                             "mtime": info.st_mtime_ns // 1000000000})
        self.assertEqual(json.loads(result.stdout), expected)
        self.assertEqual(snapshot(), before)
        self.assertEqual(os.readlink(self.root / "link"), "socket")
        info = os.lstat("/dev/null")
        result = self.run_ls(["--json", "/dev/null"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), [{
            "name": "/dev/null", "type": "character", "size": info.st_size,
            "mtime": info.st_mtime_ns // 1000000000}])

    @unittest.skipIf(options.reference, "project parser/help contract")
    def test_parser_repetition_help_and_placement(self):
        (self.root / "file").touch()
        for args in (["-aa"], ["-ll"], ["-11"], ["-hh"],
                     ["-a", "--all"], ["--json", "--json"],
                     ["-h", "--help"]):
            with self.subTest(args=args):
                self.check(args, b"", 2)
        self.check(["file", "-1"], b"file\n")
        self.check(["--", "file"], b"file\n")
        outputs = []
        for option in ("-h", "--help"):
            result = self.run_ls([option, "missing"])
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, b"")
            for word in (b"--help", b"--all", b"-l", b"-1", b"--json",
                         b"FILE", b"0 success", b"1 operational",
                         b"2 argument"):
                self.assertIn(word, result.stdout)
            outputs.append(result.stdout)
        self.assertEqual(outputs[0], outputs[1])

    @unittest.skipIf(options.reference, "project UTF-8 scalar contract")
    def test_utf8_scalar_boundaries(self):
        valid = [b"\x7f", b"\xc2\x80", b"\xdf\xbf", b"\xe0\xa0\x80",
                 b"\xed\x9f\xbf", b"\xee\x80\x80", b"\xef\xbf\xbf",
                 b"\xf0\x90\x80\x80", b"\xf4\x8f\xbf\xbf"]
        for name in valid:
            (self.root / os.fsdecode(name)).touch()
        result = self.run_ls(["--json"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, b"")
        self.assertEqual([os.fsencode(e["name"]) for e in
                          json.loads(result.stdout)], sorted(valid))
        invalid = [b"\x80", b"\xc0\xaf", b"\xc1\xbf", b"\xc2",
                   b"\xc2A", b"\xe0\x9f\xbf", b"\xed\xa0\x80",
                   b"\xe2\x82", b"\xf0\x8f\xbf\xbf", b"\xf4\x90\x80\x80",
                   b"\xf5\x80\x80\x80", b"\xf0\x90\x80"]
        for suffix in invalid:
            name = b"z-invalid-" + suffix
            path = self.root / os.fsdecode(name)
            with self.subTest(name=name):
                path.touch()
                self.check(["--json"], b"", 1)
                self.check(["--json", "--", os.fsdecode(name)], b"", 1)
                self.check(["--", os.fsdecode(name)], name + b"\n")
                path.unlink()

    @unittest.skipIf(options.reference, "project forbidden-mutation contract")
    def test_no_mutations_all_modes(self):
        (self.root / "file").write_bytes(b"must not read or mutate")
        (self.root / "dir").mkdir()
        (self.root / "link").symlink_to("file")
        os.mkfifo(self.root / "fifo")
        os.utime(self.root / "file", (1, 2))
        def snapshot():
            names = sorted(os.listdir(self.root), key=os.fsencode)
            paths = [self.root] + [self.root / name for name in names]
            metadata = []
            for path in paths:
                info = path.lstat()
                metadata.append((info.st_ino, info.st_mode, info.st_uid,
                                 info.st_gid, info.st_nlink, info.st_size,
                                 info.st_mtime_ns, info.st_ctime_ns,
                                 None if path == self.root else info.st_atime_ns))
            return names, metadata
        before = snapshot()
        for args in ([], ["-a"], ["-l"], ["--json"], ["--help"],
                     ["missing"], ["--bad"]):
            result = self.run_ls(args)
            self.assertEqual(result.returncode,
                             1 if args == ["missing"] else
                             2 if args == ["--bad"] else 0, result.stderr)
            self.assertEqual(snapshot(), before)
        self.assertEqual((self.root / "file").read_bytes(),
                         b"must not read or mutate")
        self.assertEqual(os.readlink(self.root / "link"), "file")


if __name__ == "__main__":
    unittest.main(argv=[__file__, *options.tests], verbosity=2)
