# pkg — local directory packages

Use `./build/main pkg ...`, or `pkg ...` in the shell loop:

```text
pkg -h | --help
pkg list
pkg info NAME
pkg install [--] DIRECTORY
pkg remove [--] NAME
```

Build the sample with `make package-hello`, then run:

```sh
./build/main pkg install build/packages/hello-0.1.0
./build/main hello --name learner
./build/main help hello
./build/main --commands-json
./build/main pkg remove hello
```

## Design and update

- `packages/hello/pkg.json` is the source record. The build generates a separate
  directory with a binary, structured metadata and generated help. Do not edit it.
- Install validates that directory and copies it to `$HOME/.mysh/pkgs`. It does
  not build or run payloads. HOME must be absolute. Builtin names cannot be replaced.
- Installed help/catalog use stored metadata. Execution uses the recorded
  absolute binary path, not PATH. Each lookup refreshes installed state.
- To update a command package: build a new version, remove the old package by
  name, then install the new directory. There is a gap; failed installation does
  not restore the old version. Reads and writes use a nonblocking package lock.
- Builds do not increase versions. Change the source version for a new release.
  Existing output is immutable; changed or unlisted files cause a build failure.
  Use a new output root for experiments. Never delete someone else's output.

## Shell release

`make release-shell` creates `build/releases/aishell-0.1.0/` with the complete
shell, current builtin help/catalog, and an explicitly optional hello demo.
`packages/aishell/release.json` alone defines the shell release identity/version.
Copy the complete release to a user-owned versioned directory and run
`bin/aishell`. Read its `README.md` and `SYSTEM.md`. No source is needed at runtime.
To update the shell, copy a separate new release, test it, then change your
launcher; retain the old directory for rollback. This release is not a `pkg`
payload. There is no shell self-update or automatic sudo installation.

## Limits and status

Only trusted local packages: no download, dependency resolution, archive import,
signature check or sandbox. Symlinks, hardlinks, unsafe types and unlisted files
are rejected. Do not repair corrupt state by guessing records. A failed removal
can leave partial state; inspect the diagnostic. Output failure cannot undo a
committed change. `0` is success, `1` is state/validation/I/O failure, and `2` is
syntax error. Unknown shell commands return `127`; launch failure is `126`.

Evidence: [backend](../reports/pkg-implementation.md),
[shell integration](../reports/package-shell.md), and
[release](../reports/shell-release.md). Builtin help is generated from existing
callbacks; it is not a fabricated structured argument schema.
