"""Build an immutable local ELF shell release, not a pkg payload."""

import argparse
import ctypes
import re
import stat
import subprocess
import sys
import tempfile
from pathlib import Path
from build_modules import specifications

from build_package import (
    ROOT,
    command_check,
    json_read,
    manifest_check,
    no_links,
    regular_read,
    require,
    source_inventory,
)

GUIDE = """# {name} {version}

## Install and run

Copy this entire directory to a user-owned location, for example
`$HOME/.local/opt/{name}-{version}`. No source tree, compiler or Python is
needed to run it. From that directory run:

```sh
./bin/aishell --help
./bin/aishell pwd
./bin/aishell help pkg
./bin/aishell --commands-json
./bin/aishell
```

The last form starts the command loop. Use `help` and `exit` inside it.
`docs/*.md` is generated builtin help. The catalog is a clean-HOME snapshot;
installed commands appear in the live catalog, not this build-time file.

## Manual shell update

Exit old shell sessions. Copy the complete new release to a different versioned
directory. Test its binary, then change your launcher to that binary. Keep the
old directory for rollback. Do not copy over a running release. This shell has
no self-update, no automatic sudo installation, and no remote download.
The release is NOT a package accepted by `pkg install`.

## Optional demonstration package (included)

The `examples/{demo}` directory is a prebuilt hello package, not a builtin.
From this release directory run:

```sh
./bin/aishell pkg install examples/{demo}
./bin/aishell pkg list
./bin/aishell pkg info hello
./bin/aishell help hello
./bin/aishell hello --name release
./bin/aishell pkg remove hello
```

Install a separately built schema-2 shared-module package for a new version.
Install selects it atomically and retains earlier immutable versions. Use
`pkg use hello VERSION` for rollback. A version already loaded in this session
stays loaded until restart; the diagnostic states this boundary. `pkg remove`
deactivates commands while retaining version payloads. Do not overwrite libraries.
Installed copies live in `$HOME/.mysh/pkgs`; active selections are regular files
under its `.active` directory. Moving this release does not move those copies.
HOME must be absolute and user-owned state must be safe. Only install trusted
native code: validation loads modules and can execute constructors; callbacks
run sequentially in the shell process. There are no worker threads, sandbox,
signatures or remote downloads. Executable-only schema-1 packages are rejected.

## Limits and system requirements

Read SYSTEM.md for the actual linked ELF requirements. This is NOT a fully
static or cross-platform distribution. No external PATH lookup, pipelines,
redirection, background jobs, dependency solver or automatic repair is provided.
Standard `.so` modules must remain in `bin/modules` beside the executable.
If `pkg.so` is missing, restore the complete matching release externally.
Module help preserves the existing `main` name in some text. Use `bin/aishell`
as the executable. Debug information can contain build paths; they are not
runtime dependencies. This directory has no hidden source-tree runtime paths.
"""


def run(program, *args, env=None):
    """Capture bounded trusted generator output with a deadline."""
    with tempfile.TemporaryFile() as stream:
        subprocess.run(
            [str(program), *args],
            stdout=stream,
            stderr=subprocess.PIPE,
            env=env,
            check=True,
            timeout=10,
        )
        stream.seek(0)
        data = stream.read(1048577)
    require(len(data) <= 1048576, "generator output too large")
    return data


def inventory(root):
    """Read exact safe directory inventory, bytes and permission modes."""
    no_links(root)
    require(root.is_dir(), "expected directory")
    result = {".": ("directory", stat.S_IMODE(root.stat().st_mode))}
    for path in sorted(root.rglob("*")):
        no_links(path)
        mode = path.lstat().st_mode
        key = path.relative_to(root).as_posix()
        if stat.S_ISDIR(mode):
            result[key] = ("directory", stat.S_IMODE(mode))
        else:
            result[key] = (regular_read(path, 16777216), stat.S_IMODE(mode))
    return result


