# rmdir implementation and verification

## Selection before tests

Authorized autonomous scope: rmdir only, no stage/commit/publication/wiki/AI log.
Baseline HEAD: `6c17b709677b67ea8e033b695a040dae4f397844`.
Course SQLite read-only query: CLI_Tools.docx paragraphs 67–70 require empty
directory removal, `-h`/`--help`, `--json`; FileIO slide 80 identifies rmdir.
Session lookup verified `20261009_030917_c9b29a`, title
“Subagent: Implement and execute full verification for… #3”. Registration is
excluded from the delivery allowlist.

Installed reference: /usr/bin/rmdir GNU coreutils 9.4; man-db 2.12.0 rendered
rmdir(1), August 2026; installed info `(coreutils) rmdir invocation` §12.7.
Online GNU manual retrieved directly identifies 9.12; installed 9.4 is the oracle.
Primary references: POSIX Issue 8 rmdir and lstat/stat function pages and GNU
rmdir invocation. Initial direct urllib retrieval succeeded for all three;
a later GNU refetch failed with network-unreachable, while POSIX refetch passed.

| Full documented feature | Task / course | Selection and rationale |
| --- | --- | --- |
| DIRECTORY...; explicit existing empty directories | Safe cleanup; required | Keep ordered multiple operands, continue operational errors |
| No options: no successful stdout, errors stderr | Human/scripts; essential | Keep; project diagnostic wording differs |
| Zero success, nonzero on any error | Scripts; essential | Keep; project 1 operation/output/allocation, 2 syntax/encoding |
| --ignore-fail-on-non-empty | Tolerant cleanup; optional | Defer; failure remains visible |
| -p / --parents; leaf then each ancestor, stops on failure | Ancestor cleanup; optional | Defer; only explicitly named entries |
| -p with ignore option suppresses nonempty ancestor failure/status | Optional interaction | Defer with both options |
| -v / --verbose; per processed directory man, successful removal info | Human feedback; optional | Defer; JSON supplies machine results |
| --help | Required, short alias project addition | Keep -h / --help, parser-derived grammar |
| --version | Reference metadata; optional | Defer |
| -- terminates options; option-like names | Literal paths; essential | Keep; no shell glob expansion in module |
| find -depth / -empty examples | Recursive orchestration | Defer traversal; callers may supply explicit paths |
| --json | Course requirement, not GNU | Keep ordered array of {path,status,errno} |

## Contract and implementation plan

Syntax `rmdir [-h|--help] [--json] [--] DIRECTORY...`. No implicit cwd.
Parse and validate all operands before mutation. Empty paths and invalid UTF-8
JSON paths are argument errors (2); plain mode accepts arbitrary non-NUL bytes.
Valid help permits operands but unknown options remain errors. JSON retains
original literal spellings and duplicates; each object has `path`, `status`
(`ok`/`error`), numeric saved `errno` (0 for success). No partial JSON before
operations; stdout gets one complete array plus LF unless output fails.
Operational failures additionally emit stderr; plain success is silent. Continue
all operands after operational failures; duplicate removal succeeds first then
fails missing. Child-before-parent explicit operands may remove both. A failure
never recreates earlier removals. Allocation and JSON descriptor preflight occur
before effects. Post-removal JSON output failure returns 1 without rollback.
This means a returned write error; default inherited SIGPIPE can instead
terminate the host on a broken pipe. The module never overrides signals.

Strip trailing slashes only for final-entry lstat preflight, never collapse dot,
parent or intermediate components. Reject final symlinks (including trailing
slashes), all final dot/dot-dot and all-slash root operands. Intermediate symlink
components are followed by kernel resolution, including parent traversal.
Reject aliases of this module's current directory by dev/inode. Never call
unlink, recursively enumerate, delete nonempty contents, remove implicit parents,
change cwd/umask/environment/signals, consume stdin, close borrowed descriptors,
or mutate caller argv/getopt exported values. Parent timestamps/link count can
change on successful removal; traversal may update intermediate symlink atime.
Trusted stable namespaces only: preflight/rmdir is not race-free; no hostile
namespace, durability, concurrent cwd changes or rollback guarantee. Kernel
rmdir decides emptiness at operation time. No retry of ambiguous failures.

Files: module/header/thin wrapper, Python CLI and C module harness, narrow
Makefile/registry/shell inventory/tests; concise usage and this report; one raw
execution log and structured ledger. Mixed README/FILES candidates start at HEAD
in scratch, not wholesale working-file replacements. Disposable scratch fixtures
only. No later command implementation.

