# main shell integration — ready for review

Current readiness: [accepted dispositions](shell-disposition.md).

## Delivered

`build/main` now dispatches cat/head/tail/pwd/stat/ls/mkdir/touch through their
actual `cmd_spec_t` definitions, lists them with `--help`, and runs stdin
commands with no arguments. Shell controls: `help [COMMAND]`, `exit`. Quoting,
escapes, strict syntax rejection, unknown-command status 127, last-command
status, EOF, and inherited stdin ownership are implemented. Legacy `-r` flags
and byte-exact `-h` help still pass their existing tests.

Files changed/created:
- `src/main.c`: legacy routing plus shell entry point (legacy body preserved).
- `src/registry.c`, `src/registry.h`: checked, idempotent, borrowed-spec registry;
  actual eight-module registration, lookup, enumeration; fixed capacity 32.
- `src/shell.c`, `src/shell.h`: dispatch, discovery help, byte-at-a-time reader,
  bounded in-place lexer, owned argv/line cleanup, terminal-only stderr prompt.
- `tests/test_shell.py`, `tests/test_shell_module.c`: CLI/parity/generated
  quoting/PTY tests and direct registry/ownership/EINTR/allocation-fault tests.
- `Makefile`: narrowly added shell linkage, module/test target, test prerequisite,
  and clean entry after explicit mid-turn permission; preserved touch integration.
- `docs/commands/shell.md`: selected scope, syntax/status/stdin contract.
- This report. No other command source/tests/docs, README/FILES, stage, commit,
  push, wiki, or AI log changes by this worker.

## Evidence

Scratch prefix: `/home/name/.hermes/cache/scratch/shell-integration/`.
Actual working-tree verification (not a HEAD-main substitute):

| Command | Observed result | Log under scratch prefix |
| --- | --- | --- |
| `make -B test` | exit 0; 17 shell CLI tests, shell module checks, complete existing suite including touch pass | `working-make-test.txt` |
| `make all` | exit 0 | `working-make-all.txt` |
| `make lint` | exit 0, entire current C tree | `working-make-lint.txt` |
| `clang-format --dry-run --Werror src/main.c src/shell.c src/shell.h src/registry.c src/registry.h tests/test_shell_module.c` | exit 0 | `format-owned.txt` |
| `make format-check` | exit 2, existing untouched files listed below | `format-full.txt` |
| `git diff --check -- Makefile src/main.c` | exit 0 | terminal output |
| `python3 tests/test_shell.py --program /usr/bin/true --standalone-dir build` | intended exit 1; 17 tests, 32 failed assertions/subtests | `negative-control.txt` |

Incremental behavioral RED → GREEN evidence:
1. Original compiled main: dispatch tests fail on absent pwd output, old help,
   unknown status, module parity; legacy test passes (`red-dispatch.txt`).
   Implemented one-shot registry/control dispatch: five tests pass
   (`green-dispatch.txt`).
2. Added loop tests: four new loop tests fail with empty output/status
   (`red-loop.txt`). Implemented byte-reader/basic tokenizer/loop: nine tests
   pass (`green-loop.txt`).
3. Added quoting/syntax tests: quoted cat operands split wrongly; malformed
   mkdir lines execute rather than reject (`red-quotes.txt`). Implemented
   complete lexer: passing suite (`green-all.txt`, earlier seven-module snapshot).
4. After touch completion, added eighth-command discovery/parity/effect tests:
   actual missing registration produces unknown-command/help failures
   (`red-touch.txt`); registration/build integration produces final 17-test pass
   (`working-make-test.txt`).

Two test-setup corrections were made openly: tail's actual summary includes
“finite”; repeated-module expected fixtures now precreate the directory to avoid
changing the ls fixture between expected generation and batch execution.

Sanitizers on final eight-command sources:

```sh
EXTRA_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie' \
  sh /home/name/.hermes/cache/scratch/shell-integration/build-main.sh
ASAN_OPTIONS=detect_leaks=1 python3 tests/test_shell.py \
  --program /home/name/.hermes/cache/scratch/shell-integration/main \
  --standalone-dir build
make -C /home/name/.hermes/cache/scratch/shell-integration/integration-tree \
  -B build/test_shell_module \
  CFLAGS='-std=c17 -Wall -Wextra -Wpedantic -g3 -O0 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie'
ASAN_OPTIONS=detect_leaks=1 \
  /home/name/.hermes/cache/scratch/shell-integration/integration-tree/build/test_shell_module
```

