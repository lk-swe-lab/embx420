# Package lifecycle demo

Run from any working directory (Python standard library only):

```sh
python3 /home/name/Documents/420-EMBDX/A/demo/packaging-demo.py
```

`TMPDIR` must name an existing absolute scratch directory. This environment
already configures it. Else set it explicitly to your own scratch directory.
The script overwrites **only** its sibling `packaging-demo.log` and
`packaging-demo.json`; those files are actual execution evidence, not templates.

## Prerequisites and scope

Use trusted, already built `build/main` and `build/packages/hello-0.1.0`.
If absent, prepare them separately with `make build/main package-hello`.
The capture never builds, cleans, changes versions, or replaces immutable output.
It reads the current version from `packages/hello/pkg.json`.

## Exact source-build demonstration

Each line below is a separate invocation of the real `build/main` binary,
from a fresh temporary directory outside the repository:

1. `pkg list`, then `--commands-json`: assert empty package state and baseline catalog.
2. `pkg install <absolute generated hello directory>`: verify installed directory,
   manifest, and every payload file against the generated bundle.
3. `pkg list`: require the exact installed name/version.
4. `pkg info hello`: require actual textual name, version, description, and commands.
5. `help hello`: check installed usage and `--name` option.
6. `hello --name 'Week 4'`: require `Hello, Week 4!`.
7. `--commands-json`: parse JSON; check schema, sorted unique names, builtin/control
   origins, and exact hello package identity/version.
8. `pkg remove hello`, then `pkg list`: require absent directory and empty list.
9. `--commands-json`: require exact restoration of the baseline catalog.
10. `hello`: require empty stdout, unknown-command diagnostic, and exit **127**.

All other commands must return **0** with empty stderr. The JSON records exact
argv, stdout, stderr, status, expected status, cwd, allowlisted environment, and
10-second timeout per subprocess. The readable log retains the same command
outputs and checks. No shell expansion or host PATH fallback is involved.

## Optional relocated full shell

If the current `build/releases/aishell-<version>/bin/aishell` exists, copy the
complete release into the private fixture. Run its real `pwd`, `help pkg`,
`pkg install <relocated included example>`, `hello --name relocated`, and
`pkg remove hello`. Verify the copied shell executes outside the repository and
the installed example is removed. Otherwise record an explicit optional SKIP;
never invent release output. `make release-shell` can prepare it separately.
This copy demonstration does not prove compatibility with another OS/libc.

## Isolation and cleanup

A mode-0700 fixture under `TMPDIR` contains a fresh HOME, private TMPDIR, and any
relocated release. Children receive **only** HOME, TMPDIR, LC_ALL=C, and
PATH=/nonexistent: no inherited credentials or host environment dump. HOME state
lives exclusively in `<fixture>/home/.mysh/pkgs`; real user state is untouched.
The fixture is removed on success or handled failure and its absence verified.
Inputs' binary SHA-256 values are recorded and rechecked. Paths and captured
output remain literal; temporary paths naturally differ between runs.

## Verified capture and limits

The recorded successful run returned **PASS: 17 commands; 21 checks**, including
the optional real relocated release. Exit status was **0**, cleanup was verified,
and the intended unknown-command invocation returned **127**. Syntax was checked
with Python's AST parser without creating repository bytecode files.
An initial development run exposed that `pkg info` emits text, not JSON; the
runner was corrected and the complete real demonstration rerun successfully.
These compact artifacts demonstrate the lifecycle, not the full regression suite.

Package updates require a separately built new version: remove the old name,
then install the new directory. There is an availability gap; failed installation
does not restore the old version. No update or release-version change is faked
here. Full-shell updates use a separate versioned release and launcher switch;
the shell release is not a `pkg install` payload. Install only trusted binaries:
this is not a sandbox, download service, or signature system.
