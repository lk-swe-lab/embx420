# cp implementation and verification

Implemented in the working tree; **not staged or committed**. Independent review
and the parent's exact-index delivery gate remain separate. Base HEAD:
`c18b7e83d4661089c9984fe5e6391e4c0a88e9d9`.

## Selection and references

The autonomous selection was recorded in `.tmp/cp-contract.md` **before tests**.
Indexed course lookup: `docs/data.sqlite`, `lecture_content` joined with
`lecture_sources`; `data/02/CLI_Tools.docx` paragraphs 41–46 require cp,
`-h/--help`, `-r`, `-f`, and structured success/error summaries. File I/O slide 59
illustrates mmap/memcpy/msync. Installed reference: `/usr/bin/cp`, GNU coreutils
9.4, `cp(1)`; installed `mmap(2)` and `msync(2)` researched as well.
[Raw manual/course research](../../logs/commands/cp-implementation/research.stdout).
The [historical complete option inventory](cp.md) is byte-preserved, not rewritten
as current evidence. Its proposed decisions are superseded by the final contract
below. An official man7 search was saved; extraction failed because the configured
backend is search-only. Installed rendered manuals supplied the API references.

Keep: the required four flags, regular-file copying, directory destinations,
recursive trees, sizing/same-inode safety and predictable failures. Defer all
other GNU options in the historical inventory: multi-source, -R, -t/-T, backup,
archive/preserve, interactive/no-clobber/update, sparse/reflink, hard/symbolic-link
creation modes, attributes-only, parents, filesystem/security-label/version and
verbose/debug extensions. They are not needed for the minimum course workflow.

## Final contract

- Exactly `SOURCE DEST`; options `-h/--help`, `-r/--recursive`, `-f/--force`,
  `--json`, and `--`. `-` is a literal path. Empty paths, malformed argv,
  invalid options/cardinality and non-UTF-8 JSON operands return 2 before changes.
  Valid help never copies. Unknown options remain errors even with help.
- Existing directory DEST receives SOURCE basename (trim trailing slashes for
  basename only; the kernel still resolves the original source operand).
- Regular data uses page-aligned windows up to 1 MiB: source MAP_PRIVATE/READ,
  destination MAP_SHARED/READ|WRITE, memcpy, MS_SYNC, unmap. The window size is
  rounded down to the runtime page size; page sizes above 1 MiB are rejected.
  No read/write/sendfile/copy_file_range/reflink content fallback. Empty files
  are sized to zero and never mapped. Destination requires read **and** write
  permission, an intentional difference from GNU's write-only overwrite.
- Source descriptor fstat and destination descriptor fstat type/device/inode
  checks precede ftruncate. Destination open never uses O_TRUNC. Same-inode
  aliases fail unchanged, including hardlinks and -f. Default overwrite retains
  destination inode/hardlink effects and ordinary rwx mode; special permission
  bits may be cleared by the kernel. New files use source rwx and inherited
  umask, without copying special bits, ownership, times, ACLs/xattrs or topology.
- Without -r, source links follow to regular files; dangling links fail. With
  -r, source links (including dangling links) are copied as links, not traversed.
  Regular destination links follow only existing regular referents; dangling
  links fail even with -f. Recursive directory destinations must be real dirs,
  not symlinks. Intermediate path links are resolved by the kernel.
- -f retries only a failed destination open by removing/recreating an existing
  regular non-symlink destination. It does not remove after ftruncate/mmap/sync
  errors and never removes directories. Existing destination hardlinks to the
  removed inode keep their previous contents. Failed recreation can leave no file.
- -r merges directories, includes dotfiles/empty dirs, preserves existing dir
  rwx modes, and limits directory descent to 128 levels. Newly created directory
  final rwx uses source mode and umask; owner access is temporary during filling.
  Ancestor inode-cycle checks and resolved destination-ancestor self-tree checks
  precede root creation. Sibling copies continue after child failures; there is
  no rollback. Unsupported special files fail without opening/consuming them.
- Success status 0; operational/resource/output failure 1; argument error 2.
  Normal success stdout is empty; failures have stderr diagnostics. JSON emits
  one UTF-8 object, exactly `source`, `destination` and `status` (`ok`/`error`),
  followed by newline. Paths are original operands, not resolved destinations;
  status summarizes the entire operation, not per-child effects. Invalid JSON
  path encoding is rejected before mutations. Output failure can follow a
  completed copy and returns 1 without undoing filesystem effects.
