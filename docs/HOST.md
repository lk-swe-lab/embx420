# Empty host and selected packages

`make` builds only `build/aishell`. It does not build or load command packages.
`make catalog` separately builds the local package collection.

## Human use

From `A/`, set `AISHELL_CATALOG` to the separate collection, then start:

```sh
export AISHELL_CATALOG="$PWD/build/catalog-0.3.1/catalog.json"
./build/aishell
```

Interactive startup lists available packages, not loaded commands.
Use these host controls:

- `:available` — list available packages.
- `:load NAME` — validate and load one local package.
- `:loaded` — list loaded packages.
- `:quit` — leave with the last command status.

For example: `:load hello`, then `hello --name learner`.
Load `help` before using `help`; load `cd` before using `cd`.
These controls are host operations, not command implementations.

## Agent use

```sh
./build/aishell --available-json
./build/aishell --load pwd --load help --loaded-json
./build/aishell --load pwd pwd
```

Use `--catalog FILE` or `AISHELL_CATALOG` for a separate local catalog.
The default path is `catalog/catalog.json` beside the executable.
No default catalog means an empty available list. No downloads occur.
JSON and scripted use have no startup banner. Use `--help` for host options.

## Loading is not installation

`:load` reads a bundle in place. It does not copy files or persist selection.
Each new process starts empty, even if HOME has installed packages.
To install a retained copy, first load `pkg`, then use `pkg install DIRECTORY`.
In this host, pkg activates only packages explicitly requested in this process.
Keep mapped bundle files unchanged until the process exits.

Catalog reading does not load libraries. Selecting a package does: native
constructors can run before ABI checks. Only select trusted code. No sandbox.
Schema 3 stores authoritative name, help, and usage, not invented option grammar.
Schema 2 keeps its full structured-metadata checks.

## Build, release, and compatibility

`make test-host` checks catalog selection and separate artifacts.
The catalog output/version can be set with `CATALOG_OUTPUT` and `CATALOG_VERSION`.
Use a new output directory and version when bundle bytes change. Never replace
mapped packages. The preliminary `build/catalog` is preserved, not overwritten.
`make release-shell` creates `build/host-releases/aishell-0.3.0/` with no packages.
Copy the catalog separately and pass its path. Host releases are immutable.

`build/main` is now a separate legacy eager-loading host, not an aishell alias.
It remains for old regression tests. The pipe sidequest retains its existing
module startup. Neither is the new empty-host distribution.
No PATH fallback, pipes, redirection, background jobs, or full job control is
added to the main host by this refactor.