def requirements(binary, shared=False):
    """Verify loader resolution and record ELF ABI requirements without paths."""
    dynamic = run("/usr/bin/readelf", "-d", str(binary)).decode()
    require(
        "RPATH" not in dynamic and "RUNPATH" not in dynamic,
        "ELF search paths are unsupported",
    )
    needed = re.findall(r"\(NEEDED\).*?\[(.*?)\]", dynamic)
    require(all("/" not in name for name in needed), "absolute ELF dependency")
    linked = run("/usr/bin/ldd", str(binary)).decode()
    require("not found" not in linked, "unresolved shared dependency")
    header = run("/usr/bin/readelf", "-h", str(binary)).decode()
    machine_match = re.search(r"Machine:\s*(.+)", header)
    if machine_match is None:
        raise ValueError("ELF machine missing")
    machine = machine_match.group(1)
    program = run("/usr/bin/readelf", "-l", str(binary)).decode()
    loader = re.search(r"Requesting program interpreter: (.*?)\]", program)
    if loader is None and not shared:
        raise ValueError("dynamic ELF interpreter missing")
    versions = sorted(
        set(
            re.findall(
                r"GLIBC_[0-9.]+",
                run("/usr/bin/readelf", "--version-info", str(binary)).decode(),
            )
        ),
        key=lambda s: tuple(map(int, s[6:].split("."))),
    )
    require(versions, "no glibc ABI requirement found")
    return (
        "Machine: "
        + machine
        + "\nLoader: "
        + (loader.group(1) if loader else "host dynamic linker")
        + "\nShared libraries: "
        + ", ".join(needed)
        + "\nRequired symbol versions: "
        + ", ".join(versions)
        + "\nMinimum glibc symbol ABI: "
        + versions[-1]
        + "\n"
    )


