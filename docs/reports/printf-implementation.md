# printf implementation

## Selected contract (before tests)

Authorized autonomous minimum: formatted output and `-h`/`--help` (course
`data/02/CLI_Tools.docx`, indexed paragraphs 225–227). No JSON requirement.

Syntax: `printf [-h|--help] [--] FORMAT [ARGUMENT...]`. Only leading options
are parsed; after FORMAT every operand is data, including `--`, `-h`, and
`--help`. A leading option-like FORMAT needs `--`. Help has success status;
unknown leading options/duplicate help are errors, even with help.

Keep byte-oriented `%s`, `%c`, `%d`, `%i`, `%u`, `%o`, `%x`, `%X`, `%%`.
Flags `-` and `0` (at most one each), literal widths 0–4096; `-` overrides
`0`. Strings support literal `.PRECISION` 0–4096 (bare `.` means zero);
`0` is rejected for strings/chars, precision is rejected for chars/integers.
`%c` prints the first byte (NUL for empty/missing). Numeric operands accept
optional sign, decimal/octal/hex integers; negative unsigned is rejected.
Empty/missing numeric operands mean zero. Reject whitespace, trailing garbage,
quotes, invalid octal, and overflow outside intmax_t/uintmax_t. Locale never
changes byte conversion; no process environment/locale modifications.

FORMAT escapes: `\\`, `\"`, `\a`, `\b`, `\e`, `\f`, `\n`, `\r`, `\t`,
`\v`, 1–3 octal digits (low byte), and `\x` plus 1–2 hex digits. Reject
unknown/incomplete escapes. Operands for `%s` stay literal, never reinterpreted.
Reuse FORMAT until all arguments are consumed; missing trailing arguments get
empty/zero defaults. A format with no conversions prints once and ignores extra
arguments. Empty format is successful silent output. Validate the entire format
and all consumed operands/output size before any output. Total output at most
16 MiB per invocation; reject excessive output as an argument error. Output
failure can leave a prefix; EINTR/short writes retry, zero writes fail.

Status: 0 success, 1 operational/allocation/output failure, 2 argument/format/
numeric/resource-bound error. Stdin and borrowed descriptors/argv/getopt globals
are preserved. No signal changes; default SIGPIPE can terminate standalone
processes. Restoring exported getopt globals does not restore its private scan
state or promise concurrent calls.

## Manual inventory and selection

Installed `/usr/bin/printf` is GNU coreutils 9.4, manual section 1. Sources:
GNU Coreutils printf invocation manual; installed `man 1 printf`; Argtable
primary tutorial; POSIX write reference. Raw reference retrieval is in evidence.

| GNU manual feature | Decision / rationale |
| --- | --- |
| FORMAT and repeated ARGUMENT, missing string/zero defaults | Keep essential output workflow |
| --help | Keep; add course/project -h |
| --version | Defer metadata-only option |
| %% and diouxXcs | Keep bounded typed integer/string/byte conversion |
| feEgG / floating point | Defer rounding, locale and nonfinite scope |
| Widths and flags | Keep literal width, -/0; defer *, +, space, #, grouping |
| Precision / length modifiers | Keep string precision; defer numeric precision and length modifiers |
| %b / %q | Defer separate operand-escape/shell-quoting languages |
| \", \\, a/b/e/f/n/r/t/v, octal, hex escapes | Keep byte escapes |
| \c | Defer output cancellation |
| \u / \U Unicode escapes | Defer encoding/locale-dependent output |
| Numeric leading quotes / partial conversion / unsigned negatives | Deliberately reject strictly, before output |
| Unknown escapes / extra args without conversions | Unknown escapes reject; extra args ignore silently |
| Streams, status and errors | Stdout bytes, stderr diagnostics; project statuses above, no numeric-error partial output |
| C formats incl. arbitrary types | Reject unsupported formats; never pass untrusted format to variadic printf, especially %n |

## Architecture and TDD plan

Invocation-local Argtable help/FORMAT/ARGUMENT definitions; private argv inserts
an option boundary before FORMAT, preserving caller vectors and exported getopt
state. No dynamic output buffer. Manual typed rendering with dry-run validation,
then descriptor output through existing lines_write. Thin standalone wrapper,
registry and real main dispatch share cmd_printf_spec. No broad refactor.

Vertical slices: runnable missing-output RED → literal/string GREEN; escape
RED → byte-escape GREEN; numeric/width/precision RED → typed-format GREEN;
invalid-input/resource-bound RED → preflight GREEN; integration/fault/reuse
checks and negative controls. CLI tests use fixed independent byte expectations
against actual GNU, standalone and main; project differences/help separate.
Final release: all/test/lint/scoped format; default O0 non-PIE ASan/UBSan;
full exact-patch fresh archive regression; external manifest and preservation.

## Executed verification

Complete [raw streams](../../logs/commands/printf-execution.log) and
[command/status ledger](../../logs/commands/printf-evidence.json) retain every
observed failure and correction. Base HEAD:
`bee5d7b9da4da3e4aa626f76d1a5d467895176fc`. All implementation and verification
used isolated fresh-HEAD archives; mixed shared work was not imported.

