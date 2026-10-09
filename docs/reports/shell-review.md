# Independent main shell review

Reviewed October 8, 2026, against HEAD
`b0db03c99bfd2e1a4add175448717cdf6e8e2718` and the working sources recorded in
`/home/name/.hermes/cache/scratch/shell-review/manifest.json`.

## Verdict

The eight-command builtin shell passes independent working-tree, isolated
HEAD-plus-dependency regression/build/lint, and ASan/UBSan/leak checks. No shell
logic, ownership, partial-execution, or external-fallback blocker was found.
**Conditional readiness:** parent must disposition the legacy diagnostic
compatibility finding below before claiming unchanged legacy behavior. No source,
test, or Makefile fix was made by this reviewer; any fix belongs to another worker.
The final staged/index tree still needs the parent's independent extraction/check.

## Findings

1. **Medium — undocumented legacy diagnostic change from symbol interposition.**
   `src/main.c:58` still calls `getopt_long`, but the newly linked static
   argtable3 object defines `getopt`, `getopt_long`, and `getopt_long_only`.
   The actual `build/main` exports these definitions instead of using libc's
   parser. A separately compiled HEAD main produces, for `-r` without an operand:
   `PROGRAM: option requires an argument -- 'r'`; integrated main produces
   `main: option requires an argument -- r`. Unknown/invalid options also have
   different diagnostic formatting. Both statuses remain 2. The first strict
   baseline comparison failed on this real difference; it was retained as a
   finding, not silently treated as exact parity.
   Fifteen initial cases and 46 expanded cases (with/without POSIXLY_CORRECT)
   had equal exit status/stdout; valid traversal/date/histogram output and `-h`
   match the baseline. No status/output regression was observed in those cases.
   Recommend a regression test and either isolating/restoring the legacy parser
   or explicitly accepting/documenting the diagnostic difference. Existing shell
   tests check `-r` failure status, not the original stderr, so do not detect it.
   Evidence: `focused-results.json`, `legacy-comparison.json`,
   `argtable-symbols.log`, `main-symbols.log` under the scratch prefix below.

2. **Low — qualify the stdin documentation's tail generalization.**
   `src/cmd_tail.c:122` returns before reading when count is zero. Consequently
   `tail -n 0\npwd\n` executes pwd. This preserves the existing module's behavior;
   no tail source change is requested. `docs/commands/shell.md:33–36` and the
   integration report should say that successful positive-count tail/default cat
   consume remaining input, while zero-count tail does not. Errors/help/file
   operands also need not consume stdin. The focused sanitizer probe verified
   this exact zero-count case. Prefer a shell regression for it.

3. **Nonblocking API/coverage limits.** `shell_dispatch` expects a valid positive
   argc, non-NULL argv entries and stop pointer; internal callers satisfy this,
   but its public header does not state the preconditions. The void usage callback
   cannot report silent allocation failure (e.g. pwd usage); output errors are
   detected, not arbitrary callback OOM. Existing head/tail run help repeats
   `[-n N]` according to argc, whereas help callbacks render one; tests acknowledge
   this inherited difference rather than editing the modules.

## Reviewed behavior and mechanisms

- Actual registry entries, in order: cat, head, tail, pwd, stat, ls, mkdir, touch.
  Pointers reference the real module specs, not alternate command implementations.
  Specs are borrowed, single-threaded, process-lifetime. Validation, exact-name
  lookup, duplicate rejection, identical re-registration, NULL enumeration and
  capacity 32 are exercised. Partial initialization failure exits main; no
  transactional/dynamic plugin registry is claimed.
- Both one-shot argv and loop dispatch call the same run callbacks. Controls
  help/exit, arity, 127 unknown status, module status propagation, syntax status 2,
  blank-line status preservation, EOF and error continuation pass. Help flushes
  around descriptor-writing modules. Legacy routing occurs before module parsing.
- Lexer compacts only within the explicit initialized byte extent. Quotes, empty
  arguments, adjacent fragments and escapes pass; NUL is drained through the
  line, and unsupported operators/unclosed quotes/dangling escapes never execute
  a partially parsed mkdir. Generated filenames, long lines and repeated commands
  exercise buffer/token growth.
- Reader borrows stdin and reads one byte per request, retries EINTR, and does not
  steal following builtin data. Finite head resumes parsing; normal cat/tail
  consume subsequent apparent commands as data. Allocated argv and line storage
  are freed on success, syntax, resource and read-error paths. Module tests and
  an extra scratch-only fault harness cover allocation failure both at first
  growth and at second growth after an existing line buffer, with leak detection.
- No fork/exec/system/popen/external fallback call exists in the shell route.
  Absolute executable names, substitution-looking text and unknown commands are
  rejected/literal as documented. An executable PATH sentinel was not launched.
  This is not a sandbox: registered filesystem commands have their normal effects.

## Independently executed evidence

Scratch prefix: `/home/name/.hermes/cache/scratch/shell-review/`.
`results.json` records exact commands, cwd, status and raw log paths.
`run_review.py`, `focused.py`, and `legacy_compare.py` reproduce the checks.

