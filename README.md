# Builtin shell and directory traversal

`main` runs 19 registered builtins and installed package commands. It does not
search PATH for unknown commands.
See [shell usage and limits](docs/commands/shell.md),
[current status](docs/STATUS.md) and [decision logs](ai_log/README.md).

Separate Week 03 learning project: [manual shell](sidequests/manual-shell/README.md)
and its [MP4 demo](sidequests/manual-shell/demo/manual-shell.mp4).

```sh
make
./build/main                   # Interactive: help, pwd, exit
./build/main --help             # Builtin shell help
./build/main pwd                # One-shot command
./build/main head -n 2 FILE
./build/main help touch         # Builtin usage
```

Use quotes/backslash for filenames in the loop. No expansion, pipes or
redirection. Stdin cat/positive-count tail consume subsequent script input;
`tail -n 0` does not. Prefer file operands in scripts.

## Directory traversal

Prints directory/file names recursively; skips symlink descent.

From the project root:

```sh
make                            # Build
./build/main -h                  # Legacy help; --help lists builtins
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
Seventeen builtins also have optional standalone tools in `build/`; `cd` and
`pkg` have no standalone wrapper. The separate `build/hello` is package-only,
not a builtin. Existing standalone help
examples below remain valid; every command also supports `main help COMMAND`.

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
| [pkg](docs/commands/pkg.md) | List, inspect, install and remove local packages | `./build/main help pkg` |

These are selected subsets, not full GNU replacements. The commands above are
registered in the current working tree; see [delivery status](docs/STATUS.md).
Individual reviews retain their original gate status and explicit limitations;
they are evidence snapshots, not live pending-commit instructions.
Historical evidence is preserved in [command reports](docs/reports/README.md).

## Packages and shell release

See the brief [package guide](docs/commands/pkg.md) and separate
[shell release guide and evidence](docs/reports/shell-release.md).

```sh
make package-hello
./build/main pkg install build/packages/hello-0.1.0
./build/main hello --name learner
./build/main --commands-json
./build/main pkg remove hello
make release-shell              # build/releases/aishell-0.1.0/
```

The shell release is not a `pkg install` payload. Install it in a user-owned,
versioned directory and select its `bin/aishell` launcher explicitly. Keep the
old release and installed packages when switching launchers. Package updates
use remove then install; no automatic rollback or shell self-update is provided.
Generated targets are immutable: identical builds are no-ops; changed content
needs another output root or an explicit manual move of the known old artifact.

## Demo

The current [package lifecycle demo](demo/PACKAGING.md) records 17 real commands
and 21 checks in [JSON](demo/packaging-demo.json) and a
[transcript](demo/packaging-demo.log), including a relocated release.
