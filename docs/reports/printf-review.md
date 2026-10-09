# printf independent review

Verdict: **PASS for the selected bounded byte-format contract.** No production
security or logic defect was demonstrated; no production fix was necessary.
Base: `bee5d7b9da4da3e4aa626f76d1a5d467895176fc`.

## Reviewed scope

The implementation patch was authenticated before execution: 16 delivered
SHA256/Git blobs and modes, 630 unchanged HEAD anchors and 14 installed dependency
fingerprints. Review used fresh HEAD archives plus the actual patch, never mixed
shared sources. Reviewed typed conversion, strict overflow checks, field bounds,
all-input preflight, literal untrusted operands, byte escape cursor bounds,
reuse/defaults, Argtable allocation/cleanup, private argv and exported getopt
restoration, descriptor borrowing and failure propagation. User format strings
never reach a variadic formatter; `%n` is rejected before output.

Only printf integration hunks in Makefile/registry/shell tests/docs were present.
The added review test/report/streams/ledger and narrow README/FILES references
are review evidence, not expanded command functionality.

## Independent execution

Full [streams](../../logs/commands/printf-review-execution.log) and
[status/hash ledger](../../logs/commands/printf-review-evidence.json) preserve
actual positive gates and intended negative-control failures.

| Gate | Result |
| --- | --- |
| Fresh patch-applied `make all test` | 0 |
| `make lint` | 0 |
| Scoped changed-C/header `clang-format --dry-run --Werror` | 0 |
| `make test-printf`, actual GNU coreutils 9.4 | 0 |
| Fresh patch-applied full O0 non-PIE ASan/UBSan `make all test` | 0 |
| Independent constructed numeric/byte oracle, GNU | 418 valid cases pass |
| Same oracle, standalone and real main | 418 valid + 119 invalid cases per entry point pass |
| Same oracle, sanitizer standalone and real main | Same cases pass |
| Recompiled no-output mutant | Own `PRINTF_REVIEW_BYTES` assertion, status 1 |
| Recompiled no-preflight mutant | Own `PRINTF_REVIEW_PREFLIGHT` assertion, status 1 |
| Recompiled no-getopt-restore mutant | `PRINTF_GETOPT`, SIGABRT (-6) |
| Recompiled borrowed-argv mutant, forced real parser permutation | `PRINTF_ARGV_POINTER`, SIGABRT (-6) |
| Unchanged production after negative controls | Module and own oracle pass |
| Pristine HEAD full O0 non-PIE ASan/UBSan regression | 0 |

The independent oracle constructs expected bytes without the production parser
or reference output: decimal/octal/hex operands, signed limits, unsigned maximum,
zero-padding/sign placement, both -/0 flag orders, byte string truncation and
padding, bounded octal/hex escape consumption, multibyte data, format reuse and
missing operands. Adversarial cases include all integer conversion families,
base-specific overflow, whitespace/garbage, malformed and forbidden directives,
very long field/count text, invalid later reuse and output-limit preflight.

Existing full tests additionally execute every non-NUL literal/operand byte,
all 256 octal bytes, exact 16 MiB success and larger failure, help/`--`/locale/
POSIXLY_CORRECT behavior, /dev/full, registry capacity and main/standalone help
parity, repeated main pipe and child-owned controlling-PTY prompts/statuses,
500 module reuse cycles, invalid vectors, stdin/fd/argv/getopt/cwd/environment/
signal invariants, constructors/owned OOM and EINTR/short/zero/EIO/partial writes.

Reproduce independent checks from the checkout root:

```sh
python3 tests/test_printf_review.py . --reference
python3 tests/test_printf_review.py .
```

Sanitizer flags are default `-O0`, `-fsanitize=address,undefined`,
`-fno-omit-frame-pointer -fno-pie`, link `-fsanitize=address,undefined -no-pie`.
Full runs set `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and use separate short private
scratch TMPDIRs. Installed dependency archives are not instrumented.

## Exceptions and evidence integrity

- Global format-check returns 2 on both candidate and pristine HEAD, confined to
  unchanged `src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`,
  `tests/test_help.c`, `tests/test_output.h`, `tests/test_recursive.c`.
  Scoped changed-C/header check passes; inherited files were not reformatted.
- Pristine HEAD O1 ASan/UBSan reproduces shell seam assertion
  `calls == 1 && interrupt_read == 0`, status 2. The fresh O0 baseline passes.
  Optimized full sanitizer recovery is not claimed.
- Actual installed Argtable internal-malloc probe exits 1 with `Out of memory!`.
  Constructor-NULL/module-owned OOM recovery is separate; dependency-internal
  OOM recovery, thread safety and private interrupted getopt state are not claimed.
- First review setup used the wrong dependency base (`REPO/argtable3` rather than
  `REPO/build/argtable3`) and failed authentication with FileNotFoundError before
  tests. Corrected harness then authenticated fresh checkouts and executed all
  gates. This was a reviewer setup error, not a production failure.
- Original worker logs (72 records) and exact-patch log (12 records) were each
  parsed and independently authenticated against every stdout/stderr hash;
  historical source/evidence hashes match the worker's frozen candidate.
- Raw streams retain authentic inherited whitespace. Only the two explicit raw
  log paths are exempt; source/docs/JSON patch whitespace must be clean.
- Fixed shared inventory: 1,277 paths, zero differences; HEAD unchanged, index
  empty. Session `20261009_133443_2f884f`, title `Subagent: Independently review
  and execute verification… #10`, verified; no shared registration write.

Review evidence freezes before final patch generation. Final actual-patch normal,
sanitizer, lint/format/reference/own-oracle streams and identities are recorded
outside the candidate in the review scratch handoff/manifest, preventing
self-referential hashes. Exact-index verification and any commit remain parent
handoff gates. No staging, commit, shared edits, skipped-command tests/queries,
AI_LOG/wiki changes or next-command work occurred.
