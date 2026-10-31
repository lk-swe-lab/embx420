#!/usr/bin/env python3
"""Show explicit package selection in the empty host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    """Stop on an unexpected result, including under python -O."""
    if not condition:
        raise RuntimeError(message)


def hashes(paths):
    """Record real artifact bytes before and after the demo."""
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}


def names(text):
    """Read package names from the host's JSON output."""
    return [entry["name"] for entry in json.loads(text)["packages"]]


def main():
    """Run real shell processes in an isolated HOME; capture their output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "demo")
    parser.add_argument("--catalog", type=Path,
                        default=ROOT / "build/catalog-0.3.1/catalog.json")
    args = parser.parse_args()
    output = args.output_dir.absolute()
    output.mkdir(parents=True, exist_ok=True)
    host = ROOT / "build/aishell"
    catalog = args.catalog.absolute()
    record = {"status": "FAIL", "events": []}
    lines = ["Empty host and explicit package selection"]
    fixture = None
    try:
        temporary = os.environ.get("TMPDIR", "")
        require(temporary.startswith("/") and Path(temporary).is_dir(),
                "TMPDIR must be an existing absolute directory")
        require(host.is_file() and catalog.is_file(),
                "prepare with make and make catalog")
        artifacts = [host, *sorted(p for p in catalog.parent.rglob("*")
                                  if p.is_file())]
        before = hashes(artifacts)
        fixture = tempfile.TemporaryDirectory(prefix="empty-host-demo-",
                                              dir=temporary)
        space = Path(fixture.name)
        home = space / "home"
        home.mkdir()
        env = {"HOME": str(home), "PATH": "/nonexistent", "LC_ALL": "C"}

        def run(label, *args, data=None, status=0, program=host, errors=""):
            argv = [str(program), *map(str, args)]
            result = subprocess.run(argv, input=data, text=True,
                                    capture_output=True, cwd=space,
                                    env=env, timeout=10)
            event = {"label": label, "argv": argv, "stdin": data or "",
                     "stdout": result.stdout, "stderr": result.stderr,
                     "status": result.returncode, "expected_status": status}
            record["events"].append(event)
            lines.extend(["\n$ " + shlex.join(argv), data or "",
                          result.stdout, result.stderr,
                          "status=" + str(result.returncode)])
            require(result.returncode == status, label + ": unexpected status")
            require(result.stderr == errors, label + ": unexpected stderr")
            return result.stdout

        available = run("available", "--catalog", catalog, "--available-json")
        require({"hello", "pwd", "pkg"} <= set(names(available)),
                "available package list is incomplete")
        require(names(run("empty", "--catalog", catalog, "--loaded-json")) == [],
                "startup loaded an unselected package")
        unknown = "main: hello: unknown command\n"
        run("unavailable-command", "--catalog", catalog, "hello", status=127,
            errors=unknown)
        selected = run("selected", "--catalog", catalog, "--load", "pwd",
                       "--loaded-json")
        require(names(selected) == ["pwd"], "selection loaded extra packages")
        live = run("live", "--catalog", catalog,
                   data=":load hello\nhello --name learner\n:loaded\n:quit\n")
        require("Hello, learner!" in live, "live load did not add hello")
        entry = next(p for p in json.loads(available)["packages"]
                     if p["name"] == "hello")
        package = catalog.parent / entry["path"]
        run("install", "--catalog", catalog, "--load", "pkg", "pkg", "install",
            package)
        run("installed-not-loaded", "--catalog", catalog, "hello", status=127,
            errors=unknown)
        require(names(run("fresh-session", "--catalog", catalog,
                          "--loaded-json")) == [],
                "installed selections were loaded automatically")
        standalone = space / "standalone"
        standalone.mkdir()
        relocated = standalone / "aishell"
        shutil.copy2(host, relocated)
        require(names(run("host-only", "--loaded-json", program=relocated)) == [],
                "standalone host needs bundled modules")
        require(list(standalone.iterdir()) == [relocated],
                "unexpected host-only payload")
        require(hashes(artifacts) == before, "demo changed build artifacts")
        record["input_sha256"] = before
        record["status"] = "PASS"
    except (OSError, ValueError, KeyError, RuntimeError,
            subprocess.SubprocessError) as error:
        record["error"] = str(error)
        lines.append("ERROR: " + str(error))
    finally:
        if fixture is not None:
            root = Path(fixture.name)
            fixture.cleanup()
            record["cleanup_verified"] = not root.exists()
            if not record["cleanup_verified"]:
                record["status"] = "FAIL"
        for suffix, data in (("json", json.dumps(record, indent=2) + "\n"),
                             ("log", "\n".join(lines) + "\n")):
            target = output / ("catalog-demo." + suffix)
            require(not target.is_symlink(), "refuse linked demo output")
            target.write_text(data)
    print(record["status"] + ": see " + str(output / "catalog-demo.log"))
    return 0 if record["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