- Invocation-owned argtable entries and copied argv pointer array; exported
  getopt values restored after parse. Borrowed argv strings/pointers, cwd, umask,
  SIGPIPE disposition and inherited standard descriptors stay unchanged.
  No claim of thread-safe parsing or private interrupted-getopt-scan preservation.
- Owned fds/mappings released on normal and tested error returns. Linux close
  is not retried. open/ftruncate/msync EINTR are retried. A munmap failure gets
  one cleanup retry but still reports the initial error; persistent munmap failure
  cannot guarantee release. Internal argtable allocation panic/exit behavior
  and silent allocation failure in the void usage callback remain inherited
  limitations, not universal recoverable-OOM claims.
- Failures can leave resized/partially copied destinations and partial trees.
  No atomic replacement, transaction, durability guarantee, sparse-allocation
  preservation or rollback. Source atime may change through reading/enumeration.
  **Stable, trusted trees only**: pathname/type prechecks do not eliminate
  namespace races. Concurrent source/destination changes are unsupported;
  mmap access after source truncation or storage faults can raise SIGBUS and
  terminate the containing shell. No command signal handler is installed.

## Incremental TDD (observed)

Raw outputs and numeric status files are in
[cp evidence](../../logs/commands/cp-implementation/).
[Executed command ledger](../../logs/commands/cp-implementation/execution-commands.json)
records the core commands. These are real failures/passes, not inferred cycles.

| Slice | RED observation | GREEN observation |
| --- | --- | --- |
| Regular content | Runnable stub returned 1 instead of success | Independent bytes/empty/overwrite/windows/same-inode suite passed |
| Parser/JSON | Missing operands returned 2 without required diagnostic | Help, arity/options/encoding/no-mutation and JSON passed |
| Destination/link/types | Directory destination returned EISDIR | Basename routing, types, symlinks, modes/umask and -f samefile passed |
| Recursive tree | -r directory rejected as EINVAL | Nested/empty/dot/link trees, self-tree guards, modes and partial errors passed |
| Fault recovery | Injected EINTR open returned failure instead of success | Source/dest open, sizing/sync interruption and mmap/unmap/close/allocation cleanup passed |
| Actual main | cp unknown and exact discovery inventory failed | Main discovery/help/dispatch/parity/reuse and count/capacity passed |
| Help grammar regression | Usage redundantly advertised optional PATH operands | Separate SOURCE/DEST definitions give stable exact grammar |

Reference fixtures use independent expected bytes/filesystem states, never
reference-generated expectations. `/usr/bin/true` fails the empty overwrite
assertion; a no-memcpy mutant fails binary content; a parser-borrowed-argv mutant
fails caller-pointer identity under intentional parser-array permutation.
These controls are saved separately from implementation REDs.

Fixture corrections are **not** production TDD: initial GNU FIFO tests timed out
because GNU can copy special-file content; those project-only cases were removed
from its common subset. Initial force inode inequality failed both implementations
because the filesystem immediately reused a freed inode; retaining an original
hardlink made inode replacement and unchanged old data observable. A fresh-run
executor first stopped before launch for a missing Python os import; it was
corrected and both runs were executed, then rerun on the final test bytes.

## Acceptance evidence

`tests/test_cp.py` has seven grouped tests; reference excludes project-only
help/JSON/special-file/depth/error-policy assertions, rather than pretending
GNU implements the project contract. Coverage includes:

- Binary bytes and sizes 0, 1, 4095, 4096, 4097, 1048575, 1048576,
  1048577, 8388641; new and existing targets, shorter overwrite and same inode.
- 64 MiB + 1 sparse source with boundary sentinels, exact size and SHA-256
  equality; copy succeeds under 32 MiB virtual-address cap. The cap is not
  applied to sanitizer processes (ASan needs a huge virtual reservation); the
  same large copy still runs under sanitizers. A file-size resource cap with
  child-local SIGXFSZ ignored verifies ftruncate failure leaves old content.
- Directory basenames/trailing slash, recursive merge/retained unrelated files,
  read-only source-dir mode and final directory umask/mode; 129-level rejection.
- Source/destination/dangling links, link cycles preserved as links, same link,
  hardlinked destination inode effects, FIFO rejection without blocking.
