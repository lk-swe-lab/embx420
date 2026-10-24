#!/usr/bin/env bash
# Run the video demo from any directory; stop if a command fails.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

run() {
    printf '\n$ %s\n' "$*"
    "$@"
    sleep 5
}

run make clean
run make test
run make clean
run make

run ./build/main -h
run ./build/main --help
run ./build/main -r tests/fixtures/tree
run ./build/main --recursive tests/fixtures/tree
run ./build/main -r tests/fixtures/tree --mtime
run ./build/main -r tests/fixtures/tree --mtime --utc
run ./build/main -r tests/fixtures/tree --mtime --local
run ./build/main -r tests/fixtures/long_names --mtime --utc
run ./build/main -r tests/fixtures/tree --histogram --utc
run ./build/main -r tests/fixtures/tree --histogram --local
run ./build/main -r tests/fixtures/tree --mtime --histogram --local
run ./build/main -r tests/fixtures/tree --mtime --utc --local
run ./build/main -r tests/fixtures/tree --mtime --local --utc

run make lint
printf '\nDemo completed successfully.\n'
