# Shell review dispositions — ready for parent staging

October 8, 2026; working tree based on HEAD
`b0db03c99bfd2e1a4add175448717cdf6e8e2718`.
[Independent review](shell-review.md) and [implementation evidence](shell-integration.md).

## Accepted differences and limits

- **Legacy parser diagnostics accepted:** static argtable3 defines/interposes
  getopt/getopt_long. Do not restructure the dependency. Valid legacy traversal,
  dates/histogram, exit statuses/stdout and exact `-h` help remain verified;
  standalone `--help` deliberately lists builtins. Invalid-option stderr is
  **not byte-exact libc compatibility**: prefix, quotation and wording differ.
  The new regression ignores only the program prefix and requires status 2,
  empty stdout and these exact diagnostic bodies:

  | Arguments | Accepted stderr body (followed by newline) |
  | --- | --- |
  | `-r` | `option requires an argument -- r` |
  | `--recursive` | ``option `--recursive' requires an argument`` |
  | `-Z` | `invalid option -- Z` |
  | `--bogus` | ``unrecognized option `--bogus'`` |
  | `--mtime=x` | ``option `--mtime' doesn't allow an argument`` |

  Libc HEAD main instead quotes short option letters with apostrophes and uses
  apostrophe pairs instead of backtick/apostrophe pairs for these long options.
- **Stdin wording corrected:** successful stdin cat and positive-count/default
  tail consume remaining script input through EOF. `tail -n 0` reads nothing;
  the following `pwd` executes. Help/errors/file operands need not consume stdin.
  This worker made no tail implementation edit. Public shell_dispatch preconditions now
  document valid positive argc, non-NULL argv entries/terminator, stop and registry.
- **Six preexisting format failures accepted as nonblocking scope:**
  `src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`, `tests/test_help.c`,
  `tests/test_output.h`, `tests/test_recursive.c`. All six were compared directly
  with HEAD and are byte-identical; no unrelated reformatting.
- Existing head/tail help cardinality and void usage-callback OOM limitations,
  inherited signal policy, parser-internal OOM, huge inputs and exhaustive
  fuzzing remain the independently reviewed limits, not newly verified claims.

## Regression evidence (characterization, not a production bug fix)

Both newly asserted behaviors already worked. No false pre-fix RED is claimed.
A real scratch-only stdin negative control replaces `tail -n 0` with `tail -n 1`:
its test fails with literal `pwd\n` rather than the working-directory path.
The unchanged production binary passes. A freshly compiled HEAD/libc main fails
all five diagnostic-body subtests for the intended punctuation differences;
integrated main passes. These observed negative-control RED → unchanged-GREEN
runs demonstrate sensitivity, not an implementation change. Existing exact
legacy help/CUnit and valid traversal/stdout tests were preserved and rerun.

Evidence prefix: `/home/name/.hermes/cache/scratch/shell-disposition/`.
`run_checks.py` records actual command arrays/statuses in `results.json`, with
raw, separate `.stdout`/`.stderr` files (no synthesized/truncated results).
`focused.py` runs either new named test from `tests/test_shell.py`.

| Actual command/check | Result | Evidence under prefix |
| --- | --- | --- |
| `cc -std=c17 -Wall -Wextra -g -o PREFIX/legacy-main PREFIX/main.c PREFIX/traverse.c` (sources from `git show HEAD:src/...`) | 0 | `libc-build.*` |
| `python3 PREFIX/focused.py PREFIX/consume-zero test_loop_tail_zero_preserves_next_command` | intended 1; one output assertion failure | `tail-negative.*` |
| `python3 PREFIX/focused.py ./build/main test_loop_tail_zero_preserves_next_command` | 0 | `tail-green.*` |
| `python3 PREFIX/focused.py PREFIX/legacy-main test_dispatch_legacy_diagnostics` | intended 1; five diagnostic failures | `legacy-negative.*` |
| `python3 PREFIX/focused.py ./build/main test_dispatch_legacy_diagnostics` | 0 | `legacy-green.*` |
| `make test-shell` | 0; 19 CLI tests plus direct module checks | `shell.*` |
| `make -B test` | 0; complete regression including 19 shell tests and all eight commands | `full.*` |
| `make all` | 0 | `all.*` |
| `make lint` | 0; whole current C tree, one suppressed non-user-code warning | `lint.*` |
| `clang-format --dry-run --Werror src/main.c src/shell.c src/shell.h src/registry.c src/registry.h tests/test_shell_module.c` | 0 | `scoped-format.*` |
| `make format-check` | 2; six unchanged HEAD files above, plus concurrent out-of-scope src/cmd_clear.c | `whole-format.*`, `format-baseline.json` |
| `git diff --check -- README.md FILES.md docs/sessions.txt` | 0 | `diff-check.*` |

The runner's literal PREFIX is the evidence prefix above. Independent review's
isolated builds/sanitizers describe its earlier snapshot; this worker reran
normal builds/tests/lint, not a new sanitizer campaign. Concurrent parent-side
edits appeared in cmd_tail.c/cmd_touch.c and their CLI tests during final audit.
The new tail and touch summaries each initially failed three exact shell-help
assertions (`concurrent-help-mismatch.*`, `concurrent-touch-help-mismatch.*`); this
worker updated only the shell test's expected summaries, retaining exact help
assertions. All checks were rerun afterward.
Current reviewed-scope hash differences are recorded in `docs-audit.json`; parent
owns review/sanitizer disposition for its extra module edits. Concurrent new
`src/cmd_clear.c` also adds an out-of-scope format failure; it is not one of the
six accepted preexisting files. No clear source was edited by this worker.

## Documentation and final scope

README now presents main as the builtin-shell entry point, interactive/one-shot
examples and all eight commands including touch; standalone help is optional.
Shell usage is user-facing; the original 57-line selection/verification plan is
preserved verbatim in the implementation report, explicitly historical and
superseded where its tail/parser wording was overbroad. FILES additions are
narrow; unrelated entries and historical material are preserved.

Changed paths owned by this disposition worker:

```text
README.md
FILES.md
docs/commands/shell.md
docs/reports/shell-integration.md
docs/reports/shell-disposition.md
docs/sessions.txt
src/shell.h
tests/test_shell.py
```

Session `20261008_193146_7c6fd3`, verified title “Subagent: Finish shell review
dispositions and…”: appended once under the authorized startup procedure.
No command-module, Makefile, source implementation, independent-review, AI_LOG,
wiki, stage, commit or publication changes. Parent must also include the exact
build dependency closure in shell-review.md; this worker's eight changed paths
alone are not the complete shell delivery.

Readiness is **shell scope ready for parent staging/extracted-index checks**,
not already staged or committed. Local shell/report/command links and new FILES
paths are verified by `docs-audit.json`/`audit.py` under the evidence prefix.
Unrelated inherited demo issue: the preserved MP4/WebM are absent from this
working tree, and relocated `demo/demo01.sh` still changes into demo rather than
repo root before make. README now flags the script limitation; no demo edit or
execution was authorized here. The historical MP4 link is the sole preserved
missing local Markdown target in the checked docs. Parent owns any demo cleanup.

## Resume audit — October 8, 2026

**Ready for parent staging and exact extracted-index verification**, not committed.
The interrupted transcript ended during provider recovery after its checks but
before its final audit. Its `run_checks.py` aborted after whole-format checking;
no missing final assertion was inferred as a pass. Original reports/logs remain
unchanged; this section supersedes earlier readiness/hash claims where needed.

Saved diagnostics tests, zero-tail regression, public header preconditions,
concise shell usage and README eight-command index were already complete. No
replacement implementation was needed. Corrected the remaining blanket stdin
claim in `docs/commands/tail.md`; N=0 reads nothing. FILES records new evidence.

Reviewed actual diffs against the independent-review dependency snapshot:
touch/tail source changes are help strings/spec descriptions only; tests add
assertions for those strings. No parsing, I/O, timestamp or queue logic changed.
The inherited head/tail callback/run help-cardinality distinction stays explicit.

[Durable raw evidence](../../logs/commands/shell-resume-20261008.log) contains
exact argv/cwd/status, untruncated separate-stream output, source SHA-256 ledger,
help-only diffs, and all 13 byte-preserved cleanup archive/move records.
Scratch: `/home/name/.hermes/cache/scratch/shell-resume-umlpp9g4/`.

| Executed gate | Result |
| --- | --- |
| `make -B test-shell` | 0; 19 CLI tests and module checks |
| `make -B test-touch-impl test-tail-impl` | 0; touch 11 tests, tail 46 checks, both modules |
| `make -B test` | 0; complete eight-command regression |
| `make -B all` | 0 |
| `make lint` in isolated tree and working repository | both 0 |
| scoped clang-format shell/touch/tail C/header checks | 0 |
| isolated `make format-check` | 2; exactly six byte-identical HEAD files already accepted above |
| refreshed zero-tail behavioral mutant / libc-HEAD diagnostic negative controls | intended 1 / 1; current implementation focused checks both 0 |
| ASan/UBSan/LSan shell CLI, touch/tail CLI and modules | 0; shell module passes at project-default O0 |

**Exact isolation:** extracted HEAD src/tests/Makefile/.clang-format, overlaid
only the 34 build paths from shell-review's allowlist with current bytes, linked
only installed argtable3/CUnit directories. Current main/registry and all eight
modules were used; **no HEAD-main substitution**. No clear/cd source/registration
was present. Working wildcard lint also passed; isolated full gates remain the
uncontaminated eight-command delivery evidence, not proof of later integration.

Additional O1-sanitized shell-module run aborts at test_shell_module.c:145
(`calls == 1 && interrupt_read == 0`). A fresh independent-review baseline
snapshot reproduces the byte-identical assertion failure; default O0 sanitizer
passes. This is a disclosed inherited optimized fault-harness limitation, not
an all-optimization pass or a new help-edit regression. No test was weakened.
Installed argtable3 was not itself sanitizer-instrumented.

[Exact combined staging allowlist](../../logs/commands/shell-resume-allowlist.txt)
contains 67 explicit paths (including the deletion): the 34 build dependencies,
selected command guides, original reports/reviews, two independent touch logs,
new evidence, README/FILES and approved documentation cleanup/procedure.
Planned-cp guide/archive is only the existing approved documentation cleanup,
not cp implementation. The tracked deletion `docs/commands/mkdir-review.md`
is explicitly allowed for its byte-preserved move to `docs/reports/mkdir-review.md`. The four
planning moves have untracked/ignored sources/destinations, so `.tmp` stays out
of the index. No wildcard report/log add. No AI_LOG/wiki/data/demo/session,
clear/cd, shared-helper changes or unrelated relocations belong in this batch.

Current local documentation links pass except the already recorded missing demo
video. New session `20261008_203032_95e1ae`, verified title
“Subagent: Resume interrupted shell dispositions from…”; registration left to
the parent because coordination files are outside this worker's exclusive scope.
Parent still owns final exact index extraction/retests and commits. No staging,
commit, publication, original-report rewrite or concurrent-worker edits here.
