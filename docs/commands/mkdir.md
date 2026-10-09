# mkdir: inventory, contract, and report

Status: implemented and verified below; awaiting independent review.

Manual: installed `man 1 mkdir`, GNU coreutils 9.4, August 2026 footer,
rendered on 2026-10-07. Course source: `data/02/CLI_Tools.docx`,
queried through `docs/data.sqlite`. Selection uses the user's autonomous
continuation authorization. Author/copyright/support sections are provenance.
Use argtable3 and cmd_spec_t, standalone wrapper, status 0 success,
1 operational failure, 2 invalid arguments. Parse fully before side effects.
Keep human -h/--help and course --json separate from GNU reference tests.
No shell registry exists. No commits or pushes.

Course paragraphs 61–65: creation, -p and JSON.

| Manual feature | Decision and reason |
| --- | --- |
| DIRECTORY... operands, create directories | Keep one or more; continue independent operand failures |
| -p/--parents, existing-directory success | Keep course-required nested creation |
| -m/--mode=MODE | Defer mode language; default 0777 filtered by process umask |
| -v/--verbose | Defer extra text; JSON provides results |
| -Z, --context[=CTX] | Defer security labels |
| --help | Keep; add course -h |
| --version | Defer packaging metadata |
| --json (course addition) | Keep structured results |

Contract: `mkdir [-p] [--json] DIRECTORY...`; without -p an existing entry
fails, with -p existing directories succeed but non-directories fail.
Default directories use 0777 filtered by inherited umask, with GNU-compatible
parent creation semantics; never change process-wide umask or cwd.
Symlinks to existing directories can be traversed as normal filesystem paths.
Preserve '.', '..', absolute and repeated-slash path semantics.
Text success is silent. JSON emits an array of {path,status,message}, status
"ok" or "error"; all paths must be valid UTF-8 before mutations. Errors still
diagnose to stderr; overall status1 if any operation fails. No rollback promise.

Tests first: create one/multiple, nested -p, existing directory/file/symlink,
missing parent, dot/dotdot and repeated slashes, mode under controlled umask,
partial success, literal dash names, validation before mutation, JSON escaping,
output failure, module reuse/cwd/umask preservation. Native mkdir oracle checks
side effects, modes and statuses rather than success stdout alone.

## Selected implementation plan (2026-10-08)

Autonomous implementation is authorized for mkdir only. The installed GNU
coreutils 9.4 manual was rendered again; local info `(coreutils) mkdir
invocation` and `man 2 mkdir` supplement it. Read-only SQLite verified
CLI_Tools.docx paragraphs 61–65: creation, -h/--help, -p, --json.

- One or more operands, no fixed operand-count limit beyond argc/memory.
  Empty paths are operational errors. `--` supports literal dash filenames.
  Options parse before any mutation; unknown/deferred/repeated flags are errors
  (each supported flag at most once). Help needs no operand, but cannot mask
  parse errors; valid help suppresses all creation, even with operands.
- Default final mode is 0777 filtered by inherited umask. Newly created
  intermediate parents additionally gain owner write/search (0300), matching
  GNU -p, without changing the process umask. Existing directories are never
  chmod'ed. Kernel ACL/setgid inheritance remains effective; ACL equivalence
  and hostile concurrent path replacement are not promised.
- Without -p final symlinks fail as existing entries. With -p links to
  directories succeed/traverse; dangling links, files, loops and missing
  non-parent components fail. Dot/dotdot, absolute/repeated/trailing slashes
  are resolved by the kernel, not lexically normalized.
- Continue later operands after filesystem errors; keep all completed work.
  A failed parent chain can leave earlier new parents (including adjusted
  modes). No rollback on allocation, syscall, JSON or output failure.
- JSON exact layout is compact array plus LF, one result per operand in order:
  {"path":STRING,"status":"ok","message":null} or status "error" with
  strerror(errno) message. Filesystem errors also diagnose to stderr.
  Validate UTF-8 for every JSON path before creating anything; invalid UTF-8
  is status 1, no stdout/mutations. Non-JSON accepts raw filename bytes.
- Returned output errors are status 1; JSON may be partial and creations survive.
  SIGPIPE follows the inherited policy: default termination, or returned EPIPE
  with status 1 when an embedding caller ignores SIGPIPE.
  Silent text success need not touch stdout. Help flushes borrowed stdout.
  Borrow stdin/stdout/stderr, preserve argv/cwd/umask/signals; own and free
  parser, argv copy, JSON error storage, and temporary path copies.
- No actual registry exists: exported cmd_spec plus thin wrapper is the
  integration seam, not shell registration. No invented registry helper.

Requirement matrix: `tests/test_mkdir.py` covers exact CLI output/fixtures,
reference-compatible filesystem/mode results, argument prevalidation, UTF-8,
symlinks, partial progress and output faults. `tests/test_mkdir_module.c`
covers repeated calls, borrowed descriptors/stream positions, argv, cwd,
umask/signals, allocation/syscall/write injection and cleanup/recovery.
Ordered slices: basic creation; parents/modes; JSON; parser/help/validation;
module failure/reuse checks. Each new slice requires an observed behavioral
RED from a runnable baseline followed by GREEN. Full regression, build,
clang-tidy, scoped clang-format and ASan/UBSan complete delivery evidence.

## Results

### Interruption and recovered state

The previous worker stopped on a provider timeout, not a permission blocker,
on 2026-10-08 at 16:17:19 (its last saved action was at 16:11:49).
Inspection found the selected plan, CLI fixtures, a working build rule,
header/wrapper, and a runnable no-op module. Saved prior logs show six GNU
basic fixtures passed, while the `/usr/bin/true` negative control and no-op
project baseline failed the directory-existence assertion. They do not show
a completed implementation or final verification; none is inferred.
Original transcript, research/manuals and prior logs are preserved in
`logs/commands/mkdir-20261008-interrupted.txt`; original files are untouched.

