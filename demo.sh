#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if (( $# > 1 )); then
    printf 'Usage: %s [PROJECT_ROOT]\n' "$0" >&2
    exit 2
fi
if (( $# == 1 )); then
    cd -- "$1"
fi
if [[ ! -x ./build/main ]]; then
    printf 'Build first with make all (requires installed dependencies).\n' >&2
    exit 1
fi
step=0
run() {
    step=$((step + 1))
    printf '\033[2J\033[HCommand %d/5 | committed CLI | five-second hold\n\n$' "$step"
    printf ' %q' "$@"
    printf '\n\n'
    "$@"
    printf '\n[exit 0]\n'
    sleep 5
}
run ./build/main pwd
run ./build/main ls tests/fixtures/tree
run ./build/main rg -n cmd_pwd_spec src/cmd_pwd.h
run ./build/main printf '%s: %04d\n' commands 5
run ./build/main date -u
printf '\nDemo complete: 5 commands succeeded.\n'
