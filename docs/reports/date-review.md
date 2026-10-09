# date independent review

**Verdict: pass for the selected read-only local/UTC contract.** No production
source defect was demonstrated; no production code was changed by review.
Base: `08d4bc5b7e26c2016bfc2594dcfd711abd044a37`. Review used fresh HEAD archives,
not mixed shared working files. Staging/committing remain separate parent gates.

Implementation [contract and evidence](date-implementation.md) were inspected;
its patch SHA256 was authenticated as
`65fe2bd88808f63c37669a2640369e2bb2d6210e438cecbae2ac6dc2c0087983`.
All 16 original delivered blobs, 617 unchanged anchors and 14 installed dependency
fingerprints matched. The frozen implementation log's 37 records and external
log's 44 records were authenticated against their full stdout/stderr hashes and
statuses, including actual failing/green and final verification records.

## Executed checks

Complete [raw review streams](../../logs/commands/date-review-execution.log) and
[command/status/hash ledger](../../logs/commands/date-review-evidence.json)
retain successes and failed experiments without fabricated output.

- Fresh normal `make all test`: status 0, including 61 date CLI/main checks,
  synthetic module/fault tests and 500 reuse cycles. Real registry discovery,
  help, dispatch, standalone parity, remaining capacity and child-owned
  stdin/stdout PTY loops passed in the full regression.
- Fresh O0 non-PIE ASan/UBSan `make all test`: status 0 for candidate and pristine
  HEAD. Flags: `-std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow
  -Wconversion -Wstrict-prototypes -g3 -O0 -fsanitize=address,undefined
  -fno-omit-frame-pointer -fno-pie`; link flags
  `-fsanitize=address,undefined -no-pie`. Leak/error halting enabled. Installed
  Argtable/CUnit static dependencies are not themselves sanitized.
- `make lint CLANG_TIDY=/home/name/.local/bin/clang-tidy`: status 0.
  `clang-format --dry-run --Werror` for all changed C/header files: status 0.
- `make test-date`: 25 real GNU selected-format/adapter checks, status 0.
- `python3 tests/test_date_review.py .`: status 0; independently authored
  arithmetic calendar oracle verifies 30 bounded real-clock standalone/main
  samples, 262 fixed-clock project samples, 256 real GNU fixed-clock adapter
  samples and two deliberately incorrect-byte expectation rejections. Fixtures
  include negative epochs, leap day, DST gap/fold neighbors, year bounds and
  extreme time_t conversion errors. UTC mechanism assertions prohibit localtime
  and tzset calls; each valid non-help process samples CLOCK_REALTIME once.
  GNU's `--date`/format operands are explicitly test adapters, not project scope.
- Five freshly compiled real behavioral mutants were rejected with their intended
  markers: ignored UTC, caller-argv mutation, lost getopt state, accepted invalid
  calendar and ignored output failure. Module state/fault coverage includes
  constructor-NULL/owned OOM, parser pointer permutation, malformed argv, exported
  getopt sentinels, TZ/cwd/fds/signals/stdin, short/EINTR/zero/partial writes,
  failed formatting and stdout-help recovery. Numeric snprintf deliberately
  replaces strftime; no strftime failure claim is made.
- `nm -u build/cmd_date_test.o` inspection supports numeric formatting and absence
  of environment/locale/clock-setting APIs. Source ownership, cleanup, range
  arithmetic, parser/help cardinality and error precedence were reviewed.
- Original fixed shared inventories (1264 and 1237 paths) still match exactly;
  HEAD unchanged and index empty. No shared source/doc writes, staging, commits,
  wiki/AI_LOG/publication or next-command work.

## Diagnosed exceptions and honest limits

1. Global `make format-check` returns 2 identically on pristine HEAD/candidate:
   `src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`, `tests/test_help.c`,
   `tests/test_output.h`, `tests/test_recursive.c`. No formatting expansion.
2. Pristine HEAD O1 sanitizer regression reproduces shell module assertion
   `calls == 1 && interrupt_read == 0`, status 2; its 32 CLI tests pass.
   Default O0 baseline/candidate regressions pass. Optimized full recovery is
   not claimed.
3. Existing `build/test_cd_module --internal-oom` probe exits 1 with
   `Out of memory!`; Argtable internal allocation panic remains distinct from
   tested constructor-NULL and module-owned allocation recovery.
4. Removing only the test-forwarder NOLINT reproduces LLVM's uninitialized
   va_list warning with source-first two-TU analysis at actual Makefile checks.
   Single-TU and reversed-order analysis pass. Adjacent va_start/va_end are valid;
   targeted test-only suppression is justified, not a global analyzer disable.
   The initial broader non-project check probes are retained as setup mistakes.
5. The first independent historical oracle incorrectly equated perpetual modern
   POSIX DST rules with historical IANA New York rules. It failed in 1944 and was
   corrected to use the same explicit IANA TZ; all epochs retained. An initial
   mechanism-fixture marker omission was also corrected and rerun. Neither
   failure required a production change or weakened a production assertion.
6. Local timestamps omit offset and can be ambiguous during DST folds. TZ
   environment contents are preserved, not libc timezone caches. SIGPIPE remains
   inherited; partial output cannot be rolled back. Exported getopt restoration
   is not interrupted-private-scan preservation or concurrent/reentrant parsing.

Raw streams contain authentic compiler whitespace; only explicitly named raw
execution logs may receive a whitespace exception. Source/docs patch whitespace
must pass separately. New report/README/registry links resolve against the exact
candidate. Exhaustive calendar fuzzing, host clock setting and broad GNU date
parsing/format language remain out of scope.

## Frozen reviewed delivery

This report, reviewer test and both review evidence files are added to the original
16-path candidate; README/FILES receive only narrow review links/entries. External
`date-reviewed.patch` and `manifest.json` are authoritative. Their final
normal/sanitizer/lint/scoped-format/reference/independent verification streams and
hashes are stored outside this frozen candidate, preventing evidence self-reference.
The parent must verify the exact authorized index/tree before any commit.
