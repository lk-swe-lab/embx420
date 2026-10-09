# edit-replace-line implementation

## Selected contract (before tests)

Autonomous implementation only; no staging, commits, publication or wiki edits.
Course lookup: docs/data.sqlite, lecture_content joined to lecture_sources,
CLI_Tools.docx paragraphs 89–95: complete-file FILE N TEXT replacement, help,
optional JSON {path,line,status,message}. No GNU executable oracle exists.
All listed course options retained. Independent literal byte expectations are
mandatory; insert/delete/regex editing and backups deferred.

Syntax: edit-replace-line [-h|--help] [--json] [--] FILE N TEXT.
N is positive ASCII decimal bounded by UINTMAX_MAX (leading zeroes allowed).
Exactly three operands; empty TEXT allowed, CR/LF in TEXT rejected. Empty path
rejected. JSON requires a UTF-8 path; raw non-UTF-8 content/TEXT is supported.
Help permits omitted operands but not malformed options or excess operands.

Lines are LF-delimited; an unterminated final nonempty segment is a line, and
trailing LF does not create an additional empty line. Replace existing line
content only; preserve its LF/CRLF terminator or absence of terminator. Other
bytes (including NUL) are unchanged. Empty files/out-of-range lines fail.

Final-component symlinks, directories/devices/FIFOs, multiple hard links,
foreign-owned files, and special-mode files are rejected without content writes.
Parent-directory symlinks are followed. Source must be readable/writable;
ordinary permission bits and uid/gid are preserved. New inode replaces old;
existing open descriptors still see old inode. ACLs, xattrs, sparse layout,
timestamps and inode identity are not preserved. Trusted stable local paths
only; no hostile namespace safety, advisory locking, or snapshot isolation.

Complete file streams through bounded buffers to an exclusive same-directory
0600 temporary file. Writes handle short progress/EINTR; stage receives original
group/mode, fsync and successful close before rename. Source/path inode, size,
mode, uid/gid, nlink, mtime/ctime are rechecked before commit; detected changes
abort. Check-to-rename race remains. No original truncation or writes, no backup.
Cleanup removes staging files on failure; cleanup failure is diagnosed and can
leave a temporary file. Reads may update source atime; staging affects parent
metadata. No directory fsync/power-loss durability guarantee.

Statuses: 0 success, 1 operational/output failure, 2 argument errors. Non-JSON
success is silent; failures diagnose stderr. JSON emits one object/newline on
parsed operational success/failure, status ok/error, message null on success or
UTF-8 error string. Argument/pre-parse allocation errors emit no JSON. Result
output occurs after rename: output failure never rolls back the committed edit.
Inherited SIGPIPE behavior remains unchanged; an ignored SIGPIPE permits status
1 on EPIPE. Borrow stdin/stdout/stderr; preserve argv strings/pointers, exported
getopt globals, cwd, environment, umask and signals. No claim of preserving
getopt private scan state or thread safety. Internal argtable allocation can
terminate the host; recoverable module/constructor failure is tested separately.

Primary API references consulted: argtable.org/tutorial; Linux man-pages author
site man7.org open(2), rename(2), mkstemp(3), fsync(2). Retrieved successfully
through Python urllib after the web extraction backend rejected extraction; no GNU executable comparison applies to this custom command.

## Planned acceptance mapping

- CLI bytes/newlines/binary/boundaries: tests/test_edit_replace_line.py.
- Decimal/arity/help/JSON/error/forbidden effects: same independent CLI suite.
- Parser ownership/getopt/argv/reuse/syscall/OOM/output/concurrency mechanisms:
  tests/test_edit_replace_line_module.c with narrow link wrappers.
- Real main registry/help/parity/dispatch/repeated loop: tests/test_shell.py and
  tests/test_shell_module.c plus command-specific main cases.
- Real RED/GREEN slices: byte replacement, terminators, safety, JSON and faults.
- Complete make test, all build, lint, scoped/global formatting and fresh default
  O0 ASan/UBSan regression. Known inherited global format failures remain distinct.
- Negative controls must demonstrably fail assertions, not fixture setup.

Execution results are recorded below. Raw complete log:
../../logs/commands/edit-replace-line-execution.log; structured ledger:
[Structured ledger](../../logs/commands/edit-replace-line-evidence.json).

## Observed RED/GREEN and corrective checks

- Core: real main returned 127/unknown command instead of replacing the fixture;
  module/build/registry integration then passed the same exact bytes and new inode.
- Terminators: CRLF tests observed dropped CR, including buffer boundaries;
  one-byte carry state fixed this and retained LF/no-final-newline/binary cases.
- Validation/safety: malformed decimal/multiline operands incorrectly succeeded,
  final symlink/hardlink edits succeeded, and a FIFO timed out after the bounded
  10-second deadline. Strict validation and preflight/O_NOFOLLOW/O_NONBLOCK
  rejection then passed without original content or mtime/inode changes.
- JSON/output: empty stdout and incorrectly successful /dev/full reporting were
  observed; single-object JSON and output failure propagation then passed.
- Fault slice: interrupted fsync returned 1 rather than 0 (fault 18); EINTR retry
  passed. The pre-final-recheck slice committed a detected concurrent write
  (fault 22 returned 0); source/path rechecks then rejected it unchanged by the
  editor. Real content and pathname races are distinct from metadata seams.
- Help regression: /dev/full produced status 1 but no stderr diagnostic; the
  reproducer failed and then passed after adding the missing help diagnostic.
