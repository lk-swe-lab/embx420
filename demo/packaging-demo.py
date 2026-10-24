#!/usr/bin/env python3
"""Capture a real, isolated package lifecycle; never build or install to host HOME."""

import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HERE = Path(__file__).resolve().parent
TIMEOUT = 10


def require(condition, message):
    """Use explicit checks, including under python -O."""
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    record = {"schema_version": 1, "status": "FAIL", "events": [], "checks": []}
    lines = ["Package lifecycle demo (real execution; no rebuild)"]
    fixture = None

    def check(condition, message):
        require(condition, message)
        record["checks"].append(message)
        lines.append("CHECK: " + message)

    def run(program, args, env, cwd, expected=0):
        argv = [str(program), *map(str, args)]
        event = {"argv": argv, "cwd": str(cwd), "env": dict(env),
                 "timeout_seconds": TIMEOUT, "expected_status": expected}
        record["events"].append(event)
        lines.append("\n$ " + shlex.join(argv))
        try:
            result = subprocess.run(argv, input=b"", capture_output=True,
                                    env=env, cwd=cwd, timeout=TIMEOUT, check=False)
        except subprocess.TimeoutExpired as error:
            event.update(status=None, timed_out=True,
                         stdout=(error.stdout or b"").decode("utf-8", "replace"),
                         stderr=(error.stderr or b"").decode("utf-8", "replace"))
            lines.extend([event["stdout"], event["stderr"], "[TIMEOUT]"])
            raise
        event.update(status=result.returncode,
                     stdout=result.stdout.decode("utf-8"),
                     stderr=result.stderr.decode("utf-8"))
        if event["stdout"]:
            lines.extend(["stdout:", event["stdout"].rstrip("\n")])
        if event["stderr"]:
            lines.extend(["stderr:", event["stderr"].rstrip("\n")])
        lines.append(f"[status={result.returncode}; expected={expected}]")
        require(result.returncode == expected, "unexpected command status")
        require(expected != 0 or not result.stderr, "unexpected success stderr")
        return event["stdout"], event["stderr"]

    def catalog(program, env, cwd, installed):
        out, _ = run(program, ["--commands-json"], env, cwd)
        value = json.loads(out)
        require(set(value) == {"schema_version", "commands"}
                and type(value["schema_version"]) is int
                and value["schema_version"] == 1, "catalog schema mismatch")
        entries = value["commands"]
        names = [item["name"] for item in entries]
        require(names == sorted(set(names)) and "pkg" in names,
                "catalog names are not sorted/unique or pkg is missing")
        for item in entries:
            require(set(item) == {"name", "summary", "origin", "package", "version"},
                    "catalog entry fields mismatch")
            require(isinstance(item["summary"], str) and item["summary"],
                    "catalog summary missing")
            if item["name"] == "hello":
                require(installed and item == {
                    "name": "hello", "summary": "Print a greeting.",
                    "origin": "package", "package": "hello", "version": version},
                    "hello catalog identity mismatch")
            else:
                require(item["origin"] == ("control" if item["name"] in
                        ("help", "exit") else "builtin")
                        and item["package"] is None and item["version"] is None,
                        "builtin catalog identity mismatch")
        check(("hello" in names) == installed,
              "catalog hello presence matches installed=" + str(installed))
        return value

    try:
        tmpbase = os.environ.get("TMPDIR")
        require(tmpbase and Path(tmpbase).is_absolute() and Path(tmpbase).is_dir(),
                "set TMPDIR to an existing absolute user-owned scratch directory")
        source = ROOT / "build/main"
        identity = json.loads((ROOT / "packages/hello/pkg.json").read_text())
        version = identity["version"]
        require(identity["name"] == "hello" and identity["commands"] == ["hello"],
                "expected hello package")
        bundle = ROOT / "build/packages" / ("hello-" + version)
        require(source.is_file() and os.access(source, os.X_OK),
                "missing build/main; build before capturing")
        require(json.loads((bundle / "pkg.json").read_text()) == identity,
                "generated bundle identity differs from source")
        record["inputs"] = {"source_binary": str(source),
                            "source_sha256": digest(source), "package": str(bundle),
                            "package_binary_sha256": digest(bundle / "bin/hello")}
        fixture = tempfile.TemporaryDirectory(prefix="packaging-demo-", dir=tmpbase)
        space = Path(fixture.name)
        home, private_tmp = space / "home", space / "tmp"
        home.mkdir(mode=0o700)
        private_tmp.mkdir(mode=0o700)
        env = {"HOME": str(home), "TMPDIR": str(private_tmp),
               "LC_ALL": "C", "PATH": "/nonexistent"}
        record["namespace"] = {"root": str(space), "env": env}
        lines.append("Namespace: " + str(space))
        lines.append("Child environment (allowlist only): " + json.dumps(env, sort_keys=True))
        out, _ = run(source, ["pkg", "list"], env, space)
        check(out == "" and not (home / ".mysh").exists(), "initial package state empty")
        baseline = catalog(source, env, space, False)
        out, _ = run(source, ["pkg", "install", bundle], env, space)
        installed = home / ".mysh/pkgs" / ("hello-" + version)
        check(out == f"Installed hello {version}\n" and installed.is_dir(),
              "install published package in temporary HOME")
        for name in ["pkg.json", *identity["files"]]:
            require((installed / name).read_bytes() == (bundle / name).read_bytes(),
                    "installed file differs: " + name)
        check(True, "installed manifest and payload bytes match generated bundle")
        out, _ = run(source, ["pkg", "list"], env, space)
        check(out == f"hello {version}\n", "list reports installed version")
        out, _ = run(source, ["pkg", "info", "hello"], env, space)
        check(out == f"Name: hello\nVersion: {version}\n"
              "Description: " + identity["description"] + "\nCommands: hello\n",
              "info text matches source manifest")
        out, _ = run(source, ["help", "hello"], env, space)
        check("Usage: hello" in out and "--name" in out, "installed help exposes name option")
        out, _ = run(source, ["hello", "--name", "Week 4"], env, space)
        check(out == "Hello, Week 4!\n", "package execution preserves multiword argument")
        catalog(source, env, space, True)
        out, _ = run(source, ["pkg", "remove", "hello"], env, space)
        check(out == f"Removed hello {version}\n" and not installed.exists(),
              "remove erased installed package directory")
        out, _ = run(source, ["pkg", "list"], env, space)
        check(out == "", "list is empty after removal")
        check(catalog(source, env, space, False) == baseline,
              "catalog returns exactly to baseline after removal")
        out, err = run(source, ["hello"], env, space, expected=127)
        check(out == "" and err == "main: hello: unknown command\n",
              "removed command is unknown (127), without PATH fallback")

        release_id = json.loads((ROOT / "packages/aishell/release.json").read_text())
        release = ROOT / "build/releases" / (release_id["name"] + "-" + release_id["version"])
        if (release / "bin/aishell").is_file():
            relocated = space / "relocated-release"
            shutil.copytree(release, relocated)
            binary = relocated / "bin/aishell"
            record["relocated_release"] = {"source": str(release), "sha256": digest(binary)}
            check(os.access(binary, os.X_OK), "real full-shell release copied outside repository")
            out, _ = run(binary, ["pwd"], env, space)
            check(out == str(space) + "\n", "relocated shell executes from unrelated directory")
            out, _ = run(binary, ["help", "pkg"], env, space)
            check("Usage: pkg" in out, "relocated full shell provides package help")
            example = relocated / "examples" / ("hello-" + version)
            run(binary, ["pkg", "install", example], env, space)
            check(installed.is_dir(), "relocated release example installed in temporary HOME")
            out, _ = run(binary, ["hello", "--name", "relocated"], env, space)
            check(out == "Hello, relocated!\n", "relocated release executes included package")
            run(binary, ["pkg", "remove", "hello"], env, space)
            check(not installed.exists(), "relocated example removed")
        else:
            record["relocated_release"] = {"status": "SKIP", "reason": "no built release binary"}
            lines.append("SKIP: optional relocated release (no built binary)")
        check(digest(source) == record["inputs"]["source_sha256"]
              and digest(bundle / "bin/hello") == record["inputs"]["package_binary_sha256"],
              "source and package binaries unchanged")
        record["status"] = "PASS"
    except (OSError, ValueError, KeyError, TypeError, RuntimeError,
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
            lines.append("Fixture removed: " + str(record["cleanup_verified"]))
        lines.append("RESULT: " + record["status"])
        (HERE / "packaging-demo.json").write_text(json.dumps(record, indent=2) + "\n")
        (HERE / "packaging-demo.log").write_text("\n".join(lines) + "\n")
    print(f"{record['status']}: {len(record['events'])} commands; "
          f"{len(record['checks'])} checks; see demo/packaging-demo.log and .json")
    return 0 if record["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
