# clear/cd actual-main integration

## Outcome and scope

**Ready for independent combined review, not staged or committed.** Actual main
now has ten builtins, including sequentially integrated clear then fixed cd.
Nine optional standalone binaries exist; cd deliberately has none. Command
implementations and command-owned tests match their independent-review hashes.
Only shared registry/build/tests, concise command status/indexes, narrow FILES
additions, this report and owned integration evidence were changed.

Baseline HEAD: `0ef9cb995568626e9edef7cfb5e6580995749369`.
Evidence: `logs/commands/clear-cd-integration/`; every executed gate has complete
`.stdout`, `.stderr`, and `.json` command/cwd/environment/status records.
`fresh-manifest.json` identifies fresh snapshots and tested source hashes;
`summary.json` verifies reviewed-code identity and inherited formatting failures.
The older implementation/review reports are preserved historical snapshots;
this report and the command guides describe current integration status.

No stage, commit, push, cp work, wiki edit, AI_LOG read/write or user-terminal
clear occurred. Session ID was absent from this subagent's environment, so lookup
and registration could not be verified; the parent must resolve registration.
Command-development/TDD skills and project session, anatomy/API, add-command,
coding, design, TDD, verification and logging procedures were loaded/read.

## Sequential genuine integration RED → GREEN

| Step | Observed result |
| --- | --- |
| Unchanged eight-command baseline | `make -B test-shell`: 19 Python tests and shell module, status 0. |
| Clear standalone baseline | Strict direct build status 0; no missing executable used as RED. |
| Clear actual-main RED | Existing `test_main_parity` exit 1: main returned 127/unknown command instead of terminal bytes/status/help and omitted discovery. All nine failing assertions retained. |
| Clear inventory RED | Updated discovery/help/parity expectations fail against unchanged eight-command main, exit 1. |
| Clear GREEN | Added clear registry/source/header, standalone/module/target/all/test/clean wiring; `make -B test-clear test-shell` status 0, 8 clear plus 19 shell Python tests and both modules. |
| Cd persistence RED | Existing absolute/relative main-loop test exit 1: unknown cd and unchanged cwd, against real clear-integrated main. |
| Cd inventory RED | Added ten-command expectations and cd-help tests; dispatch slice exit 1 against the clear-only main. |
| Combined GREEN | Added cd registry/source/header and wrapped module/test/clean wiring, no standalone or main/shell branch. `make -B test-cd test-clear test-shell` status 0: 7 cd, 8 clear, 21 shell Python tests and all three C modules. |

The reviewed scratch patches were inspected as references, not blindly applied.
`tests/test_shell.py` now separates exact builtin inventory from standalone help
and execution parity, preserving all original standalone coverage and adding
clear. Cd help is checked through main/direct callback aliases; the added loop
checks physical symlink/`..` persistence, clear coexistence, failure continuation
and repeated help. Cd's dedicated suite tests HOME, literal dash/empty paths,
syntax, permission/path-length failures, physical resolution and exit statuses.
Clear's dedicated suite tests discovery/help/dispatch, seven-byte PTY effects,
nonterminal silence, TERM guards, invalid-before-effect, closed output, and
repeated success/error/help/success in real main.

Registry tests assert clear/cd specification identity, initial count 10,
capacity 32, and exact overflow behavior. After the probe, the computed threshold
is `32 - 10 - 1 = 21` successful extras; three further extras are rejected and
the final count remains 32. Duplicate re-registration still succeeds when full.
No count, overflow, fault or ownership assertion was removed or weakened.

## Final executed gates

From the repository root:

```sh
python3 logs/commands/clear-cd-integration/run.py combined-green make -B test-cd test-clear test-shell
python3 logs/commands/clear-cd-integration/run.py full-regression make test
python3 logs/commands/clear-cd-integration/run.py lint make lint
python3 logs/commands/clear-cd-integration/fresh.py
```

