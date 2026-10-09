# sleep implementation

## Research and selection (before TDD)
Course `CLI_Tools.docx` indexed paragraphs 229–231: delay; -h/--help.
Saved FOLLOW_ON_FIVE selects validated bounded durations and interruption policy.
Installed manual sleep(1), GNU 9.4; online GNU manual 9.12 §25.1;
POSIX Issue 8 nanosleep (retrieved primary HTML in external research artifacts).

Manual inventory: NUMBER operands, default seconds; fractional numbers;
s/m/h/d units; multiple operands summed; --help; --version; nonnegative
values including GNU floating/exponent/hex/locale syntax and infinity;
normal silence, zero success, nonzero failure. Keep delay, zero, fixed decimal
fractions and help. Defer units, multiple operands, --version, exponent/hex,
infinity and locale decimals: unnecessary to course minimum, increase policy.
-h is a course addition. No JSON required for this human utility.

## Selected contract
`sleep [-h|--help] [--] SECONDS` exactly one duration unless help requested.
ASCII `[0-9]+(\.[0-9]{1,9})?`, at most 32 bytes, range 0..2147483647
inclusive; fractional part at upper bound must be zero. No sign, whitespace,
NaN/Inf, exponent, suffix or leading/trailing decimal point. Leading zeros
accepted within length bound. Exact integer nanoseconds without float/locale.
0 successful silence, 2 invalid arguments, 1 allocation/nanosleep/help-output
failure. Invalid syntax/options take priority over help; help with valid table
syntax need not validate duration. Unknown options after operand are errors
regardless of POSIXLY_CORRECT; -- ends option processing. No stdin consumption.
Validate everything before sleeping. No module-owned descriptors or changes to
argv, exported getopt globals, environment, locale or signal dispositions/mask.
No separate clock calls: nanosleep relative duration and its errno are the API.
Handled signals: retry EINTR using returned remaining timespec until complete;
other errors fail. Default terminating signals retain normal process cancellation.
Repeated handled signals may add handler/scheduling overhead; no upper latency
promise and no host cancellation protocol beyond inherited signals. Not thread-safe
with concurrent getopt use; no promise about getopt private interrupted scans.

## Plan / acceptance matrix
Vertical 1: missing/zero/integer parsing, standalone runner and argtable help.
Vertical 2: fractions, strict grammar and upper-bound deterministic timespec seam.
Vertical 3: EINTR remainder retries and operational failure seam.
Vertical 4: resources, constructor/owned OOM, output help failure, private argv
mutation/getopt sentinels, inherited signal cancellation; main registry/help/loop/PTY.
Reference independent fixtures: actual short GNU intervals/status/silent streams;
project-only strict errors, help and seams separately. Bounded monotonic timing
lower bound checks; no large real waits. No fabricated timing reference.
Full make all/test/lint, scoped formatting, default O0 non-PIE ASan+UBSan full
regression in fresh final patch checkout. Inherited format/O1 fault issues and
argtable internal fatal OOM separately characterized. All writes scratch-only;
no shared repairs, skipped rmdir, stage, commit, wiki, AI_LOG or next command.

## Evidence
Results appended after actual execution. Compact full raw execution stream and
structured ledger are logs/commands/sleep-execution.log and sleep-evidence.json.
External final exact-patch verification does not alter these frozen artifacts.

## Executed results

| Gate | Actual observation |
| --- | --- |
| Integer/zero TDD | Runnable baseline returns 2 for zero (RED); minimal integer module passes slice 1. |
| Decimal TDD | Whole-second implementation returns 2 for 0.020 (RED); decimal parser passes real short intervals and synthetic upper-bound/boundary requests. |
| EINTR TDD | Pre-retry module fails with missing-remainder marker and SIGABRT; remainder loop passes two remainders, zero remainder, EIO/EINVAL and actual 30 ms handled-alarm interval. |
| Main integration TDD | HEAD main fails sleep dispatch; integration passes discovery/help/status, 100 repeated success/error/help loop cycles and terminal-owned PTY exact prompts/CRLF bytes. First full-suite parity inventory failure corrected by adding sleep case without dropping cases. |
| Reference | Actual GNU sleep 9.4 passes independent zero/short decimal/-- fixtures and monotonic lower bounds; true fails 20 ms lower bound. No long reference waits or fabricated timing. |
| Grammar | Empty/negative/signs/NaN/Inf/non-ASCII/whitespace, excessive precision, suffix/exponent/hex, trailing garbage, multiple operands and overflow rejected before waiting; POSIXLY_CORRECT unset/set checked. |
| Module | 1000 constructed exact-timespec fixtures, 100 mixed reuse cycles; forced parser argv permutation, caller pointers/bytes, getopt sentinels, cwd/locale/environment pointers/PATH, borrowed descriptors and unread stdin pass. Three constructor NULL failures and owned malloc failure recover. |
| Help/output | Run/usage exact byte parity; usage callback OOM diagnostics, /dev/full stream errors, status-bearing help failure and subsequent stdout recovery pass. |
| Signals | Actual handled SIGALRM resumes, preserves disposition and blocked SIGUSR1. Readiness-synchronized real-nanosleep children terminate on inherited SIGTERM/SIGINT; CLI cancellation passes. Production installs no handlers. |
| Negative controls | Compiled no-delay/no-EINTR-retry/direct borrowed argv/no-getopt-restoration mutants fail intended assertion markers; unchanged production passes afterward. |
| Normal release | make all test, make lint, scoped clang-format pass; all new C files satisfy 80 columns after correcting an overlong include annotation. |
| Default sanitizers | Fresh candidate and pristine HEAD full make all test at O0/non-PIE ASan+UBSan pass with leak/error halting. Dependencies not instrumented. |
| O1 characterization | Candidate O1 sleep sanitizer scope passes; candidate and pristine HEAD full shell fault seam fail at calls == 1 && interrupt_read == 0. No O1 full-recovery claim. |

### Preserved failures and limits

Global format-check fails only unchanged src/cmd_spec.h, src/traverse.c,
src/traverse.h, tests/test_help.c, tests/test_output.h and tests/test_recursive.c;
pristine HEAD reproduces all six. Changed-C scoped formatting passes.

Installed Argtable internal malloc/calloc failure exits the host with status 1
and `Out of memory!`; constructor-NULL/module-owned allocation recovery does not
promise arbitrary dependency-internal OOM recovery. Void usage callback reports
output failure through stream state. There is no production clock_gettime API:
relative nanosleep EIO/EINVAL errors are tested, not fictional clock failures.

Handled signals add possible handler/scheduling overhead. No private-getopt scan
restoration, concurrent-thread safety or custom shell cancellation protocol is
promised. Units, multi-duration summing and general GNU floats are deferred.
Web-extract backend cannot extract URLs; primary GNU/POSIX HTML was retrieved
successfully with curl. Hidden-stderr initial EINTR RED was rerun with an explicit
marker before implementation. Formatting's include alignment initially exceeded
80 columns; a narrower annotation fixes the actual line-limit assertion.

Scratch-only implementation: inherited fixed 1277-path inventory matches.
Exact final preservation and patch/blob/mode/anchor checks and final replay
regressions are external handoff gates, not implied by this frozen report.
No shared edits, stage, commit, wiki or AI_LOG access; independent review and
exact-index delivery remain parent gates. Session lookup verified the exact
subagent session; scratch-only registration is excluded from delivery.

Detailed commands/statuses/streams: [raw execution](../../logs/commands/sleep-execution.log)
and [ledger](../../logs/commands/sleep-evidence.json). Final replay writes its
streams outside this tree so frozen delivered evidence cannot self-modify.