### Observed TDD and harness strength

Resume executions are retained verbatim, with commands and actual statuses,
in `logs/commands/mkdir-20261008-execution.txt`.

| Evidence section | Actual observation |
| --- | --- |
| 00-baseline | GNU 25 fixtures passed; true and project no-op failed existence assertion |
| 01-basic-green-parents-red | Basic 6 passed; nested/existing -p failed status assertion |
| 02-parents-green-json-red | Parent/path/mode 25 passed; JSON failed exact stdout assertion |
| 03-json-green-all | JSON accumulated 29 and original full project 41 passed |
| 10-help-parity-red | Help depended on argc; new byte-parity regression failed |
| 11-help-parity-green | Logical grammar rendering fixed parity; accumulated tests passed |
| 21-final-negative-control | Deliberately wrong true executable failed existence assertion, exit 1 |

Parser/error/reuse and later strengthened tests are characterization/fault
checks, not falsely claimed as additional RED-GREEN implementation slices.
The invocation-local argtable defines options once. Help temporarily renders
the logical one-or-more grammar instead of its argc-sized storage bound;
public usage and CLI help are byte-identical. The module copies argv pointers
before parsing, prevalidates all JSON names, and retains errno per operand.
Intermediate parents are created, then gain owner write/search only if newly
created; process umask/cwd are never changed. No shared helper was modified.

### Final verification

| Exact command / log section | Result |
| --- | --- |
| `make test-mkdir` (24-delivery-regression-build) | GNU coreutils 9.4: 26 independent common-subset fixtures passed |
| `make test-mkdir-impl` (23-pipe-policy-fixtures) | 55 independent project CLI fixtures and module suite passed |
| `make test && make all` (24-delivery-regression-build) | Full working-tree regression passed; all delivery binaries built |
| `make lint` (25-delivery-lint-format) | clang-tidy passed all 27 C sources |
| `clang-format --dry-run --Werror src/cmd_mkdir.c src/cmd_mkdir.h src/mkdir_main.c tests/test_mkdir_module.c` | Scoped formatting passed |
| ASan/UBSan commands (26-delivery-sanitizers; full invocation in log) | All 55 CLI fixtures and module suite passed; leak detection enabled, no findings |
| `make format-check` (15-final-global-format) | Failed only the seven inherited files listed below |

CLI fixtures cover raw/control/Unicode names, literal dash names, 100
operands, no-parent/existing/file/symlink/dangling/loop errors, dot/dotdot,
absolute/repeated/trailing slashes, restrictive umasks, preserved existing
modes, setgid/group inheritance, partial chains and later success, exact JSON
and help bytes, invalid Unicode scalar encodings before any mutation,
unknown/deferred/repeated flags before any mutation, real permission denial,
/dev/full, and a real broken pipe. Sentinel file bytes and symlinks survive.

Direct cmd_spec dispatch covers 80 repeated success/error/help calls, copied
argv pointer/string preservation, stdin position, open borrowed descriptors,
cwd/umask/SIGPIPE disposition preservation, CLI/public-usage parity, and
recovery after failure. Link-time seams inject NULL parser entries, own
malloc/calloc/strdup failures, mkdir ENOSPC, stat/chmod errors, interrupted,
short, zero-progress and failed writes. Every returning invocation releases
all tracked allocations; sanitizers supplement this ownership check.
Preflight allocation failures do not create a fresh operand; failing parent
metadata/chmod leaves the independently expected partial mode, with later
operand success. Ignored SIGPIPE produces returned EPIPE/status 1 without
changing the caller's signal policy. Silent success does not write stdout.

### Limits and review readiness

- A real low-level malloc fault inside installed argtable3 terminated the
  process with exit 1. GDB traced it to dependency `panic()` in `arg_lit0`
  (05–07 evidence). This is NOT recoverable module allocation handling.
  Later tests explicitly scope internal library allocations away from the
  recoverable own-allocation seam, while still tracking their normal cleanup;
  constructor-NULL handling is tested separately. Fatal dependency OOM remains
  an embedding limitation; do not claim graceful recovery from every OOM.
- Global format failures remain in `src/main.c`, `src/traverse.c`,
  `src/cmd_spec.h`, `src/traverse.h`, `tests/test_help.c`,
  `tests/test_recursive.c`, and `tests/test_output.h`. They were not reformatted.
- No live shell registry exists in src. The exported specification, thin
  standalone wrapper, and direct specification tests are the real integration
  seam; shell discovery/enumeration is not claimed. FILES.md is updated.
- Sanitized command/shared-helper/test code links the existing unsanitized
  argtable3 archive. ACL equivalence, hostile concurrent path replacement,
  allocation failures inside that archive, threaded/reentrant parser use,
  broad fuzzing/coverage measurement and non-C diagnostic locales are not
  established. These are outside the course-minimal subset, except fatal
  dependency OOM remains explicitly flagged for independent review.
- JSON error messages use inherited-locale strerror, not a stable translated
  message schema; exact error expectations use LC_ALL=C. Returned error
  diagnostics have no promised localized byte-for-byte GNU equivalence.
- Working Makefile and FILES.md retain older unrelated uncommitted integration.
  Shared cmd_spec/cmd_lines were left untouched. This verifies the working
  tree, not an isolated prospective commit; parent staging/commit review must
  construct and verify its intended tree separately.

Ready for independent review with these limitations, not an assertion that
every repository-wide gate is green. No touch work, staging, commit, push,
publication, wiki update, or AI_LOG content read was performed.