| Gate | Actual result |
| --- | --- |
| Actual working full regression, before and after final clean/rebuild | `make test`, status 0 both times; all existing/new CLI, CUnit, random-line and module recipes executed. |
| Actual clean/all | `make clean`, status 0; main, shell module, clear and both new module-test binaries absent. `make all`, status 0, rebuilt main and nine standalone tools. |
| Fresh strict O0 complete regression | New scratch source copy, `make -B test` with all project warnings plus `-Werror`, status 0. |
| Fresh strict O2 focused integration | Separate fresh source copy, `make -B test-clear test-cd test-shell`, status 0. |
| Fresh default-O0 ASan/UBSan complete regression | Separate fresh source copy, instrumented main, standalones and modules; `make -B test`, status 0. Leak detection and halt-on-error enabled; no sanitizer findings. |
| Whole-project clang-tidy | `make lint`, status 0. |
| Scoped formatting | Registry, shell module and all clear/cd C/header/wrapper/module files, dry-run `--Werror`, status 0. |
| Whole-project formatting | `make format-check`, status 2, only six inherited paths listed below; their bytes equal HEAD. |
| Real-main edge gates | Actual default binary plus fresh O0/O2/sanitized main each pass `edges.py`, status 0. |
| Real dependency-internal OOM | `./build/test_cd_module --internal-oom`, exit 1 / `Out of memory!`; observed dependency termination, not recovery. |

Fresh build flags: project C17 warnings, `-Werror -g3`, and the indicated
`-O0`/`-O2`. Sanitizer flags add
`-fsanitize=address,undefined -fno-omit-frame-pointer` at **default O0**;
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
Prebuilt argtable3/CUnit libraries are borrowed, not sanitizer-rebuilt.
Intentional module diagnostics from syntax/OOM/output-fault assertions are not
sanitizer findings. No O1 sanitizer-harness pass is claimed: the inherited O1
fault-injection limitation remains acknowledged rather than weakening its test.

Additional `edges.py` gates use real subprocess-owned resources:

- Regular-file clear output is exactly empty/status 0 for unset, empty, dumb,
  xterm and unknown TERM, with stdin closed.
- A fully interactive stdin/stdout PTY main loop interleaves pwd/clear/cd,
  follows a symlink physically, applies `..`, and checks exact output including
  both screen clears, CRLF translation and eight stderr prompts. It drains while
  the child runs with a deadline; no output reaches the user's terminal.
- Thirty relative cd steps reach a physical cwd longer than 4096 bytes; pwd
  prints that exact path, then an absolute cd returns to the fixture root.
- Dedicated cd permission assertions ran as nonroot euid 1000, not skipped.

Inherited format-only failures: `src/cmd_spec.h`, `src/traverse.c`,
`src/traverse.h`, `tests/test_help.c`, `tests/test_output.h`,
`tests/test_recursive.c`. These files were not edited.

## Delivery/index proposals and remaining gates

`prepare_delivery.py` in the evidence directory creates a scratch-only delivery
proposal from HEAD. It emits exact newline-delimited minimal path allowlists,
HEAD-based `README.md`/`FILES.md` index versions and their patch, with recorded
hashes. Working FILES reorganization/data/wiki/demo material and the existing
README demo relocation difference remain intact and excluded from the proposals.
No proposal is applied to the real index; the real index remains empty.

The actual HEAD-plus-allowlist proposed runtime tree was freshly built and ran
`make -B test`, status 0. Initial new-document link inspection found the original
cd review's linked evidence aggregate/directory absent from the proposal; that
aggregate is now explicitly included. The initial inspection failure is preserved
as `proposal-link-discovery-initial.*`, not misreported as a runtime regression.
Historical reviewer snapshots/raw archives beyond that aggregate remain outside
the minimal commit proposal; they are still present unchanged in the working tree.

The minimal main/code/docs proposal includes the reviewed clear/cd implementations,
tests, command/implementation/review documents, owned integration evidence and
shared changes. The historical clear report links to two **untracked supporting
procedure documents**, `docs/COMMAND_ANATOMY.md` and `docs/COMMAND_API.md`.
Those are excluded from the narrow integration allowlist to avoid absorbing
unrelated work. The parent must authorize those dependencies or narrowly resolve
those two report links before final index dependency/link verification. Their
absence does not affect compilation/tests but is not a closed publication gate.

Remaining: independent combined review; parent acknowledgement of known parser
OOM/void-usage/private-getopt limits; resolve the two documentation dependencies;
verify the exact approved extracted index tree and allowlist after any correction;
then parent-only commit. No final committed or index-tree pass is claimed here.

Limits retained: serial process-wide cwd/parsing only; exported getopt values,
not private interrupted-scan state or concurrency; real argtable internal OOM
may terminate before restoration; void usage callbacks cannot return constructor
OOM status. ANSI assumptions do not establish arbitrary terminal compatibility.
No exhaustive fuzzing, coverage, 32-bit run or full Bash/ncurses parity is claimed.