- Nonroot permission failures (actual uid 1000), successful force replacement,
  unchanged original hardlink, locked-parent denial, no source-content/mode/mtime
  mutations across 40 seeded unusual-name fixtures, JSON escaping/encoding.
- `/dev/full` JSON/help output errors, parse errors/no changes, missing inputs,
  recursive special-file partial progress and post-error reuse.

`tests/test_cp_module.c` verifies actual mmap call count, bounded/page-aligned
mappings, no content read/write fallback, empty files without maps, constructor
and owned argv allocation failure, both mmap failures, ftruncate/msync/munmap/
close errors, source/destination open and sizing/sync EINTR, unchanged content
on sizing failure, zero tracked owned fds/maps, argv/getopt/cwd/umask/signal/
standard-fd preservation, invalid module arguments, and 100 repeated successes.
Parser reordering is deliberately induced to prove test sensitivity, not a
claim that argtable normally reorders caller arrays.

Actual `main` tests verify 11-command help inventory, cp callback/standalone help
parity, binary copying, quoted JSON destination, operational status propagation,
script continuation without stdin consumption and repeated success/error/help.
Registry module tests retain fixed 32 capacity and update builtin count to 11;
all existing builtin parity cases remain present.

| Final check | Actual result / raw evidence |
| --- | --- |
| `make test-cp-impl test-cp test-shell` | 0; final-targeted stdout/stderr |
| `make test` | 0; delivery-regression stdout/stderr |
| Fresh HEAD + exact source overlays: `make clean all test` | 0; final-fresh-normal.* |
| Fresh full ASan+UBSan, default -O0, leak detection | 0; final-fresh-sanitizers.* |
| `make lint` | 0; final-lint.* |
| Scoped clang-format dry run | 0; final-scoped-format.* |
| `make format-check` | 2; six inherited paths, format-global.* / format-inherited-paths.json |
| `git diff --check` scoped to implementation integration/docs | 0; delivery-diff-check.* |
| Actual help/discovery | 0; delivery-help.* |
| mmap mechanism | strace and module assertions; mechanism-filtered.txt / mechanism-symbols.stdout |
| Concurrent source truncation characterization | Child terminated SIGBUS (-7); sigbus-observation.json |

Sanitizer flags: normal warning flags + `-g3 -O0
-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie`;
link `-no-pie -fsanitize=address,undefined`. ASAN_OPTIONS:
`detect_leaks=1:halt_on_error=1`; UBSAN_OPTIONS:
`halt_on_error=1:print_stacktrace=1`. Installed dependency libraries remain
unsanitized. Fresh builds use isolated scratch directories with only installed
argtable3/CUnit linked; no shared working binaries or copied built executables.

The clang-tidy variadic-list checker reported a test-wrapper false positive:
va_start precedes va_arg, minimized equivalents pass, runtime/sanitizer tests
pass. One documented line-local `NOLINTNEXTLINE` suppresses only that wrapper
warning; production analysis is unsuppressed. Six inherited global-format failures:
src/traverse.c, src/cmd_spec.h, src/traverse.h, tests/test_help.c,
tests/test_recursive.c, tests/test_output.h; none were reformatted.

## Delivery scope

New module/header/wrapper and CLI/module tests; narrow Makefile/registry/shell
inventory integration; concise cp guide; shell guide builtin-count update;
README cp implemented index and FILES additions. Session startup registered only
this verified assignment subagent in docs/sessions.txt; that mixed file is
**excluded** from the proposed command delivery. No AI_LOG/wiki/push/stage/commit.

Parent proposals in `.tmp/cp-index/README.md` and `FILES.md` are generated from
HEAD with only cp changes; working README/FILES retain unrelated existing work.
Their adjacent patches are for review, not applied to the index. The exact
allowlist and final-source hashes are in `.tmp/cp-allowlist.txt` and
`logs/commands/cp-implementation/source-hashes.json`. Historical cp.md report
Git blob remains `f346d338cef3d2cd20c67794d6a875731277f6e7`.

Not covered: hostile namespace races, concurrent mutation recovery, persistent
kernel unmap failures, bind-mount cycle construction, every library-internal OOM,
exhaustive fuzzing, cross-platform/non-Linux behavior, crash durability, or the
inherited optimization-sensitive shell fault harness at -O1. These are limitations,
not passing gates. Independent review/exact-index verification still required.
