# Builtin shell and directory traversal

`main` runs eight builtin commands directly; no external-command fallback.
See [shell usage and limits](docs/commands/shell.md).

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
Optional standalone tools (run from the project root):

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

These are selected subsets, not full GNU replacements; also registered builtins.
`cp` is [planned, not implemented](docs/commands/cp.md).
Historical evidence is preserved in [command reports](docs/reports/README.md).

## Demo

Run `./demo.sh`: clean, all tests, clean, build, CLI demos, then lint.
Each command is printed before execution, with five-second pauses between.
The script stops if a command fails; it does not format source files.

Video: [2026-10-01-demo.mp4](demo/2026-10-01-demo.mp4) (no audio).
The original WebM recording is also in `demo/`.
