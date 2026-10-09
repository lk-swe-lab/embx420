# date implementation and verification

## Scope and sources

Date-only candidate based on `08d4bc5b7e26c2016bfc2594dcfd711abd044a37`,
not the mixed shared working tree. User authorized isolated implementation and
execution, not staging/committing. Skipped rmdir remains excluded.

Course read-only SQLite query: `lecture_content` joined to `lecture_sources`,
`data/02/CLI_Tools.docx` paragraphs 233–235: show system time, `-h`/`--help`.
JSON for human tools is optional and deliberately omitted.

Primary sources inspected: GNU coreutils official **date invocation** manual;
installed `/usr/bin/date` 9.4 `--help`/`--version`; POSIX Issue 8 `clock_gettime`,
`localtime`/`localtime_r`, and `gmtime`/`gmtime_r` function specifications.

- https://www.gnu.org/software/coreutils/manual/html_node/date-invocation.html
- https://pubs.opengroup.org/onlinepubs/9799919799/functions/clock_gettime.html
- https://pubs.opengroup.org/onlinepubs/9799919799/functions/localtime.html
- https://pubs.opengroup.org/onlinepubs/9799919799/functions/gmtime.html

### Full manual selection inventory

| Manual feature | Selection and reason |
| --- | --- |
| Display current local system date/time | Keep purpose; numeric project format replaces locale-dependent GNU default. |
| `-u`, `--utc`, `--universal` | Keep all aliases for unambiguous UTC timestamps. |
| `--help` | Keep; add course `-h`. |
| `--version` | Defer packaging/version workflow. |
| `-d`/`--date=STRING`, `-f`/`--file=DATEFILE` | Defer input parsing/batch parsing; current time only. |
| `-r`/`--reference=FILE`, `--resolution` | Defer file timestamps/clock-resolution workflow. GNU display selectors are mutually exclusive. |
| `-s`/`--set=STRING`, `MMDDhhmm[[CC]YY][.ss]` | Defer clock mutation; never set the clock. |
| `--debug` | Defer with date parsing. |
| `-I[FMT]`/`--iso-8601[=FMT]` | Defer date/hours/minutes/seconds/ns format selection. |
| `-R`/`--rfc-email`, `--rfc-3339=FMT` | Defer named output formats (RFC3339 date/seconds/ns). |
| `+FORMAT` operand | Defer broad format language; no operands. |
| GNU format directives | Defer literal percent; weekday/month names; date/time locale representations; year/century/ISO-year; month/day/day-of-year; hour/minute/second/nanosecond/epoch; quarter; AM/PM; weeks/weekdays; newline/tab; timezone names and offsets. |
| GNU formatting modifiers | Defer `- _ 0 + ^ #`, decimal widths, and `E`/`O` locale modifiers; numeric fields normally zero-padded. |
| stdout date/help/version, stderr errors, zero/nonzero status | Keep streams; project fixes statuses 0/1/2. |
| Locale/TZ-controlled GNU default | Numeric project output ignores locale; libc TZ/default local rules retained without environment writes. |

## Selected contract

Written before tests in the isolated external `contract.md`; SHA256 is retained
in the external handoff manifest. Scope decisions were delegated autonomously.

- No operands or format language; bare `--` accepted. Unknown flags/operands,
  including GNU clock-setting syntax, are status 2 with empty stdout and stderr
  diagnostics. Invalid arguments take precedence over help. Repeated flags are
  idempotent; all UTC aliases and `-h`/`--help` share parser-derived help.
- Default: `YYYY-MM-DDTHH:MM:SS\n`, current **local** calendar. UTC adds `Z`.
  ASCII numeric format, seconds only, locale-independent. Offset-less local time
  is deliberately ambiguous during DST folds; use UTC for a unique timestamp.
- Read `CLOCK_REALTIME` once only after valid non-help parse. Clock changes are
  possible; no monotonicity guarantee. Discard subseconds. Negative epochs are
  valid when libc converts them. Restrict years to 0000–9999 before arithmetic.
  Conversion/range errors return 1, no stdout. Calendar second 60 is rendered if
  supplied; no independent leap-second accounting. POSIX clock semantics apply.
- `gmtime_r` provides UTC without changing `TZ`; local mode calls `tzset` then
  `localtime_r`. Environment strings/vector are not changed; libc's timezone
  caches are not claimed immutable. No `setlocale`, environment writes, clock
  setting, cwd mutation, stdin reads, inherited-fd closure or signal changes.
- Invocation-local Argtable3 constructors, private argv pointer copy, parser
  table cleanup and exported getopt restoration. Not concurrent/reentrant
  getopt parsing or preservation of an interrupted private getopt scan.
- Status 0 success, 1 operational failure, 2 invalid module/CLI arguments.
  Module-owned malloc and constructor-NULL errors are recoverable; Argtable's
  internal panic/OOM is a separately observed inherited limitation.
- Shared `lines_write` handles short writes/EINTR; zero write is EIO, errors
  return 1. Partial output cannot be rolled back. SIGPIPE is inherited. Help
  checks flush/error and clears stdout error state for subsequent module calls.
  The void usage callback reports allocation failure on its supplied stream.
