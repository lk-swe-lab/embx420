"""Build the shell host without any command packages."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import sys
import tempfile
from build_package import no_links, publish_directory, regular_read, require

ROOT = Path(__file__).resolve().parents[1]
GUIDE = """# AiShell host

No command packages are included or loaded at startup.
Keep the package catalog in a separate directory. Run:

    ./bin/aishell --catalog /absolute/path/to/catalog.json

Use :available, :load NAME, :loaded, and :quit.
For an agent, use --available-json and repeat --load NAME for selected packages.
Selection is local to this process. Native packages are trusted code.
Linux and compatible ELF/libc are required. No downloads or job control.
"""


def inventory(folder):
    """Read safe bytes and modes without following links."""
    no_links(folder)
    result = {}
    for path in [folder, *sorted(folder.rglob("*"))]:
        no_links(path)
        mode = path.lstat().st_mode
        key = str(path.relative_to(folder))
        if stat.S_ISDIR(mode):
            result[key] = (None, stat.S_IMODE(mode))
        else:
            result[key] = (regular_read(path, 16777216), stat.S_IMODE(mode))
    return result


def build(args):
    """Publish an immutable host-only directory; preserve existing outputs."""
    require(re.fullmatch(r"(0|[1-9][0-9]{0,4})(\.(0|[1-9][0-9]{0,4})){2}",
                         args.version) is not None and
            all(int(n) <= 65535 for n in args.version.split(".")),
            "invalid host version")
    binary = Path(args.binary).absolute()
    output = Path(args.output_root).absolute()
    no_links(output)
    require(output not in binary.parents and output != binary,
            "input/output overlap")
    data = regular_read(binary, 16777216, executable=True)
    output.mkdir(parents=True, exist_ok=True)
    target = output / ("aishell-" + args.version)
    no_links(target)
    with tempfile.TemporaryDirectory(prefix=".host-", dir=output) as temporary:
        stage = Path(temporary) / "release"
        (stage / "bin").mkdir(parents=True)
        (stage / "bin/aishell").write_bytes(data)
        (stage / "bin/aishell").chmod(0o755)
        (stage / "README.md").write_text(GUIDE)
        record = {"schema_version": 1, "name": "aishell",
                  "version": args.version, "packages_included": [],
                  "host_sha256": hashlib.sha256(data).hexdigest()}
        (stage / "release.json").write_text(json.dumps(record, indent=2) + "\n")
        for path in [stage, *stage.rglob("*")]:
            if path.is_dir():
                path.chmod(0o755)
            elif path.name != "aishell":
                path.chmod(0o644)
        if target.exists():
            require(target.is_dir() and inventory(target) == inventory(stage),
                    "existing host release differs; use a new version")
        else:
            publish_directory(stage, target)
    return target


def main():
    """Read build options and report failures without replacing outputs."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/aishell")
    parser.add_argument("--output-root", type=Path,
                        default=ROOT / "build/host-releases")
    parser.add_argument("--version", default="0.3.0")
    try:
        print(build(parser.parse_args()))
    except (OSError, ValueError) as error:
        print("build-host-release: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
