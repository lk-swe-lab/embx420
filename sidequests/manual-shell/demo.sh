#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
mkdir -p demo
command -v strace >/dev/null
run() {
    printf '\n$ %s\n' "$*"
    "$@"
    sleep 5
}
run make -C ../.. build/pwd build/cat build/head
run make clean
for mode in manual strtok; do
    printf '\n$ make TOKENIZER=%s test > demo/tests-%s.log 2>&1\n' "$mode" "$mode"
    if ! make "TOKENIZER=$mode" test > "demo/tests-$mode.log" 2>&1; then
        cat "demo/tests-$mode.log"
        exit 1
    fi
    python3 -c 'import pathlib,sys; print("\n".join(pathlib.Path(sys.argv[1]).read_text().splitlines()[-4:]))' "demo/tests-$mode.log"
    sleep 5
done
run make clean
run make TOKENIZER=manual
printf '\n$ ./Unix_Shell (one foreground session; %% lines are inputs)\n'
step() {
    printf '\n%% %s\n' "$1" >&2
    printf '%s\n' "$1"
    sleep 5
}
{
    step 'pwd --help'
    step 'cat --help'
    step 'head --help'
    step pwd
    step 'cd fixtures'
    step pwd
    step 'cat lines.txt'
    step 'head -n 2 lines.txt'
    step 'cat missing.txt'
    step pwd
    step exit
} | ./Unix_Shell
printf '\n$ printf "pwd\\nexit\\n" | strace -f -e trace=process ./Unix_Shell\n'
printf 'Parent clone/fork -> child exec of repository build/pwd -> parent wait4.\n'
printf 'pwd\nexit\n' | strace -f -e trace=process ./Unix_Shell
sleep 5
printf '\nDemo complete. Both tokenizer suites passed; missing-file recovery shown.\n'
sleep 5