- Additional main integration initially used the old core-only built binary;
  JSON/parity assertions failed and passed after rebuilding. This is stale-binary
  characterization, not a separate production bug or fabricated TDD cycle.
- Initial lint found a variadic fcntl test-wrapper analysis error. The wrapper now
  accepts/asserts only the actual module's three-argument F_SETFD call shape;
  fixtures call __real_fcntl directly. No analyzer suppression was added.
- HEAD-based documentation patch setup initially assumed the mixed working README
  phrase existed in HEAD. Lookup established HEAD's actual standalone-count phrase;
  the corrected narrow patch passed git apply --check and source equality checks.

## Final executed acceptance

| Gate | Actual observation |
| --- | --- |
| `make test-edit-replace-line-impl` | PASS: 13 CLI groups and 355 direct module invocations. |
| `python3 tests/test_edit_replace_line.py --program build/main --prefix edit-replace-line` | PASS: all 13 groups through actual main. |
| `make test all` | PASS: complete existing regression and delivery binaries. |
| `make lint` | PASS: 47 C files, no unsuppressed analyzer diagnostics. Existing unrelated suppressions unchanged. |
| Scoped `clang-format --dry-run --Werror` | PASS for new module/header/wrapper, registry and module integration tests. |
| `make format-check` | FAIL only the six inherited files below; no new formatting failures. |
| Fresh HEAD + narrow integration `make all test lint` | PASS in the review checkout; no unrelated dirty dependency imported. |
| Fresh default O0 ASan/UBSan `make all test` | PASS: leak/halt-on-error enabled, non-PIE binaries. Dependency libraries remain uninstrumented. |
| Five negative controls | PASS sensitivity: four compiled mutations abort with defect-specific markers (argv, getopt, no rename, no recheck); /usr/bin/true fails the independent byte expectation. |
| Real strace mechanism | PASS: original never written/truncated; exclusive stage completely written, synced and closed before rename. |
| Large file / name boundaries | PASS: 32 MiB + 4-byte fixture under 24 MiB child address-space limit, exact independent streaming SHA-256; maximum NAME_MAX basename. Limit intentionally disabled only for sanitizer binaries; bytes still checked there. |
| Main registry/loop | PASS: 13 builtin inventory, 12 standalone parity, 32-entry capacity, discovery/help/dispatch/status/reuse, and exact child-owned stdin/stdout PTY prompts and bytes. |

CLI groups cover first/middle/last and empty/unterminated lines, LF/CRLF including
cross-buffer CR, binary content/TEXT, option-like paths/TEXT, exact escaped UTF-8
JSON, invalid JSON path before effects, malformed/overflow decimal, help/arity,
missing/out-of-range input, real permission failures, owner/mode policies,
symlink/hardlink/FIFO/directory rejection, output failures and 100 seeded byte
properties. Normal permission fixtures ran as uid 1000, with no root skips.

Direct module checks cover 100 success/error/help reuse cycles, caller argv pointer
and string preservation under real-parser-plus-permutation sensitivity, exported
getopt sentinels, stdin offset and descriptor ownership, cwd/HOME/umask/SIGPIPE,
all six constructor NULLs, both module malloc sites and public usage constructor
failures. Tracked allocations/descriptors return to zero. Stage open/stat/temp/
read/write/chown/chmod/fcntl/fsync/close/rename failures, short/EINTR/zero transfers,
source/path changes, final metadata changes and diagnosed cleanup failure are
exercised. Source inode/mode/owner/group/nlink/mtime and bytes remain unchanged on
ordinary pre-rename failures. A foreign-owner preflight uses a metadata seam;
no privileged chown to unrelated accounts was performed. /dev/full and ignored
SIGPIPE/EPIPE return failure without reverting the committed edit or changing the
host signal policy. Installed internal argtable parser malloc failure was probed
in a child and exits the host with status 1; this is an explicit dependency limit,
not a recoverable-all-OOM claim.

Inherited format files: src/cmd_spec.h, src/traverse.c, src/traverse.h,
tests/test_help.c, tests/test_output.h, tests/test_recursive.c. Final audit compares
these bytes to HEAD. No O1 sanitizer claim: the prior reviewed shell fault harness
baseline failure remains documented in the restart handoff and was not rerun.

## Delivery boundary

Implementation and verification complete; independent review and exact-index
verification/authorized commit belong to the parent. This worker never stages or
commits and does not start edit-insert-line. HEAD remains 8c5f12b, index empty at
handoff checks. Existing dirty/deleted/untracked work is preserved.

Exact allowlist and final source/dependency SHA-256 hashes are recorded by the
final audit in the structured ledger and ignored .tmp/EDIT_REPLACE_LINE_HANDOFF.md.
README/FILES are mixed working files: never stage them whole. HEAD-based narrow
versions plus a checked integration patch are in
/home/name/.hermes/cache/scratch/edit-replace-line-integration/. The normal review
checkout is /home/name/.hermes/cache/scratch/edit-replace-line-review-p074qi_8;
final sanitizer checkout is recorded in the ledger. These are review snapshots,
not an exact staged-tree or independent reviewer claim.

[Raw complete execution](../../logs/commands/edit-replace-line-execution.log)
and the structured ledger preserve actual commands, complete streams, intended
REDs/negative controls, setup failures, final passes and limitations. Reproduction
helpers remain in scratch: edit-run.py, edit-extra.py, edit-sanitize.py and
edit-integration.py. No GNU/reference pass is claimed for this custom command.
