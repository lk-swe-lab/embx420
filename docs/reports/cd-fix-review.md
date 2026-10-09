# cd getopt fix: independent re-review

## Verdict

**Original blocking source finding resolved. Ready for actual main integration,
not yet integrated or approved as a final combined delivery.** No new blocking
cd source defect found. Approval covers exported getopt values on returning
calls in the selected serial-shell contract, not complete process-global isolation.
Original review remains unchanged: [cd-review.md](cd-review.md).

Owned evidence: `logs/commands/cd-fix-review-20261008-210921/`.
`manifest.json` records exact commands/statuses, reviewed and snapshot SHA-256
values, dependency hash, euid and limits. Each execution has complete separate
stdout/stderr/JSON records. Before/current diffs are preserved, because the cd
files are untracked and ordinary git diff does not expose their correction.

## Source and regression assessment

- `src/cmd_cd.c:74–82` saves optarg/optind/opterr/optopt immediately before
  the sole arg_parse call and restores all four immediately after it returns.
  No branch, goto or ordinary return intervenes. Therefore parse error, normal
  help, failed help output, empty/missing HOME, empty operand, explicit dash,
  chdir failure and success all execute restoration before their own behavior.
- Malformed argc/argv/NULL elements and constructor/copy allocation failures
  occur before parsing. Usage constructs/renders/frees tables without parsing.
  Fresh sentinel checks cover those early paths and usage callbacks too.
- Source notes explicitly exclude private interrupted-scan state. The current
  implementation report explicitly excludes concurrency and synchronization;
  process-wide cwd and parser globals are unsuitable for an inferred threaded
  guarantee. Restoring exported values does not restore private getopt state,
  make nested interrupted scans safe, or synchronize another thread.
- The source change is narrowly the include, limited claim and immediate guard;
  cwd/HOME/PWD/OLDPWD/CDPATH and argv-copy behavior remain unchanged. The four
  source/test hashes match the fix worker's manifest; reviewed owned bytes
  remained unchanged throughout this re-review. Static review found no new
  injection, secret, exfiltration or resource-ownership concern.
- Inspected prior actual RED/GREEN and all four missing-restore mutant records:
  each mutant aborted on its corresponding changed sentinel. Those are prior
  worker evidence, not fresh executions by this reviewer. Independently rebuilt
  the final harness against pre-fix source: compile 0, execution SIGABRT (-6),
  optind=3/opterr=0/optarg not preserved. Fixed source passes the same harness.

## Fresh executed verification

All builds use a new snapshot of current sources, not previous binaries. Only
scratch registry/Makefile and explicitly identified scratch shell expectations
were changed. The repository registry still excludes cd at final inspection.

| Check | Result |
| --- | --- |
| Strict C17 module build and sentinel harness | 0; 400 repeated calls plus malformed inputs, constructor/copy faults, syscall faults, help-output recovery, extra returning paths and usage callbacks pass. |
| Original independent global probe rebuilt against fixed source | 0; success/error/help return 0/2/0 with pointer and all integer sentinels preserved. |
| Scratch `make test-cd` | 0; all 7 real-main tests and module harness pass, including physical traversal, persistence, HOME, syntax, help/discovery and last-status propagation. |
| Permission coverage | euid 1000; real mode-000 permission failure exercised, not skipped. |
| Scoped clang-tidy / clang-format / Python syntax | All 0. |
| Fresh ASan/UBSan module and main | Builds/run 0; harness and all 7 main tests pass, no sanitizer finding. |
| Fresh unchanged eight-command shell baseline | 0; 19 Python tests and C shell module pass. |
| Scratch cd addition, unchanged shell expectations | Python exit 1 with exactly 3 help-inventory failures; C aborts (-6) at count == 8. |
| Scratch count-only correction | C aborts (-6) at remaining-capacity assertion, described below. |
| Scratch inventory/count/capacity maintenance, fresh strict rebuild | 0; 19 shell tests and C module pass; all 7 cd tests and cd module pass again. |
| Real dependency-internal OOM | Exit 1 with `Out of memory!` inside argtable; unchanged limit, not recovery. |

Strict flags include project C17 warnings, -Werror, -g3 and -O0. Sanitizers add
`-fsanitize=address,undefined -fno-omit-frame-pointer`, with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The prebuilt argtable archive
is not instrumented. `cd: help output failed` is intentional /dev/full output.
All cwd-changing checks run in subprocesses; no parent-agent cwd change occurs.

## Required parent-owned integration maintenance

The original inventory finding is confirmed and **has an additional capacity
expectation** hidden behind the first C count assertion:

1. Add cd's registry entry, main source/header dependencies, wrapped module-test
   target and aggregate-test/clean participation. The handoff patch applies
   read-only against the repository at this review; reconcile later changes.
   No standalone cd or cd-specific main/shell branch is needed.
2. In `tests/test_shell.py`, separate builtin help inventory from standalone
   parity. Include cd's exact summary in help; retain the existing eight
   standalone names. Otherwise `test_dispatch_help`,
   `test_loop_multiple_and_eof`, and `test_loop_stdin_not_read_ahead` fail.
   Preserve standalone parity and test cd help through main/callback parity.
3. In `tests/test_shell_module.c`, maintain initial count and cd discovery.
   For cd alone, eight becomes nine. Also fix line 167's remaining-capacity
   threshold: after nine builtins plus probe, only 22 extra registrations fit
   capacity 32, not 23. A fresh count-only test demonstrated that second abort.
   If clear is integrated simultaneously, recompute from the final builtin
   inventory instead of hardcoding the cd-only values. Preserve overflow and
   final count assertions; do not weaken or remove them.
4. Run actual combined-tree `TMPDIR=/home/name/.hermes/cache/scratch make test-cd`,
   `make test-shell`, accumulated `make test`, relevant lint/format and fresh
   sanitizer/persistent-main checks before final delivery. Scratch passes are
   not proof of the concurrently staged or final repository tree.

The scratch maintenance diffs are saved separately in evidence; they are
illustrative verified changes, not edits to repository tests. The frozen
snapshot records the count-only intermediate C test, with final maintenance
explicitly supplied by those diffs. No full-project pass is claimed.

## Remaining acknowledged limits and scope

Real argtable-internal OOM can terminate the shell without returning through
restoration. Constructor-NULL seams and owned-copy OOM do not establish all-OOM
recovery. Void print_usage still cannot convey constructor-NULL status and may
emit empty help; output errors remain stream state. These limits remain explicit
in the implementation report and require parent acceptance for delivery.
No exhaustive fuzzing, 32-bit run, private-state or concurrency verification is
claimed. The original broad behavioral review is retained, not overwritten.

Created only this report and its owned evidence; no repository source, shared
registry/Makefile/tests/README/FILES/index, staging or commit changes were made.
Session verified: `20261008_210921_5beab2`, title “Subagent: Independently re-review
cd getopt-global fix…”. Registration deferred to parent because shared writes
are excluded; sessions.txt was not edited.
