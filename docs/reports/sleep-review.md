# sleep independent review

Verdict: approved after one malformed-option correction. Reviewed against committed
HEAD `134de9868ff7f462ea301696e8a4e6ec11ac845e` in isolated archives; no staging or
commit. Exact-index delivery remains a separate parent gate.

## Finding and correction

The independent grammar oracle exposed a hang on `sleep '- -'` and `sleep '-h-'`,
also through real `main sleep`. A deadline regression failed with status 91 before
any correction. GDB stopped at the 51st dependency `getopt_internal` call with
`optind == 1` and empty scan pointer. The dependency's trailing internal-hyphen
path returns a malformed-option result without advancing the operand, then
restarts it. Upstream `arg_getopt_long.c` lines 524–534 corroborate this path;
the installed binary behavior, not an assumed source-version match, is the proof.

`src/cmd_sleep.c` now rejects internal hyphens in short-option clusters before
Argtable parsing, only before the first explicit `--`. No supported short flag
contains such a hyphen. Other option parsing and shared help stay in Argtable.
The deadline regression now passes standalone/main, unset/set POSIXLY_CORRECT,
help-error priority, whitespace/control bytes and the `--` boundary. No production
signal handler or clock API was introduced.

## Independent acceptance evidence

[Python review tests](../../tests/test_sleep_review.py) construct 6,378 independent
fixtures: 1,851 valid and 4,527 invalid. Expected grammar uses a regex and exact
`Fraction` arithmetic, not the production digit parser. Constructed fractions cover
all 1–9 precisions, maximum/range/length boundaries, leading zeros, signs,
NaN/Inf, garbage and excessive precision. Large durations use only a syscall seam.

[Module oracle](../../tests/test_sleep_review_module.c) asserts every requested
normalized timespec. A synthetic elapsed clock uses three distinct returned
remainders down to zero; each next request must equal the preceding remainder and
total synthetic elapsed time must equal the original interval. EIO/EINVAL stop
with status 1. Parser pointer permutation and nondefault getopt sentinels verify
borrowed state; mixed invalid/valid calls exercise reuse.

Real periodic SIGALRM interrupts a 35 ms call; monotonic elapsed time is bounded
below by 35 ms and above by 3 s. Actual runs recorded 11 handled alarms and 12
nanosleep calls, with exact kernel-returned remainder reuse. Disposition and mask
remain inherited. Existing module tests additionally execute SIGTERM/SIGINT
cancellation, allocation/constructor faults, unread stdin, help/output failures
and recovery. Readiness notification is before forwarding the real syscall, not
a claim that the child is already kernel-blocked.

Actual GNU 9.4, standalone and real main independently satisfy specified short
valid fixtures, exact silence/status and monotonic lower bounds. No GNU timing is
fabricated or used to manufacture project expectations. Real `true` fails the
15 ms lower-bound negative control. Five compiled precision/EINTR/validation/
argv/getopt mutants fail with their intended markers and status 90; unchanged
production then passes. Reproduce with:

```sh
make all test
make test-sleep-review
python3 tests/test_sleep_review.py . --mutations
make lint
clang-format --dry-run --Werror src/cmd_sleep.c src/cmd_sleep.h src/sleep_main.c tests/test_sleep_module.c tests/test_sleep_review_module.c src/registry.c tests/test_shell_module.c
```

## Executed release gates and limits

Corrected candidate: normal `make all test`, full lint and scoped formatting pass.
Default O0 non-PIE ASan/UBSan full `make all test` passes, with leak/error halting.
Independent oracle also passes ASan/UBSan. Full tests retain real-main discovery,
run/usage help parity, dispatch/status, registry capacity, 100 mixed loop cycles
and child-owned PTY exact prompts/CRLF. All new C lines are at most 80 columns.

Pristine HEAD full O0 sanitizer regression passes. Global formatting fails with
status 2 only for the same six inherited paths: src/cmd_spec.h, src/traverse.c,
src/traverse.h, tests/test_help.c, tests/test_output.h, tests/test_recursive.c.
Candidate and pristine HEAD O1 sanitizer shell fault modules abort at the same
`calls == 1 && interrupt_read == 0` assertion; focused candidate O1 sleep passes.
This is not a claim that full O1 regression passes. Static dependencies are not
sanitizer-instrumented. Actual dependency-internal OOM still exits 1 with
`Out of memory!`; module-owned/constructor recovery is a distinct tested scope.

Review harness corrections are preserved: initial broad oracle tool timeout
(124, later isolated to the genuine malformed-option hang), missing relative
header during lint (2, fixed in the new review test), and unformatted new guard
(1, scoped formatting corrected). These are not reported as successful gates.
Raw stream whitespace is intentionally retained; strict source/documentation
whitespace must pass independently, with raw-log exceptions enumerated externally.

Session `20261009_142047_f31277` was authenticated; registration is scratch-only.
Inherited fixed 1,277/1,290-path inventories were authenticated at startup;
final preservation and exact frozen-patch replay are external handoff evidence.
No shared overwrite, skipped-command work, wiki/AI_LOG access, staging, commit,
push or next-command dispatch occurred. The original implementation report/logs
remain unchanged; this report supersedes its review-ready status, not its history.

[Complete review streams](../../logs/commands/sleep-review-execution.log) and
[command/status/hash ledger](../../logs/commands/sleep-review-evidence.json) are
frozen before the final patch. Final exact-patch regressions write outside the
candidate, preventing evidence self-modification.