| Actual command/check | Result | Evidence under prefix |
| --- | --- | --- |
| Working `make -B test` | 0; full regression, 17 shell CLI tests plus module fault checks | `working-test.log` |
| Working `make all` | 0 | `working-all.log` |
| Working `make test-shell` | 0; 17 CLI tests plus module checks | `working-test-shell.log` |
| Working `make lint` | 0; actual entire C source/test tree | `working-lint.log` |
| Scoped clang-format dry-run/Werror: main, shell.[ch], registry.[ch], test_shell_module.c | 0 | `scoped-format.log` |
| `git diff --check -- Makefile src/main.c` | 0 | `diff-check.log` |
| Working `make format-check` | 2; six unchanged HEAD files only | `whole-format.log`, `focused-results.json` |
| Isolated `make -B test`, `make all`, `make lint` | all 0 | `fresh-test.log`, `fresh-all.log`, `fresh-lint.log` |
| Isolated sanitizer main/module build | 0 | `sanitizer-build.log` |
| Sanitizer shell CLI suite | 0; 17 tests | `sanitizer-cli.log` |
| Sanitizer direct module suite | 0; no ASan/UBSan/LSan findings | `sanitizer-module.log` |
| Extra sanitizer second-growth OOM harness | 0; no sanitizer findings | `extra-build.log`, `extra-module.log`, `test_shell_extra.c` |
| Additional shell probes / legacy comparisons | 24 initial records; 46 expanded comparisons; status/stdout match all legacy cases | `focused-results.json`, `legacy-comparison.json` |

Sanitizer flags were `-fsanitize=address,undefined -fno-omit-frame-pointer
-fno-pie -no-pie`; execution used `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1`
and `UBSAN_OPTIONS=halt_on_error=1`. Installed argtable3 is not instrumented.
Third-party parser-internal OOM, huge integer-limit lines, exhaustive fuzzing,
multithreaded registry access and changed signal dispositions are not verified.
Signal policy including SIGPIPE remains inherited. This is a minimal builtin
shell, not a complete POSIX shell or LLM client.

The six format failures are src/cmd_spec.h, src/traverse.c, src/traverse.h,
tests/test_help.c, tests/test_output.h, tests/test_recursive.c; byte comparison
against HEAD proves all six unchanged. All scoped C lines meet the 80-character
rule. `final-audit.json` records source hashes still matching the tested snapshot,
no external-execution call matches, and an empty cached diff.

## Exact parent dependency scope

The isolated tree was made from `git archive HEAD` plus **exactly these 34 paths**,
using the complete current Makefile, not merely its already-applied shell patch.
Only build/argtable3 and build/cunit dependency directories were linked from the
working tree; no unrelated source, data/wiki/report or untracked module was linked.
The normal fresh regression/build/lint passed before sanitizer rebuilds.
`scope.txt` is the machine-readable allowlist; `manifest.json` records its hashes.
This establishes build/test dependency closure, not final-index verification.

```text
Makefile
src/main.c
src/shell.c
src/shell.h
src/registry.c
src/registry.h
tests/test_shell.py
tests/test_shell_module.c
src/cmd_cat.c
src/cmd_cat.h
src/cat_main.c
tests/test_cat.c
tests/test_cat_module.c
src/cmd_head.c
src/cmd_head.h
src/head_main.c
tests/test_head.py
tests/test_head_module.c
src/cmd_tail.c
src/cmd_tail.h
src/tail_main.c
tests/test_tail.py
tests/test_tail_module.c
src/cmd_pwd.c
src/cmd_pwd.h
src/pwd_main.c
tests/test_pwd.py
tests/test_pwd_module.c
src/cmd_touch.c
src/cmd_touch.h
src/touch_main.c
tests/test_touch.py
tests/test_touch_module.c
tests/test_lines_random.py
```

cat/head/tail/pwd are currently untracked but required by registry/main and
Makefile all/test-shell/test. Do not omit their sources, headers, wrappers or
regression tests. Existing HEAD supplies cmd_lines, cmd_spec, stat, ls, mkdir,
traverse and their tests/fixtures; no new versions of those files are needed.
The full Makefile delta from HEAD integrates these older modules as well as
shell/touch; applying only Makefile.shell.patch to HEAD would not establish this
closure, and that patch must not be reapplied to the working Makefile.

For documented shell/touch delivery, add the intentionally selected documentation
separately: docs/commands/shell.md, docs/reports/shell-integration.md, this report,
docs/commands/touch.md and whichever independent touch evidence the parent selects.
Older cat/head/tail/pwd usage docs are useful but not build dependencies. Do not
stage all reports/logs or unrelated dirty files blindly. After any fix, refresh
hashes and reverify the exact proposed staged tree before commit.

## Reviewer changes only

Created this report and scratch evidence/harnesses. Appended the verified current
session ID `20261008_192134_00b5e0` / title “Subagent: Independently review and
verify main shell…” to docs/sessions.txt under the authorized startup procedure.
Updated the current-profile command-development skill with the static-parser
symbol-interposition verification lesson. No source/test/Makefile edits,
staging, commits, AI_LOG/wiki/push/README/FILES changes. Unrelated dirty work was
preserved. Parent owns disposition/fixes, final staged-tree checks and publication.
