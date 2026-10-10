# Builtin shell and directory traversal

`main` runs 22 registered builtin commands directly; no external-command fallback.
See [shell usage and limits](docs/commands/shell.md),
[current status](docs/STATUS.md) and [decision log](AI_LOG.md).

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
Twenty-one commands also have optional standalone tools in `build/`; `cd` is
in-process only, with no standalone executable. Existing standalone help
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
| [env](docs/commands/env.md) | Read-only sorted environment listing; optional JSON | `./build/main help env` |
| [echo](docs/commands/echo.md) | Print literal operands separated by spaces and a newline | `./build/main help echo` |
| [date](docs/commands/date.md) | Show numeric current local time or UTC | `./build/main help date` |
| [printf](docs/commands/printf.md) | Bounded typed integer/string/byte formatting and escapes | `./build/main help printf` |
| [sleep](docs/commands/sleep.md) | Delay for bounded decimal seconds; handled-signal retries | `./build/main help sleep` |

These are selected subsets, not full GNU replacements. The commands above are
committed and registered at the [current checkpoint](docs/STATUS.md).
Individual reviews retain their original gate status and explicit limitations;
they are evidence snapshots, not live pending-commit instructions.
Historical evidence is preserved in [command reports](docs/reports/README.md).

## Demo

Run `./demo.sh` after `make all`. The [verified recording](demo/commands-demo.mpg)
shows five real commands with five-second holds: `pwd`, `ls`, `rg`, `printf`,
`date -u`. It is a 25.08-second MPEG program stream with MPEG-2 video.
See [execution and media evidence](demo/README.md). Historical recordings remain
unchanged; commit and publication are separate gates.