| Requirement / gate | Actual evidence |
| --- | --- |
| Literal output, strings/chars, percent, argument reuse/defaults | Runnable no-output baseline fails independent bytes; slice1-green passes. Literal/empty formats and option-like data are covered. |
| Byte escapes | Literal-only module fails slice2; decoding added, accumulated slice2-green passes; every octal byte 0–255 checked. |
| Typed integers, fields and string precision | Existing string renderer fails slice3; bounded typed conversion added, slice3-green passes. Signed minimum/maximum, unsigned maximum, octal/hex, alignment/zero padding, 4096 width/precision and seeded 100-number cases checked. |
| Strict validation before output | Prefix-producing module fails slice4; dry-run preflight added, slice4-green passes. Unsupported conversions (including %n), malformed escapes, invalid numbers, overflow, reuse error and >16 MiB reject with empty stdout. Exactly 16 MiB succeeds. |
| GNU subset | Actual `/usr/bin/printf` 9.4 passes fixed expected bytes, seeded numbers, all octal escapes and every non-NUL operand/literal byte. Project-only help/errors/differences are separate. |
| Main integration | Real previous-HEAD main fails updated discovery/dispatch tests, then build/registry integration passes. Exact builtin inventory/help/parity and remaining capacity reflect 21 builtins/20 standalone wrappers, with cd builtin-only. |
| Main repeated loop and PTY | Successful/error/help/recovery and last-error status pass; child owns controlling PTY with both stdin/stdout attached, exact CRLF bytes and prompts verified. |
| Module state/ownership/faults | 500 success/help/error/literal/empty cycles; forced parser pointer permutation, exact argv pointer/string preservation, getopt sentinels, unchanged environment/cwd/SIGPIPE, borrowed descriptors, unread stdin, four constructor failures, help-constructor faults, owned malloc OOM, EINTR/short/zero/EIO/partial writes, /dev/full help error and reuse recovery pass. |
| Negative controls | Actual compiled no-output and no-preflight mutants fail byte assertions; no-getopt-restore fails PRINTF_GETOPT; direct borrowed-parser argv with actual permutation fails PRINTF_ARGV_POINTER. Untouched production passes afterward. |
| Normal regression | Final `make all test`, `make lint`, scoped clang-format and 80-column checks pass. printf tests are included in make test, without dropping existing tests. |
| Default sanitizers | Fresh candidate and pristine HEAD full O0 non-PIE ASan/UBSan regressions pass, with leak/error halting. Installed static dependencies are not themselves instrumented. |
| Preserved shared work | Fixed inherited 1277-path inventory remains the preservation boundary. Final read-back and HEAD/index checks are recorded in the external manifest; no shared writes, staging or commits. |

### Preserved failures and limits

- First GNU slice included silent-unused-argument expectations. GNU instead warns
  (same success/stdout). These already-approved deliberate differences were moved
  out of the common subset; the original failure and corrected reference pass
  remain recorded. Production behavior was not altered to imitate GNU warnings.
- Initial escape refactor produced a redundant shadowed branch, caught during
  source inspection and removed by the subsequent typed-renderer rewrite. Final
  strict build and lint pass; early compiler streams are retained.
- The first borrowed-argv mutant failed status before reaching pointer assertions
  because it also removed the explicit option boundary before a negative number.
  A plain-string --argv-only harness selection isolates pointer ownership;
  the same real compiled mutant then fails the intended pointer marker.
- Actual Argtable internal malloc failure exits the host with status 1 and
  `Out of memory!`, not an abort as the initial probe expected. Constructor-NULL
  and module-owned malloc recovery are separate passing tests; arbitrary
  dependency-internal OOM recovery is not claimed.
- A pristine-HEAD O0 sanitizer regression initially failed AF_UNIX fixture setup
  because its long TMPDIR exceeded socket pathname capacity. No assertion or
  source was weakened; the full baseline rerun with a short private scratch
  TMPDIR passes. Final exact-patch regressions also use short distinct TMPDIRs.
- Pristine HEAD O1 ASan/UBSan full regression reproduces inherited shell fault-seam
  assertion `calls == 1 && interrupt_read == 0` (status 2). Its real CLI shell
  suite passes. Default O0 regressions pass; optimized full recovery is not claimed.
- Global `make format-check` returns 2 identically for baseline/final candidate:
  `src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`, `tests/test_help.c`,
  `tests/test_output.h`, `tests/test_recursive.c`. Those six inherited files are
  unchanged and excluded. All changed C/headers are scoped-format clean.
- Formats never reach a variadic formatter: rendering uses typed byte conversion
  and existing lines_write only. Help/diagnostics use fixed C formats. No locale,
  environment, cwd, signal or descriptor ownership changes. `%n` cannot write
  memory; literal `%n` operands remain literal data. No floating/Unicode/JSON,
  numeric precision or broad GNU compatibility is promised. Bounded deterministic
  byte-domain/generated tests are not exhaustive arbitrary-format fuzzing.
- Exported getopt restoration does not preserve its private interrupted scan state
  or imply thread-safe concurrent parser calls. SIGPIPE is inherited; output
  failures may leave a prefix. The void usage callback reports allocation failure
  on its supplied stream, rather than returning an allocation status.
- Exact session ID `20261009_130342_9693af` and title
  `Subagent: Implement and execute complete printf…` were verified. Shared
  session-file registration is deliberately excluded to preserve the no-shared-
  edits boundary, consistently with the preceding isolated delivery.

## Frozen review handoff

The report, raw log and ledger are frozen before building the final HEAD-based
patch. That exact patch is applied to fresh archives; final normal/full-default-
sanitizer/lint/scoped-format/reference commands and full streams live outside
this candidate to avoid evidence self-reference. The external manifest records
full SHA256, Git-blob/mode identities, unchanged anchors, installed dependency
fingerprints, fixed preservation inventory, exact 16-path allowlist and patch hash.
Independent review and any later authorized exact-index verification are parent
handoff gates; this task neither stages nor commits. No skipped-command queries/
tests, AI_LOG/wiki access or publication, and no next-command work.