- Numeric `snprintf` (not `strftime`): fixed ASCII format, bounded validated
  integer fields. Unexpected/failed formatting is also guarded and injected.

## Executed TDD and coverage

Complete raw streams: [execution log](../../logs/commands/date-execution.log).
Commands/statuses/stream hashes and reproduction scripts:
[evidence ledger](../../logs/commands/date-evidence.json).

| Gate | Actual observation |
| --- | --- |
| Independent GNU subset | 25 checks pass: bounded real-clock/local/UTC/aliases/locales plus eight fixed epoch/offset/leap-day/DST fixtures. GNU-only `--date=@...` is a test adapter, not a project feature. Expected bytes specified independently. |
| Local RED → GREEN | Runnable rejecting baseline fails `DATE_CURRENT`, then real local output passes. |
| UTC/parser RED → GREEN | Local-only module fails `-u`; UTC/parser added. Initially duplicate `-uu` failed; idempotent parser cardinality corrected, logical help retained; all parser checks pass. |
| Main RED → GREEN | Real HEAD-based main returns 127 unknown date, then registry/build integration passes. |
| CLI/main | 61 checks pass: aliases, locale/TZ, `--`, invalid/unsupported/empty arguments, POSIXLY_CORRECT, help agreement, /dev/full, discovery/help/dispatch and repeated pipe/child-owned stdin+stdout PTY loops. Real-clock samples are bounded by independent Python clock intervals, not equality across separately timed programs. |
| Synthetic module | Epoch zero, negative epoch, leap day, half-hour zone, both DST transitions, libc extreme-time conversion failure, year bounds, invalid calendar fields and synthetic second 60 pass. |
| Fault/reuse/state | 500 success/UTC/help/error cycles; constructor failures, owned malloc OOM, clock/conversion/formatting failures, short writes/EINTR/zero-write/EIO/partial-output failure, help output failure/recovery, malformed argv, forced parser pointer permutation, getopt sentinels, unchanged TZ/cwd/signal/descriptors and unread stdin pass. These already-correct failure behaviors are characterization, not invented pre-fix TDD. |
| Sensitivity | Five real compiled mutants rejected by intended markers: ignored UTC, caller argv mutation, lost getopt state, accepted invalid calendar, ignored output failure. Production sources unchanged by mutants. |
| Normal full suite | `make all test` passes; date is in normal regression, nineteen standalone tools/twenty builtins. Exact shell inventory/help/parity and remaining registry capacity pass. |
| Default sanitizers | Fresh candidate `make all test` with O0 ASan/UBSan/non-PIE passes, as does pristine HEAD at identical flags. Dependencies are installed unsanitized static libraries. |
| Lint/scoped formatting | `make lint` passes after narrowly documented LLVM 18 analyzer false-positive suppression; changed C/headers pass clang-format and 80-column checks. |
| Shared preservation | Fixed 1237-path inherited inventory and 1264-path task inventory show zero changes; shared HEAD/index unchanged. |

### Diagnosed tool issue and inherited limits

`make lint` initially reported uninitialized `va_list` in the test-only snprintf
forwarder despite adjacent `va_start`/`va_end`. Isolated-TU and reversed two-TU
analysis pass; source-first two-TU analysis reproduces the warning. A narrowly
commented `NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)` suppresses that
order-dependent LLVM 18 false positive only; runtime forwarding/fault assertions
remain exercised under ASan/UBSan. No global checks were disabled.

Global `make format-check` fails identically on pristine HEAD and candidate:
`src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`, `tests/test_help.c`,
`tests/test_output.h`, `tests/test_recursive.c`. These six inherited files are
not reformatted or included in the date patch. Raw evidence whitespace is
preserved rather than normalized; source/docs whitespace are checked separately.

Pristine HEAD O1 ASan/UBSan regression reproduces the inherited shell-module
`calls == 1 && interrupt_read == 0` fault-seam assertion. Its real shell CLI suite
passes. Default O0 full regression passes in both baseline and candidate;
optimized full-regression recovery is not claimed. Argtable internal parser OOM
exits the host with status 1 in the existing regression probe; constructor-NULL
and module-owned OOM remain distinct tests.

No clock setting, broad formatting/date parsing, JSON, exhaustive calendar fuzzing,
thread-safe global parser operation or host-secret environment dumps. The required
fixed/synthetic/real intervals and deterministic faults cover the selected scope.
Session ID/title were verified through exact session lookup; shared session-file
registration is deliberately excluded to retain the no-shared-edits allowlist.
No rmdir queries/tests, staging, commits, wiki/AI_LOG writes or publication.

## Frozen delivery boundary

The candidate report/log/ledger are frozen before applying the final HEAD-based
patch into two new archives. Those exact-patch normal/full-sanitizer/lint/format
commands and read-back hashes are recorded **outside** the candidate so final
verification cannot change its own evidence bytes. The external manifest contains
full SHA256/Git-blob/mode identities, unchanged anchors, dependency fingerprints,
preservation inventories, exact allowlist and patch hash. Independent review and
any authorized exact-index verification/commit remain the parent's gates.
