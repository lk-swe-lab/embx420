#!/usr/bin/env python3
"""Independent mkdir fixtures; select reference or project explicitly."""
import argparse
import errno
import json
import os
import signal
from pathlib import Path
import stat
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("program")
parser.add_argument("--reference", action="store_true")
parser.add_argument("--slice", choices=["basic", "parents", "json", "all"], default="all")
args = parser.parse_args()
program = os.path.abspath(args.program)
cases = 0


def run(argv, root, status=0, output=b"", error=False, mask=0o022, stdout=None):
    global cases
    p = subprocess.run([program, *argv], cwd=root, env={**os.environ, "LC_ALL": "C"},
                       input=b"borrowed input", stdout=stdout or subprocess.PIPE,
                       stderr=subprocess.PIPE, timeout=5, umask=mask)
    assert (p.returncode == status if not args.reference else
            (p.returncode == 0) == (status == 0)), (argv, status, p.returncode, p.stderr)
    if stdout is None:
        assert p.stdout == output, (argv, output, p.stdout)
    assert bool(p.stderr) == error, (argv, p.stderr)
    cases += 1
    return p


with tempfile.TemporaryDirectory(prefix="mkdir-cli-", dir=os.environ.get("TMPDIR")) as tmp:
    root = Path(tmp)
    run(["one", "two words", "tab\tname", "line\nname"], root)
    for name in ["one", "two words", "tab\tname", "line\nname"]:
        assert (root / name).is_dir()
        assert stat.S_IMODE((root / name).stat().st_mode) == 0o755
    run(["one", "later"], root, 1, error=True)
    assert (root / "later").is_dir()
    run(["missing/child"], root, 1, error=True)
    assert not (root / "missing").exists()
    run(["--", "-literal"], root)
    (root / "file").write_bytes(b"sentinel\x00bytes")
    run(["file"], root, 1, error=True)
    assert (root / "file").read_bytes() == b"sentinel\x00bytes"
    run([""], root, 1, error=True)
    if args.slice != "basic":
        run(["-p", "parents/a/b", "one"], root)
        assert (root / "parents/a/b").is_dir()
        run(["--parents", "slashes///x//"], root)
        run(["-p", "dots/../sibling/./end"], root)
        assert (root / "dots").is_dir() and (root / "sibling/end").is_dir()
        run(["-p", str(root / "absolute/a")], root)
        os.symlink("one", root / "link")
        run(["link"], root, 1, error=True)
        run(["-p", "link", "link/child"], root)
        assert (root / "link").is_symlink() and (root / "one/child").is_dir()
        os.symlink("absent", root / "dangling")
        os.symlink("loop", root / "loop")
        for name in ["dangling", "loop", "file/child"]:
            run(["-p", name], root, 1, error=True)
        run(["-p", "partial/../file/no", "after-partial"], root, 1, error=True)
        assert (root / "partial").is_dir() and (root / "after-partial").is_dir()
        for mask in [0o000, 0o022, 0o077, 0o300, 0o777]:
            name = f"mask-{mask:o}"
            run(["-p", name + "/leaf"], root, mask=mask)
            parent_mode = stat.S_IMODE((root / name).stat().st_mode)
            assert parent_mode == ((0o777 & ~mask) | 0o300), (mask, parent_mode)
            assert stat.S_IMODE((root / name / "leaf").stat().st_mode) == (0o777 & ~mask)
            os.chmod(root / name / "leaf", 0o700)
            os.chmod(root / name, 0o700)
        (root / "group-parent").mkdir()
        os.chmod(root / "group-parent", 0o2770)
        run(["-p", "group-parent/new/leaf"], root, mask=0o300)
        assert stat.S_IMODE((root / "group-parent").stat().st_mode) == 0o2770
        assert stat.S_IMODE((root / "group-parent/new").stat().st_mode) == 0o2777
        assert stat.S_IMODE((root / "group-parent/new/leaf").stat().st_mode) == 0o2477
        assert (root / "group-parent/new/leaf").stat().st_gid == os.getegid()
        os.chmod(root / "group-parent/new/leaf", 0o700)
        os.chmod(root / "one", 0o710)
        run(["-p", "one"], root)
        assert stat.S_IMODE((root / "one").stat().st_mode) == 0o710
        run(["-p", ".", "..", "/"], root)
        run(["long-" + "x" * 260], root, 1, error=True)
        names = [f"many-{i}" for i in range(100)]
        run(names, root)
        assert all((root / name).is_dir() for name in names)
    if not args.reference and args.slice in ["json", "all"]:
        escaped = 'json-"\\\t\n-é'
        expected = ('[{"path":' + json.dumps(escaped, ensure_ascii=False, separators=(",", ":")) +
                    ',"status":"ok","message":null}]\n').encode()
        run(["--json", escaped], root, output=expected)
        assert (root / escaped).is_dir()
        expected = (b'[{"path":"json-parent/leaf","status":"ok","message":null},'
                    b'{"path":"one","status":"ok","message":null}]\n')
        run(["--json", "-p", "json-parent/leaf", "one"], root, output=expected)
        assert (root / "json-parent/leaf").is_dir()
        expected = ('[{"path":"one","status":"error","message":' +
                    json.dumps(os.strerror(errno.EEXIST)) +
                    '},{"path":"json-later","status":"ok","message":null}]\n').encode()
        run(["--json", "one", "json-later"], root, 1, expected, True)
        assert (root / "json-later").is_dir()
        for bad in [b"bad-\xff", b"bad-\xc0\x80", b"bad-\xed\xa0\x80",
                    b"bad-\xf4\x90\x80\x80", b"bad-\xe2\x82"]:
            run([b"--json", b"must-not-create", bad], root, 1, error=True)
            assert not (root / "must-not-create").exists()
            assert not os.path.exists(os.fsencode(root) + b"/" + bad)
        run([b"raw-\xff"], root)
        assert os.path.isdir(os.fsencode(root) + b"/raw-\xff")
    if not args.reference and args.slice == "all":
        for argv in [[], ["--bad", "forbidden"], ["-m", "700", "forbidden"],
                     ["--version"], ["-p", "-p", "forbidden"],
                     ["--json", "--json", "forbidden"], ["--help", "--bad", "forbidden"],
                     ["forbidden", "--bad"], ["-pp", "forbidden"],
                     ["--help", "--help", "forbidden"],
                     ["--json=yes", "forbidden"], ["--parents=yes", "forbidden"]]:
            run(argv, root, 2, error=True)
            assert not (root / "forbidden").exists()
        plain_help = subprocess.run([program, "--help"], cwd=root,
                                    capture_output=True, timeout=5)
        expected_help = (
            b"Usage: mkdir [-hp] [--json] DIRECTORY [DIRECTORY]...\n\n"
            b"  -h, --help           Show help and exit.\n"
            b"  -p, --parents        Create missing parents.\n"
            b"  --json               Emit UTF-8 JSON results.\n"
            b"  DIRECTORY            One or more directories to create.\n"
            b"\nCreate directories; -p accepts existing directories.\n"
            b"Exit status: 0 success, 1 operational failure, 2 argument error.\n")
        assert plain_help.returncode == 0 and not plain_help.stderr
        assert plain_help.stdout == expected_help
        cases += 1
        for flag in ["-h", "--help"]:
            p = subprocess.run([program, flag, "help-forbidden"], cwd=root,
                               capture_output=True, timeout=5)
            assert p.returncode == 0 and not p.stderr
            assert p.stdout == plain_help.stdout
            assert p.stdout.startswith(b"Usage: mkdir") and b"--json" in p.stdout
            assert b"--parents" in p.stdout and b"DIRECTORY" in p.stdout
            assert not (root / "help-forbidden").exists()
            cases += 1
        with open("/dev/full", "wb") as full:
            run(["-p", "one"], root, stdout=full)
            run(["--help"], root, 1, error=True, stdout=full)
            run(["--json", "output-created"], root, 1, error=True, stdout=full)
        assert (root / "output-created").is_dir()
        reader, writer = os.pipe()
        os.close(reader)
        try:
            p = subprocess.run([program, "--json", "pipe-created"], cwd=root,
                               stdout=writer, stderr=subprocess.PIPE, timeout=5)
            assert p.returncode == -signal.SIGPIPE and not p.stderr
            assert (root / "pipe-created").is_dir()
            cases += 1
        finally:
            os.close(writer)
        if os.geteuid() != 0:
            (root / "denied").mkdir(mode=0o500)
            run(["denied/child"], root, 1, error=True)
            assert not (root / "denied/child").exists()
            os.chmod(root / "denied", 0o700)
        assert (root / "file").read_bytes() == b"sentinel\x00bytes"
print(f"mkdir {'reference' if args.reference else 'project'}: {cases} exact fixtures passed ({args.slice})")