def build(args):
    """Generate privately; never replace, move or remove existing output."""
    output = args.output_root.resolve()
    inputs = [
        args.manifest.parent,
        args.demo_package,
        args.binary,
    ]
    for path in inputs:
        resolved = path.resolve()
        require(
            output != resolved
            and output not in resolved.parents
            and resolved not in output.parents,
            "input/output ancestry overlap",
        )
        no_links(path)
    raw = regular_read(args.manifest, 65536)
    identity = json_read(raw, 65536)
    require(
        set(identity) == {"schema_version", "name", "version", "description"}
        and type(identity["schema_version"]) is int
        and identity["schema_version"] == 1,
        "release fields mismatch",
    )
    require(
        re.fullmatch("[a-z][a-z0-9-]{0,62}", identity["name"])
        and re.fullmatch(
            r"(0|[1-9][0-9]{0,4})(\.(0|[1-9][0-9]{0,4})){2}", identity["version"]
        )
        and all(int(p) <= 65535 for p in identity["version"].split(".")),
        "invalid release identity",
    )
    require(isinstance(identity["description"], str), "invalid description")
    require(
        sorted(p.name for p in args.manifest.parent.iterdir()) == [args.manifest.name],
        "unlisted release source entry",
    )
    binary = regular_read(args.binary, 16777216, executable=True)
    payload = {"release.json": raw, "bin/aishell": binary}
    modules = args.binary.parent / "modules"
    no_links(modules)
    module_payload = {}
    for path in sorted(modules.iterdir()):
        if path.name == ".stamp":
            continue
        require(re.fullmatch(r"[a-z][a-z0-9-]{0,59}\.so", path.name),
                "unexpected standard module entry")
        module_payload[path.name] = regular_read(path, 16777216)
    expected = {name + ".so" for _, _, name in specifications()
                if name != "hello"}
    require(set(module_payload) == expected,
            "standard module inventory differs from source specifications")
    for name, data in module_payload.items():
        payload["bin/modules/" + name] = data
    demo_record = json_read(regular_read(args.demo_package / "pkg.json", 65536), 65536)
    manifest_check(demo_record)
    require(demo_record["commands"] == ["hello"], "expected hello demonstration")
    source_inventory(args.demo_package, set(demo_record["files"]))
    demo = demo_record["name"] + "-" + demo_record["version"]
    demo_meta = json_read(
        regular_read(args.demo_package / "metadata/commands.json", 262144), 262144
    )
    require(
        demo_meta["package"] == demo_record["name"]
        and demo_meta["version"] == demo_record["version"]
        and len(demo_meta["commands"]) == 1,
        "demo metadata mismatch",
    )
    command_check(demo_meta["commands"][0])
    for name in ["pkg.json", *demo_record["files"]]:
        payload["examples/" + demo + "/" + name] = regular_read(
            args.demo_package / name, 16777216, executable=name == "bin/hello"
        )
    with tempfile.TemporaryDirectory(prefix="release-home-") as home:
        # Execute the exact captured payload, not a mutable source path or override.
        generator = Path(home) / "aishell"
        generator.write_bytes(binary)
        generator.chmod(0o755)
        (Path(home) / "modules").mkdir()
        for name, data in module_payload.items():
            (Path(home) / "modules" / name).write_bytes(data)
        shell_requirements = requirements(generator)
        env = {"HOME": home, "LC_ALL": "C", "PATH": "/nonexistent"}
        catalog_raw = run(generator, "--commands-json", env=env)
        catalog = json_read(catalog_raw, 1048576)
        require(
            catalog["schema_version"] == 1 and catalog["commands"],
            "invalid generated catalog",
        )
        names = [entry["name"] for entry in catalog["commands"]]
        require(names == sorted(set(names)), "invalid catalog names")
        require(set(name + ".so" for name in names) == set(module_payload),
                "standard module/catalog inventory mismatch")
        payload["metadata/commands.json"] = catalog_raw
        for entry in catalog["commands"]:
            require(
                re.fullmatch("[a-z][a-z0-9-]{0,59}", entry["name"])
                and entry["origin"] in ("builtin", "control"),
                "invalid catalog",
            )

            help_text = run(generator, "help", entry["name"], env=env).decode()
            require(help_text.strip(), "empty builtin help: " + entry["name"])
            payload["docs/" + entry["name"] + ".md"] = (
                "# "
                + entry["name"]
                + "\n\n"
                + entry["summary"]
                + "\n\n```text\n"
                + help_text
                + "```\n"
            ).encode()
    payload["README.md"] = GUIDE.format(**identity, demo=demo).encode()
    payload["SYSTEM.md"] = (
        "# System requirements\n\nLinux with compatible "
        "ELF architecture, loader and libc.\n\n"
        + shell_requirements
        + "\nDemo hello requirements:\n"
        + requirements((args.demo_package / "lib/hello.so").absolute(), shared=True)
        + "\nStandard command module requirements:\n"
        + "\n".join(name + "\n" + requirements(modules / name, shared=True)
                     for name in module_payload)
        + "\nPackage operations require Linux renameat2 "
        "RENAME_NOREPLACE support (Linux 3.15+), a local "
        "filesystem with this operation and POSIX locks.\n"
        "The glibc ABI bound is not a tested oldest OS.\n"
    ).encode()
    no_links(args.output_root)
    args.output_root.mkdir(parents=True, exist_ok=True)
    destination = args.output_root / (identity["name"] + "-" + identity["version"])
    no_links(destination)
    with tempfile.TemporaryDirectory(prefix=".release-", dir=args.output_root) as tmp:
        stage = Path(tmp) / "payload"
        stage.mkdir()
        for name, data in payload.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            target.chmod(0o755 if name == "bin/aishell" else 0o644)
        for path in [stage, *stage.rglob("*")]:
            if path.is_dir():
                path.chmod(0o755)
        if destination.exists():
            require(
                inventory(destination) == inventory(stage),
                "existing output differs; use a new version or output root",
            )
        else:
            # Linux atomic no-replace publication, including an empty target.
            libc = ctypes.CDLL(None, use_errno=True)
            rename = libc.renameat2
            rename.argtypes = [
                ctypes.c_int,
                ctypes.c_char_p,
                ctypes.c_int,
                ctypes.c_char_p,
                ctypes.c_uint,
            ]
            rename.restype = ctypes.c_int
            if rename(-100, bytes(stage), -100, bytes(destination), 1) != 0:
                raise OSError(ctypes.get_errno(), "release publication failed")
    return destination


def main():
    """Select trusted build inputs and return one diagnostic on failure."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--manifest", type=Path, default=ROOT / "packages/aishell/release.json"
    )
    parser.add_argument("--binary", type=Path, default=ROOT / "build/aishell")
    demo_identity = json_read(
        regular_read(ROOT / "packages/hello-so/pkg.json", 65536), 65536
    )
    parser.add_argument(
        "--demo-package",
        type=Path,
        default=ROOT
        / "build/native-candidates"
        / (demo_identity["name"] + "-" + demo_identity["version"]),
    )
    parser.add_argument("--output-root", type=Path, default=ROOT / "build/so-reviewed-releases")
    try:
        print(build(parser.parse_args()))
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        AttributeError,
        subprocess.SubprocessError,
    ) as error:
        print("build-shell-release: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
