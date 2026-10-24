#!/usr/bin/env python3
"""Build pinned static dependencies into NEW local prefixes; see docs/TESTING.md."""

import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request


ARGTABLE_COMMIT = "f25c624e3a5a65f4d1897aa7d51c897f8f8098d0"
SOURCES = {
    "argtable3": (
        f"https://codeload.github.com/argtable/argtable3/tar.gz/{ARGTABLE_COMMIT}",
        "a0fc43317d31447dc2dda5852e71e4089e7eb4ed8429e2315a36533cfb4ce9e3",
        f"argtable3-{ARGTABLE_COMMIT}",
    ),
    "cunit": (
        "https://downloads.sourceforge.net/project/cunit/CUnit/2.1-3/CUnit-2.1-3.tar.bz2",
        "f5b29137f845bb08b77ec60584fdb728b4e58f1023e6f249a464efa49a40f214",
        "CUnit-2.1-3",
    ),
}


def run(args, cwd):
    print("+ " + " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, check=True)


def unpack(archive, destination, expected_root):
    """Accept only ordinary files/directories below the expected archive root."""
    with tarfile.open(archive) as bundle:
        members = bundle.getmembers()
        for member in members:
            path = PurePosixPath(member.name)
            if (path.is_absolute() or ".." in path.parts or not path.parts
                    or path.parts[0] != expected_root
                    or not (member.isfile() or member.isdir())):
                raise ValueError(f"Unsafe archive member: {member.name}")
        # No archive-controlled links, devices, ownership or privileged modes.
        for member in members:
            target = destination / member.name
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                source = bundle.extractfile(member)
                if source is None:
                    raise ValueError(f"Missing archive file data: {member.name}")
                with source, target.open("xb") as output:
                    shutil.copyfileobj(source, output)
                target.chmod(0o755 if member.mode & 0o111 else 0o644)


def download(url, digest, archive):
    print(f"Downloading {url}", flush=True)
    with urllib.request.urlopen(url, timeout=120) as response:
        if not response.geturl().startswith("https://"):
            raise ValueError("Refusing non-HTTPS download redirect")
        data = response.read(16 * 1024 * 1024 + 1)
    if len(data) > 16 * 1024 * 1024:
        raise ValueError("Dependency archive exceeds 16 MiB limit")
    actual = hashlib.sha256(data).hexdigest()
    if actual != digest:
        raise ValueError(f"SHA-256 mismatch for {url}: {actual}")
    archive.write_bytes(data)
    print(f"Verified SHA-256 {actual}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path,
                        default=Path(__file__).resolve().parents[1] / "build",
                        help="parent of NEW argtable3/ and cunit/ (default: repo/build)")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--only", choices=list(SOURCES), nargs="+",
                        default=list(SOURCES))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    root = args.build_root.expanduser().absolute()
    # Refuse symlinked destination ancestry, including dangling links.
    for path in (root, *root.parents):
        if path.is_symlink():
            parser.error(f"symlinked build-root ancestry: {path}")
    for name in args.only:
        if os.path.lexists(root / name):
            parser.error(f"refusing existing prefix: {root / name}; choose a new --build-root")
    tools = ["cc", "ar"]
    if "cunit" in args.only:
        tools += ["make", "ranlib", "m4", "perl", "libtoolize", "aclocal",
                  "autoheader", "automake", "autoconf"]
    for tool in tools:
        if not shutil.which(tool):
            parser.error(f"missing build tool: {tool}; see docs/TESTING.md")
    root.mkdir(parents=True, exist_ok=True)
    # Reserve both destinations before building. Failure leaves partial prefixes
    # for inspection, never deletes/replaces any prefix, and a rerun refuses them.
    for name in args.only:
        (root / name).mkdir()
    with tempfile.TemporaryDirectory(prefix=".bootstrap-", dir=root) as temporary:
        work = Path(temporary)
        for name, (url, digest, source_root) in SOURCES.items():
            if name not in args.only:
                continue
            archive = work / (name + ".tar")
            download(url, digest, archive)
            unpack(archive, work, source_root)
            source = work / source_root
            prefix = root / name
            if name == "argtable3":
                # Follow upstream's src list with its Linux defaults. No
                # CMake/autotools are needed for this simple static archive.
                (prefix / "include").mkdir()
                (prefix / "lib").mkdir()
                objects = []
                for unit in sorted((source / "src").glob("*.c")):
                    target = work / (unit.stem + ".o")
                    run(["cc", "-std=c99", "-D_XOPEN_SOURCE=700", "-O2",
                         "-fPIC", "-fvisibility=hidden", "-c", unit,
                         "-o", target], work)
                    objects.append(target)
                run(["ar", "rcs", prefix / "lib/libargtable3.a", *objects], work)
                shutil.copyfile(source / "src/argtable3.h",
                                prefix / "include/argtable3.h")
                licenses = prefix / "share/licenses/argtable3"
                licenses.mkdir(parents=True)
                shutil.copyfile(source / "LICENSE", licenses / "LICENSE")
            else:
                # This official archive lacks generated configure/Makefile.in.
                # Follow its bootstrap's tool sequence, but not its default
                # $HOME install location or optional examples/memtrace settings.
                run(["libtoolize", "--copy"], source)
                run(["aclocal"], source)
                run(["autoheader"], source)
                run(["automake", "--add-missing", "--copy"], source)
                run(["autoconf"], source)
                run(["sh", "./configure", f"--prefix={prefix}",
                     f"--libdir={prefix / 'lib'}", "--enable-static", "--disable-shared",
                     "--disable-curses", "--disable-examples", "--disable-test"], source)
                run(["make", f"-j{args.jobs}"], source)
                run(["make", "install"], source)
                licenses = prefix / "share/licenses/cunit"
                licenses.mkdir(parents=True)
                shutil.copyfile(source / "COPYING", licenses / "COPYING")
            header = "include/argtable3.h" if name == "argtable3" else "include/CUnit/CUnit.h"
            library = "lib/libargtable3.a" if name == "argtable3" else "lib/libcunit.a"
            for relative in (header, library):
                if not (prefix / relative).is_file():
                    raise RuntimeError(f"Missing installed artifact: {prefix / relative}")
    print(f"Installed static dependencies under {root}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError, tarfile.TarError) as error:
        sys.exit(f"bootstrap failed (prefixes preserved): {error}")