Ordered slices: (1) empty/nonempty/type/error continuation; (2) parser/help/JSON
and byte encoding; (3) final-link/dot/cwd safeguards and literal resolution;
(4) borrowed state, allocation/syscall/output fault seams and repeated reuse;
(5) actual registry/discovery/dispatch/help/parity/capacity/loop/PTY integration.
Behavior slices 1–3 and registry dispatch have runnable RED then GREEN; module
fault/ownership tests characterize the implementation and use behavioral mutants
for sensitivity, not a claimed pre-fix production TDD cycle. Independent expected
fixtures, bad executable/no-rmdir/argv mutants establish harness sensitivity.
Full all/test/lint/scoped-format, fresh O0 ASan/UBSan and exact HEAD-patch checkout
verification required; inherited global formatting/O1 faults separately reported.

## Execution results

Executed commands and complete stdout/stderr: `logs/commands/rmdir-execution.log`;
ordered status/hash ledger: `logs/commands/rmdir-evidence.json`. The delivery
finalizer independently matched all 86 existing ledger entries to their raw
streams and verified both stream hashes and recorded process statuses.

| Check | Observed result / ledger labels |
| --- | --- |
| RED → GREEN behavior | `slice1-red`, `slice2-red`, `slice3-red` exit 1; corresponding GREEN exit 0; registry RED exit 1 followed by corrected integration GREEN exit 0 |
| Project CLI | `final-expanded-focused`: 12 tests passed, no skips; includes all permitted one-byte components, Unicode/invalid UTF-8, real permissions/name limits and inherited broken-pipe policy |
| Module ownership/faults | Same focused run: four constructor seams, three owned allocation seams, three preflight seams, 12 injected rmdir errors, real nonempty race, output recovery, borrowed state/resources and 500 success/error/help reuse loops passed |
| Real main integration | `integration-corrected-green`: 33 shell tests passed, including rmdir discovery, help/parity, quoted paths, loop continuation and child-owned PTY; registry/ownership C harness passed |
| GNU selected common subset | `final-reference`: 12 discovered, seven passed, five explicitly skipped project-only JSON/parser, cwd protection and signal-policy cases |
| Fresh HEAD plus candidate | `fresh-default-all-test`, `fresh-default-lint`, `fresh-default-scoped-format`: exit 0; patch application and source/document whitespace checks exit 0 |
| Fresh sanitizer regression | `fresh-O0-ASanUBSan-full`: complete `make all test` exit 0 with O0, ASan/UBSan, leak detection and halt-on-error enabled |
| Negative controls | no-rmdir exit 1 with `RMDIR_NOT_REMOVED`; borrowed argv/getopt/JSON mutants each SIGABRT (-6) with `RMDIR_ARGV_POINTER_CHANGED`, `RMDIR_GETOPT_CHANGED`, `RMDIR_JSON_LENGTH`; production GREEN exit 0 |

The earlier `registry-green` and `full-regression` failed because the standalone
parity fixture omitted rmdir; `full-lint` caught insecure strcpy in the test
harness. These are retained failures, not passing evidence. The corrected
parity fixture and bounded test copy passed subsequent full regression/lint.

### Known failures and limits

- Global `make format-check` exits 2 both in the working tree and unchanged HEAD.
  The inherited six paths are `src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`,
  `tests/test_help.c`, `tests/test_output.h`, `tests/test_recursive.c`; none is
  changed by this delivery. Scoped formatting passes.
- O1 ASan/UBSan shell fault harness aborts (-6) on
  `calls == 1 && interrupt_read == 0` in both HEAD and candidate. O0 full
  sanitizer regression passes; no claim of an O1 full-suite pass.
- `internal-library-oom-characterization` exits 1 with `Out of memory!` before
  returning to its host. Constructor-NULL and module-owned OOM recover, but
  argtable's internal allocation failures may terminate the process.
- Exported getopt values are restored, not its private interrupted-scan state;
  parsing is not thread-safe. Namespace/cwd concurrency, durability, default
  SIGPIPE termination and post-removal output rollback remain outside guarantees.
- Source/document whitespace checks exclude only the raw execution log; exact
  observed raw bytes are retained, including diagnostic trailing whitespace.

### Frozen delivery gate

The final exact-patch regression uses a fresh HEAD archive, never a working-tree
build substitution. README/FILES versions are narrow scratch candidates based
on HEAD; unrelated mixed working edits and session records are not delivered.
Candidate report/log/ledger bytes are frozen before that regression. Its own
streams, results, source/dependency/mode fingerprints and preservation proof
are external at `/home/name/.hermes/cache/scratch/rmdir-state/manifest.json`
with `exact-execution.log` and `exact-evidence.json` beside it. This separates
preliminary observations above from the final gate and avoids recursive hashes.
No denied course lookup was retried by the finalizer. Independent review,
exact-index verification, staging and any commit belong to the parent; none is
claimed completed here.
