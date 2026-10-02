# Directory traversal

Prints directory/file names recursively; skips symlink descent.

From the project root:

```sh
make                            # Build
./build/main -h                  # Full help (also --help)
./build/main -r DIR              # Traverse DIR
./build/main -r DIR --mtime      # Add file/directory modification dates
./build/main -r DIR --mtime --utc   # UTC dates
./build/main -r DIR --mtime --local # Local dates (default)
./build/main -r DIR --histogram --utc # Hourly file counts after the listing
make test                       # Rebuild and run all CUnit tests
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

## Demo

Run `./demo.sh`: clean, all tests, clean, build, CLI demos, then lint.
Each command is printed before execution, with five-second pauses between.
The script stops if a command fails; it does not format source files.

Video: [2026-10-01-demo.mp4](demo/2026-10-01-demo.mp4) (no audio).
The original WebM recording is also in `demo/`.
