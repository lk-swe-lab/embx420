#!/usr/bin/env bash
# Show real pipe output and process calls. Do not build or change packages.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
shell="$PWD/build/lean-shell"

if [[ ! -x "$shell" ]]; then
    printf 'Build first: make -C sidequests/lean-ai-shell test\n' >&2
    exit 1
fi
if [[ ${TMPDIR:-} != /* || ! -d "$TMPDIR" ]]; then
    printf 'TMPDIR must name an existing absolute directory.\n' >&2
    exit 1
fi
for tool in strace timeout python3 cmp; do
    command -v "$tool" >/dev/null || exit 1
done

fixture=$(mktemp -d "$TMPDIR/lean-pipe-demo.XXXXXX")
trap 'rm -rf -- "$fixture"' EXIT
cd -- "$fixture"
export HOME="$fixture/home" LC_ALL=C
mkdir sub "$HOME"
printf 'red\nblue\ngreen\n' > lines

run_case() {
    local label=$1 commands=$2 expected=$3 errors=${4:-}
    printf '\n%s\n' "$label"
    while IFS= read -r line; do
        printf 'lean> %s\n' "$line"
    done <<< "$commands"
    printf '%s\n' "$commands" |
        timeout 10 "$shell" > output 2> error
    cat output
    if [[ -s error ]]; then
        printf 'Expected diagnostic:\n'
        cat error
    fi
    printf '%s' "$expected" | cmp -s - output
    printf '%s' "$errors" | cmp -s - error
    printf 'CHECK: output and exit status match.\n'
    sleep 5
}

run_case 'Single command' $'pwd\nexit' "$fixture"$'\n'
run_case 'Two stages' $'cat lines | head -n 2\nexit' $'red\nblue\n'
run_case 'Three stages' $'cat lines | cat | head -n 1\nexit' $'red\n'
run_case 'Parent directory' $'cd sub\npwd\ncat ../lines | head -n 1\nexit' \
    "$fixture/sub"$'\nred\n'
run_case 'Syntax recovery' $'pwd || cat\npwd\nexit' "$fixture"$'\n' \
    $'lean: syntax error or input limit\n'

printf '\nProcess trace\n'
printf 'lean> cat lines | cat | head -n 1\nlean> exit\n'
printf 'cat lines | cat | head -n 1\nexit\n' |
    timeout 10 strace -f -e trace=process,pipe,pipe2,dup2 -o trace \
        "$shell" > output 2> error
printf 'red\n' | cmp -s - output
[[ ! -s error ]]
cat output trace
python3 -c '
from pathlib import Path
import re
text = Path("trace").read_text()
patterns = (r"\bpipe2?\(", r"\b(clone3?|fork|vfork)\(",
            r"\bdup2\(", r"\bwait4\(")
if not all(re.search(pattern, text) for pattern in patterns):
    raise SystemExit("Missing pipe, fork, routing, or wait evidence.")
if len(re.findall(r"\bexecve\(", text)) != 1:
    raise SystemExit("Expected one host exec; commands must use modules.")
'
printf 'CHECK: pipes, children, descriptor routing, and waiting observed.\n'
sleep 5
printf '\nPASS: pipe demo\n'
