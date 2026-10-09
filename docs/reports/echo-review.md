# Echo independent review

**Verdict: selected contract passes; no blocking source defect found.** Reviewed
input: HEAD `71c6e49a4be299db071f0a2a1fc6ef411e343a5e` plus the worker's
16-path patch SHA-256
`7873ebc8e3bb2eecaace3dbe8a1ca28581cdad8d97f7c31de47bc70f5db78b5c`.
No shared working files, index, commits, skipped command or publication changed.

## Review and execution

Read the complete module and narrow integration hunks. Private option grouping
preserves operand order and the first `--` boundary; Argtable still validates
names/cardinality. Logical help cardinality is restored after rendering. Every
module allocation is freed; literal emission uses the unchanged EINTR/short-write
helper. Invalid module vectors are rejected before parsing. No secret/injection
finding in added source lines. New echo documentation links resolve in the
HEAD-based candidate. Historical 18-record raw streams/ledger hashes authenticate.

| Actual gate | Result |
| --- | --- |
| `make all test` in fresh normal checkout | 0; full regression, project 528 checks, module 500 reuse cycles |
| Same full suite with O0 ASan/UBSan, non-PIE, leak/error halting | 0 |
| `python3 tests/test_echo.py /usr/bin/echo --reference` | 0; 467 retained-subset checks |
| `python3 tests/test_echo_review.py .` in normal and sanitizer trees | 0 each; reviewer-owned 3,895 project checks and 263 independent GNU fixtures |
| `PATH=/home/name/.local/bin:/usr/bin:/bin make lint` | 0; inherited suppressed diagnostics only |
| Scoped `clang-format --dry-run --Werror` on changed C/header files | 0 |
| Non-log patch `git apply --check --whitespace=error-all` | 0 |

The reviewer oracle exhausts vectors of length 0–3 over eight explicit grammar
classes, separately checks every non-NUL byte after `--`, and tests standalone
and real-main dispatch with unset/synthetic `POSIXLY_CORRECT`. Exact independent
help bytes do not come from the implementation. Large literal/vector fixtures
run unchanged against GNU, standalone and main. GNU differences are not treated
as reference compatibility failures. Existing regression separately verifies
registry capacity, discovery/parity, 100 mixed main loops and exact child-owned
PTY prompts/output.

Four compiled module controls deliberately remove caller-vector isolation,
getopt restoration, help-error clearing and final LF. All abort with their
intended `ECHO_ARGV_POINTER`, `ECHO_GETOPT`, `ECHO_HELP_ERROR_REUSE` or
`ECHO_OUTPUT_LENGTH` marker. `/usr/bin/true` substituted as the reference fails
the reviewer oracle's `GNU_REFERENCE_BYTES` assertion. No generic crash or failed
control compilation is counted as sensitivity evidence.

## Exceptions and remaining gates

Global `make format-check` returns 2 on both unchanged HEAD and candidate with
identical diagnostics for six paths: `src/cmd_spec.h`, `src/traverse.c`,
`src/traverse.h`, `tests/test_help.c`, `tests/test_output.h`,
`tests/test_recursive.c`. O1 sanitizer `make test-shell` reproduces the unchanged
HEAD injected-read assertion `calls == 1 && interrupt_read == 0`; this optimized
gate is not claimed passing. Internal Argtable malloc failure prints
`Out of memory!` and exits 1 before echo returns. Constructor-NULL/owned-allocation
recovery does not imply dependency-internal OOM recovery or instrumented libraries.

Inherited SIGPIPE, valid C-string storage, private interrupted getopt state,
thread safety and massive module vectors retain the [contract's limits](echo-implementation.md).
Initial restricted-PATH lint could not find clang-tidy; explicit installed PATH
resolved it. The first reviewer baseline marker expectation was wrong, not a new
source failure; the actual assertion and corrected repeat remain in evidence.
Raw execution whitespace is a declared evidence-only exception, not reformatted.

[Raw streams](../../logs/commands/echo-review-execution.log) and
[command/status/hash ledger](../../logs/commands/echo-review-evidence.json)
are frozen before final exact-patch replay. That replay's streams, complete
SHA-256/Git-blob/mode manifest, unchanged anchors, installed-dependency hashes and
fixed 1,237-path shared preservation audit are external scratch artifacts to avoid
self-reference. Authorized exact-index verification and any later commit remain
parent-orchestrator gates; this review does neither.