Both exit 0; 17 CLI tests pass, module checks pass, no ASan/UBSan/LSan findings.
Logs: `sanitizer-cli.txt`, `sanitizer-module-build.txt`, `sanitizer-module.txt`.
Installed argtable3 itself is not sanitizer-instrumented. Scratch main was then
rebuilt normally; working `build/main` is the real normal Makefile build.

Coverage includes exact one-shot and batch streams/status, eight-wrapper parity,
help callbacks, repeated success/error/help, terminal prompt, empty stdin, final
line without newline, 17,000-byte whitespace line, 600 cat operands, 100 repeated
cat calls, 180 generated quoted/escaped filenames with binary output, syntax
rejection without mkdir side effects, literal expansion characters, no external
fallback, /dev/full and EISDIR, registry duplicates/invalid/full capacity,
reader EINTR, line/argv/empty-line allocation failures, and borrowed stdin open.

## Limits / existing issues

- This is a minimal builtin shell, not a full POSIX shell or LLM integration.
  No external execution, expansion, substitution, pipelines/redirection/jobs,
  multiline input, cd, or package loading. Course-index evidence and scope are
  in the preserved selection plan below; no indexed pipeline/redirection
  requirement was found in the performed searches.
- Reader does not read ahead. Builtins using stdin consume subsequent script
  bytes as data: head returns after its prefix; successful stdin cat and
  positive-count tail consume to EOF. `tail -n 0` reads nothing, so the next
  command executes. Help/errors/file operands need not read stdin; prefer files.
- Existing head/tail run help renders repeated `[-n N]` by argc; their
  print_usage callbacks render one. Tests assert run/standalone parity and the
  exact known callback difference; their implementations were not edited.
- The existing void print_usage API cannot report silent allocation failure
  reliably; shell detects output-stream errors but does not invent a callback
  success signal. Full OOM inside argtable3 and impractically huge integer-limit
  inputs are not claimed verified. Signal dispositions (including SIGPIPE)
  remain inherited; no sandbox/security boundary is claimed.
- Initial analyzer warnings were resolved by carrying the reader's explicit
  byte length into a bounded lexer rather than inferring extent with strlen.
  Initial whole-tree lint include-path failure was fixed using the project's
  `../src/` test-header convention. Final whole-tree lint passes.
- Full format-check failures are in untouched `src/cmd_spec.h`,
  `src/traverse.c`, `src/traverse.h`, `tests/test_help.c`,
  `tests/test_output.h`, `tests/test_recursive.c`; no unrelated reformatting.
- Independent review passed conditionally; the parent accepted static argtable3
  getopt interposition: malformed legacy options have different stderr wording,
  prefix and punctuation from libc. Status 2/empty stdout and valid legacy
  help/stdout remain verified; byte-exact legacy error parity is not claimed.
  The six unchanged HEAD format failures above are accepted preexisting,
  nonblocking scope. Final index verification remains the parent's gate.
  All task execution/build acceptance checks above are exercised, not inferred.

## Parent handoff

The shell-only Makefile delta (against the touch-completed working snapshot) is
archived at `scratch-prefix/Makefile.shell.patch` and is **already applied** to
working Makefile. Do not apply it a second time. SHA-256:
`4188b1c5995cf90df68aa528eba31392e75059ac5e81889ee7c36507048f6ab9`.
`build-main.sh` remains an exact scratch source/dependency build recipe.

Suggested README text (parent owns README):

```text
main is the builtin shell: ./build/main COMMAND ARGS..., or ./build/main
for stdin commands. ./build/main --help lists commands; help COMMAND shows
module usage. Quotes/backslash group filenames; expansion, pipelines,
redirection and external commands are not supported. Stdin-reading commands
consume subsequent script bytes as data; prefer file operands in scripts.
Legacy traversal remains ./build/main -r DIR ...; -h shows legacy help.
```

