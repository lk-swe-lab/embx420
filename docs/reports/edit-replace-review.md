# edit-replace independent review

Verdict: selected contract passes independent source review and executed checks.
No blocking C defect found; no production C/test changes made. One documentation
mismatch was corrected in the narrow scratch README candidate: sixteen builtins
have fifteen standalone wrappers, not fourteen. Mixed working README/FILES remain
untouched. This is not exact-index verification or permission to commit.

## Reviewed behavior

Reviewed complete matcher/parser/staging source, integration hunks and harnesses:
whole-file leftmost-longest ERE versus literal bytes, ASCII case folding, literal
replacement, empty-match retention/EOF and anchors using REG_STARTEND/REG_NOTBOL;
LF/CRLF/unterminated/binary policy; successful/no-match/error JSON counts; inclusive
input/output/operand limits; regex status and thread-locale restoration; help and
borrowed argv/exported getopt state. Staging review covered source permissions,
link/type/owner/mode checks, exclusive same-directory creation, complete bounded
writes, metadata restoration, fsync/close/recheck/rename order, precommit cleanup,
postcommit output errors and remaining namespace race. Reviewed source contains
no command execution, network access or embedded credentials.

Course SQLite lookup independently located CLI_Tools.docx paragraphs 109/111
(global regex and FILE PATTERN REPLACEMENT); implementation report preserves the
selected flags and detailed course/API basis. No invented GNU executable oracle.

## Independent execution

Exact original sixteen-path patch was applied to fresh archived HEAD
705eee07b7b45d3f97f26f9c4e90b6cec2dc21e3; every candidate blob and source anchor,
installed argtable3/CUnit dependency and unrelated-file fingerprint was checked.
Normal `make all test lint`, full main CLI and scoped clang-format passed.
Fresh default O0 ASan/UBSan `make all test`, main CLI and scoped formatting passed
with detect_leaks/halt_on_error; vendored libraries remain uninstrumented.

Observed inherited acceptance suites: twenty editor CLI groups with 780 literal
and 180 ERE cases; direct module 464 invocations; 31 shell groups, 16 builtin/15
wrapper inventories, 32-entry capacity, actual main help/dispatch/parity, repeated
loop and child-owned stdin/stdout PTY assertions. Both module reuse and preserved
getopt/argv/locale are tested, not inferred from standalone success.

Additional independent finite-language span oracle uses no regex engine: enumerate
accepted byte spans, select the earliest start/longest end, then apply documented
global progression. It tests alternatives, concatenation, nullable/repeated
subexpressions, intervals, classes/negated classes, start/end anchors and newline/
non-ASCII bytes with ASCII -i. Per executable: 650 seeded ERE cases and 777
exhaustive case-insensitive literal cases. Passed through standalone and main in
both normal and sanitizer builds. Exact stdout/status/bytes/counts, inode, mode/
uid/gid, no-match timestamps, staging absence and four-second deadlines asserted.
This finite subset is not an exhaustive libc-ERE correctness or complexity proof.

All six inherited negative controls were rebuilt/re-executed; each failed with
its intended marker/status: argv, getopt, no rename, omitted recheck, no-op and
lost zero-width byte. The independent span oracle also rejected real no-op and
lost-byte controls with expected exact-output/content assertions. These are
characterization sensitivity checks, not fabricated production bug-fix TDD.

Independent real strace observed exclusive same-directory stage, stage writes,
fsync and close before rename, no source write/truncate, and no stage at all for
no-match; exact bytes/counts/source invariants checked. Trace streams and hashes
are retained with reproduction source in the ledger.

Global `make format-check` returns 2 on exactly six paths: src/cmd_spec.h,
src/traverse.c/.h, tests/test_help.c, tests/test_output.h, tests/test_recursive.c.
Each was compared directly with HEAD and is byte-identical. Scoped formatting
passes. Real raw compiler/terminal streams retain whitespace; only the explicit
implementation/review raw-log paths are exempt, never C/docs/JSON.

## Evidence and delivery

[Complete independent raw streams](../../logs/commands/edit-replace-review-execution.log)
and [command/status/hash ledger and reproduction scripts](../../logs/commands/edit-replace-review-evidence.json).
[Implementation contract and prior evidence](edit-replace-implementation.md).

Reviewed HEAD-based delivery is frozen externally at
/home/name/.hermes/cache/scratch/edit-replace-review/manifest.json and
integration.patch, with nineteen explicit paths, SHA-256/blob IDs, unchanged
anchors/dependencies and working-file preservation fingerprints. Final exact
reviewed-patch reproduction streams/ledger are separate final-proof.log/.json in
that directory to avoid self-referential tracked hashes. Review README/FILES are
narrow candidates, not the mixed working files. Final delivery verifier reapplies
that actual patch into fresh archives and reruns normal and sanitizer gates.

Session 20261009_014655_0ef41a, verified title
“Subagent: Independently review and execute verification… #4”, was appended only
to docs/sessions.txt; excluded from delivery. No stage, commit, push, wiki/AI_LOG
access or later-command work occurred. Parent owns exact-index verification and
any separately authorized commit.

## Explicit limits

ASan regexec interception rescans NUL-terminated buffers despite REG_STARTEND:
normal repeated-regexec stress remains 1 MiB, sanitizer stress is 8 KiB, both
retain exact byte/count/new-inode and four-second assertions. No claim that
1 MiB instrumented regex stress passed. Other 8 MiB input/32 MiB output limits
are unchanged; sanitizer omits only the address-space cap needed for shadow
memory. Prior long timeout diagnosis is implementation evidence, not a new
independent hostile-pattern benchmark.

Trusted patterns/paths only: no hard hostile-regex CPU/memory cap, Linux/BSD
REG_STARTEND dependency, final check-to-rename race, no directory fsync/locks,
no ACL/xattr/timestamp/sparse preservation, no postcommit-output rollback.
Internal argtable OOM may terminate host; exported getopt restoration is not
private interrupted-scan preservation or thread-safe parsing. Known inherited
O1 shell-harness limitation was not rerun; no optimized sanitizer claim.
