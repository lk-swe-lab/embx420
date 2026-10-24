# Shared-module shell and directory traversal

`build/aishell` (`build/main` compatibility alias) loads 22 standard `.so`
commands eagerly at startup (intentionally not lazy), plus optional native
packages. No command implementation is statically
linked into the host; unknown commands never fall back to PATH.
See [shell usage and limits](docs/commands/shell.md),
[current status](docs/STATUS.md) and [decision logs](ai_log/README.md).

Separate Week 03 learning project: [manual shell](sidequests/manual-shell/README.md)
and its [MP4 demo](sidequests/manual-shell/demo/manual-shell.mp4).

```sh
make
./build/main                   # Interactive: help, pwd, exit
./build/main --help             # Runtime module help
./build/main pwd                # One-shot command
./build/main head -n 2 FILE
./build/main help touch         # Module usage
```

Use quotes/backslash for filenames in the loop. No expansion, pipes or
redirection. Stdin cat/positive-count tail consume subsequent script input;
`tail -n 0` does not. Prefer file operands in scripts.

## Directory traversal

Prints directory/file names recursively; skips symlink descent.

From the project root:

```sh
make                            # Build
./build/main -h                  # Legacy help; --help lists modules
./build/main -r DIR              # Traverse DIR
./build/main -r DIR --mtime      # Add file/directory modification dates
./build/main -r DIR --mtime --utc   # UTC dates
./build/main -r DIR --mtime --local # Local dates (default)
./build/main -r DIR --histogram --utc # Hourly file counts after the listing
make test                       # Rebuild and run full regression
make build/main build/test_recursive && ./build/test_recursive  # Traversal test only
make lint                       # Analyze C code
make format-check               # Check formatting; no edits
make format                     # Format C files (80 columns)
make clean                      # Remove compiled program/tests
```

Time selectors affect dates and histograms; the last selector wins.
Date output uses a 30-character name column (truncated) and UTC/LOCAL labels.
Histogram: regular files only, all 24 hours, bars scaled to at most 30 chars.
Tests require CUnit; checks require clang-tidy and clang-format.

## Supported commands

Run `./build/main COMMAND ARGS...` or enter the command in the loop.
Seventeen standard modules also have optional standalone tools in `build/`.
`cd`, `pkg`, `help`, `exit` and `traverse` have no standalone wrapper. The separate
`build/hello` is an example/legacy test tool, not a standard shell command.
Every command also supports `aishell help COMMAND`.

| Command | Purpose | Help |
| --- | --- | --- |
| [cat](docs/commands/cat.md) | Concatenate files/stdin; optional numbering | `./build/cat --help` |
| [head](docs/commands/head.md) | First N lines; optional JSON | `./build/head --help` |
| [tail](docs/commands/tail.md) | Last N lines of finite input; optional JSON | `./build/tail --help` |
| [pwd](docs/commands/pwd.md) | Physical working directory; optional JSON | `./build/pwd --help` |
| [stat](docs/commands/stat.md) | Metadata for one object; optional JSON | `./build/stat --help` |
| [ls](docs/commands/ls.md) | One directory/file listing; optional JSON | `./build/ls --help` |
| [mkdir](docs/commands/mkdir.md) | Create directories, optionally parents/JSON | `./build/mkdir --help` |
| [touch](docs/commands/touch.md) | Update timestamps/create files; optional JSON | `./build/touch --help` |
| [clear](docs/commands/clear.md) | Clear visible ANSI terminal screen; redirected output is silent | `./build/main help clear` |
| [cp](docs/commands/cp.md) | mmap file/tree copying; force retry and optional JSON | `./build/main help cp` |
| [cd](docs/commands/cd.md) | Change physical cwd persistently in the loop | `./build/main help cd` |
| [rg](docs/commands/rg.md) | Search explicit files/trees; ERE, literals, context and JSON | `./build/main help rg` |
| [edit-replace-line](docs/commands/edit-replace-line.md) | Replace one existing line through staged editing; optional JSON | `./build/main help edit-replace-line` |
| [edit-insert-line](docs/commands/edit-insert-line.md) | Insert before a line or append through staged editing; optional JSON | `./build/main help edit-insert-line` |
| [edit-delete-line](docs/commands/edit-delete-line.md) | Delete one existing line through staged editing; optional JSON | `./build/main help edit-delete-line` |
| [edit-replace](docs/commands/edit-replace.md) | Global ERE/literal replacement through staged editing; optional JSON | `./build/main help edit-replace` |
| [mv](docs/commands/mv.md) | Same-filesystem rename; explicit force and optional JSON | `./build/main help mv` |
| [rmdir](docs/commands/rmdir.md) | Remove empty directories; adopted baseline command | `./build/main help rmdir` |
| [pkg](docs/commands/pkg.md) | Install/select/deactivate local native versions | `./build/main help pkg` |
| help | Live command help/catalog | `./build/main help help` |
| exit | Leave with previous status | `./build/main help exit` |
| traverse | Directory traversal and histograms | `./build/main help traverse` |

These are selected subsets, not full GNU replacements. The commands above are
registered in the current working tree; see [delivery status](docs/STATUS.md).
Individual reviews retain their original gate status and explicit limitations;
they are evidence snapshots, not live pending-commit instructions.
Historical evidence is preserved in [command reports](docs/reports/README.md).

## Packages and shell release

See the [package guide](docs/commands/pkg.md), [build setup](docs/TESTING.md)
and [redesign/current boundary](docs/reports/so-redesign.md).

```sh
make package-hello release-shell # prepare before starting the demo
./build/aishell
```

Inside that same running shell:

```text
hello
pkg install build/native-candidates/hello-0.2.0
hello --name learner
help hello
pkg remove hello
hello
pkg use hello 0.2.0
hello --name restored
exit
```

The first and post-removal hello are unknown (127). Install adds the module live;
remove hides it but retains immutable versions. Use selects a retained version.
Replacing an already published version requires restart; mapped bytes are never
replaced/unloaded. All callbacks run sequentially in the parent with no threads.
Native code is trusted, not sandboxed; constructors may run during install checks.
Install/use bind stored metadata to the actual module spec before selection.
A post-commit close failure returns 1, not success; inspect `pkg info` before retry.

The full release is `build/so-reviewed-releases/aishell-0.2.0/`, not a pkg payload.
Copy it completely, including `bin/modules/`, to a user-owned versioned directory
and explicitly select `bin/aishell`. Retain the old release for launcher rollback.
No download, dependency solving, signatures or shell self-update. Generated
outputs are immutable: identical bytes are a no-op; changed bytes need a new
version/output root. Earlier executable-package reports remain historical.

## Demo

The [package lifecycle demo](demo/PACKAGING.md) captures **11 real processes,
22 loop inputs and 27 checks** in [JSON](demo/packaging-demo.json) and a
[transcript](demo/packaging-demo.log), including a relocated complete release.
It never builds/cleans and uses isolated HOME with PATH=/nonexistent; 80 input
SHA-256 entries and fixture cleanup are recorded. Final independent review and
exact-tree acceptance remain pending.