Suggested FILES entries (parent owns FILES):
- `src/shell.[ch]`: minimal builtin shell dispatch, lexer and stdin loop.
- `src/registry.[ch]`: actual command registration, lookup and discovery.
- `tests/test_shell.py`, `tests/test_shell_module.c`: CLI/parity/ownership/fault
  integration coverage; `make test-shell` runs both, also included in `make test`.
- `docs/commands/shell.md`: shell contract; this report: execution evidence.

Verified session lookup: `20261008_185844_7177a6`, title
“Subagent: Implement main AI-shell integration source and…”. Registration is
left to the parent because coordination files were explicitly parent-owned;
this worker did not change docs/sessions.txt. Updated the current-profile
command-development skill with the reusable no-read-ahead/length/flush lesson.

## Preserved original selection and verification plan

The original usage-plan text follows verbatim as historical planning evidence.
Its blanket tail/parser wording is superseded by the limits above and the
disposition report, not a current compatibility claim.

```text
# Minimal main shell

Authorized autonomous selection: registry dispatch and command loop, not a POSIX
shell or an LLM client. Course evidence: indexed Command Anatomy paragraphs
55–56, 244–247; FileIO slide 17; Introduction page 13. A broader indexed search
for pipeline/redirection/REPL/interactive returned no pipeline or redirection
requirement. This does not claim the full course roadmap is complete.

## Contract selected before tests

- `main COMMAND ARGS...` calls the actual command specification's run callback.
  Register cat, head, tail, pwd, stat, ls, mkdir, touch; no duplicate
  command parsers.
- `main --help`, `main help`: list registry names/summaries and shell syntax.
  `help COMMAND` calls its print_usage callback. Invalid help arity: status 2.
- No arguments: read stdin one byte at a time through newline/EOF, with an owned
  growable line and argv pointer array. Never buffer ahead of builtin input.
  Prompt `main> ` on stderr only when stdin is a terminal.
- Loop continues after command/syntax errors. Return last nonblank command status
  at EOF; initially 0. `exit` (no operands) stops and preserves that status.
  One-shot `main exit` succeeds. Invalid exit arity: status 2, loop continues.
- Unknown command: `main: NAME: unknown command` on stderr, status 127. Module
  statuses propagate unchanged; shell input/output/resource failures: status 1.
- Whitespace separates arguments. Single/double quotes group text and can produce
  empty operands; adjacent quoted/unquoted fragments concatenate. Backslash
  escapes the next byte outside single quotes, including inside double quotes.
  No multiline quotes or continuation. Unclosed quote/dangling escape/NUL byte
  are syntax errors (2), never partial execution.
- Unquoted `| & ; < >` are rejected (2). Quoted/escaped versions are ordinary
  filename bytes. `$`, backticks, glob characters, and `#` are literal text:
  no expansion, substitution, comments, pipelines, redirects, external fallback,
  background jobs, environment assignment, cd, or package loading.
- Commands borrowing stdin consume subsequent script bytes as data. `head -n 1`
  consumes exactly one following data line, then parsing resumes. `cat` and
  `tail` consume through EOF, so subsequent apparent commands are data, not
  executed. Prefer file operands in command scripts. Interactive cat/tail need
  terminal EOF, not a shell-specific data delimiter.
- Flush stdout around callback dispatch to preserve help/raw-descriptor output
  order. Borrow inherited descriptors; free line/argv after each invocation.
- First argument beginning `-` routes legacy traversal options, except standalone
  `--help` is the new shell listing. `-h` retains byte-exact old traversal help;
  `-r`, `--recursive`, date/time/histogram flags keep their old parser/behavior.

## Boundaries and verification plan

Incremental slices: one-shot registry/help/errors; loop/status/stdin ownership;
quoting/syntax rejection. Test exact streams/status, all eight wrapper parity,
repeated help/success/error, EOF without newline, blank lines, syntax errors with
no side effects, no external fallback, stdout/input failures, legacy traversal.
Use negative controls and sanitizer/resource checks when tools permit. Build
main in scratch first; after touch finished, explicitly authorized narrow
working Makefile integration includes all eight modules. Detailed executed
evidence: `docs/reports/shell-integration.md`.

API reference: POSIX read (Issue 8),
https://pubs.opengroup.org/onlinepubs/9799919799/functions/read.html.
This shell deliberately has simpler quoting than POSIX shell-language semantics.
```
