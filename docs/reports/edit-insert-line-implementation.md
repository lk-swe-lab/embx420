# edit-insert-line implementation

## Selected contract (before tests)

Autonomous scope: this command only, no staging/commits/wiki/publication.
Indexed course evidence: docs/data.sqlite, lecture_content/source join,
CLI_Tools.docx paragraphs 89 and 97–101: deterministic complete-file editing,
FILE N TEXT, -h/--help, --json. The prior command's paragraph 95 JSON shape is
retained: {path,line,status,message}, ok/error, null success message. No GNU
executable oracle exists; use independently specified exact byte expectations.

Syntax: edit-insert-line [-h|--help] [--json] [--] FILE N TEXT.
Positive ASCII decimal N bounded by UINTMAX_MAX; leading zeroes allowed.
Exactly three operands, nonempty FILE, empty TEXT allowed, CR/LF rejected in TEXT.
JSON paths must be UTF-8; original bytes and TEXT need not be UTF-8. Help accepts
omitted operands, not malformed options or extra operands. '-' is literal.

LF defines lines; nonempty unterminated final content is a line; trailing LF
adds no empty line. Insert before existing 1-based N; N=line_count+1 appends;
N=1 is valid for an empty file. Every new line is TEXT followed by **LF**, even
in a CRLF/mixed file; existing bytes and CRLF terminators are preserved unchanged.
Appending to a nonempty unterminated file first adds LF to separate its old last
line, then TEXT+LF. A bare final CR is preserved and thus becomes CRLF when the
separator is appended. No newline-style inference or conversion. Fixed LF is
selected for deterministic bounded one-pass behavior, not GNU compatibility.

Retain preceding command's safety policy: reject final symlinks, hardlinks,
nonregular, foreign-owned or special-mode files; follow parent symlinks. Require
read/write source access and writable parent. Preserve ordinary mode and uid/gid;
new inode replaces old; existing descriptors see old inode. ACLs, xattrs, sparse
layout and timestamps are not preserved; reads can change atime. Stage exclusively
in the same directory, write all bytes with bounded buffers, restore mode/group,
fsync (retry EINTR), successfully close stage/source, recheck source/path identity,
size, mode, uid/gid, nlink, mtime/ctime, then rename. Precommit failure leaves the
source unedited and removes stage unless diagnosed cleanup failure prevents it.
Parent metadata can change. Trusted stable paths only: no locking, snapshot,
hostile-namespace protection, or directory-fsync/power-loss guarantee. The final
check-to-rename race remains. Race tests distinguish editor effects from injected
external writes.

Statuses: 0 success, 1 operational/output failure, 2 argument error. Silent
non-JSON success; errors diagnose stderr. Parsed operational attempts emit one
JSON object/newline; argument/preparse allocation errors emit no JSON. JSON
output failure after rename does not roll back. Inherit SIGPIPE; preserve borrowed
stdin/stdout/stderr, argv pointers/bytes, exported getopt globals, cwd/environment,
umask/signals. Getopt private state/thread safety not promised. Installed argtable
internal OOM can exit the host; module-owned/constructor NULL recovery tested.

Reuse decision: use existing cmd_spec and cmd_lines output/UTF-8 helpers; keep
invocation-local argtable and staging mechanics in this module, following the
reviewed prior command rather than extracting an editing framework or changing
prior source. Defer delete/regex editing, backups and configurable newlines.
Primary API references consulted: argtable.org/tutorial and author-maintained
Linux man-pages open(2), rename(2), mkstemp(3), fsync(2). This custom CLI has no
reference command/manual options beyond its course specification.

## Acceptance plan

Small RED/GREEN slices: existing-line insertion; empty/append semantics; then
inherited validation, JSON and staging/resource contract characterization.
CLI byte oracle covers LF/CRLF/unterminated/binary/empty/bounds, decimal/arity/help,
JSON/UTF-8, modes/links/permissions/output, buffer/large/name bounds and seeded
properties. Direct module wrappers cover borrowed arguments/getopt/resource/reuse,
constructor/module OOM, transfer and stage failures, output, observed concurrency,
cleanup and forbidden effects; mutants require intended markers. Main integration
requires discovery/help/dispatch/parity, repeated loop, real child-owned stdin AND
stdout PTY, inventory and capacity. Run full make all test lint, scoped/global
format checks, fresh HEAD+narrow patch normal and default O0 ASan/UBSan regressions.
Six inherited formatting failures and O1 baseline harness limitation stay separate.

Execution evidence: [raw log](../../logs/commands/edit-insert-line-execution.log)
and [ledger](../../logs/commands/edit-insert-line-evidence.json). Final results and
handoff paths are recorded below; no unexecuted check is a pass.

## Executed results

| Gate | Observation |
| --- | --- |
| Two vertical RED/GREEN slices | Core main dispatch failed with 127/unknown command, then exact insertion passed. All six empty/append cases failed with ERANGE, then passed after EOF append/separator handling. |
| `make test-edit-insert-line-impl` | PASS: 14 CLI groups, including 150 seeded byte properties; 355 direct module invocations. |
| CLI suite through `build/main --prefix edit-insert-line` | PASS: all 14 groups. |
| `make all test` | PASS: complete regression and delivery binaries. |
| `make lint` | PASS: 50 C files, no unsuppressed analyzer diagnostics; existing suppressions unchanged. |
| Scoped `clang-format --dry-run --Werror` | PASS: new C files plus registry and shell-module integration. |
| `make format-check` | Only the same six inherited files fail; their bytes match HEAD. |
| Fresh HEAD + narrow patch `make all test lint` | PASS without unrelated working-tree dependencies. |
| Fresh default O0 ASan/UBSan `make all test` | PASS with leak/halt-on-error enabled and non-PIE; dependency libraries remain uninstrumented. Main-prefixed CLI suite also passes. |
| Six negative controls | Four compiled argv/getopt/no-rename/no-recheck mutants abort with intended markers; no-op executable and omitted-append-separator mutants fail independent byte assertions. |
| Actual syscall trace | Original never written or truncated; exclusive same-directory stage fully written, synced and closed before rename. |
| Size/resource boundaries | Exact independent streaming SHA-256 for 32 MiB+4-byte input under 24 MiB address-space limit; NAME_MAX basename passes. Memory limit disabled only under sanitizer, bytes still checked. |
| Main integration | Exact 14-builtin inventory, 13 standalone parity, 32-entry capacity, discovery/help/dispatch/error status, repeated success/error/help loop and child-owned stdin/stdout PTY bytes/prompts all pass. |

CLI checks include empty TEXT/files, first/middle/last insertion, terminated and
unterminated append, fixed-LF insertion amid preserved CRLF/mixed/binary bytes,
buffer edges, decimal syntax/overflow, arity/help/options, UTF-8 JSON path escaping,
invalid JSON paths before effects, real permission failures, mode/group retention,
symlink/hardlink/FIFO/directory rejection and postcommit output failures. Permissions
ran as uid 1000 with no root skips. The byte oracle uses original-byte offsets and
literal expected insertion effects; no synthesized GNU oracle is used.

Direct module checks retain the preceding editor's staging contract, revalidated
against the insertion source: 100 success/error/help cycles, argv pointer/string
preservation under real-parser-plus-permutation sensitivity, exported getopt
sentinels, stdin offset/borrowed descriptors, cwd/HOME/umask/SIGPIPE, all constructor
NULLs, both module malloc sites, allocation/descriptor balance and public usage.
Open/stat/temp/read/write/fcntl/group/mode/fsync/close/rename faults, short/EINTR/
zero progress, real content/path races, observed metadata changes and diagnosed
cleanup failure are exercised. Ordinary precommit failures preserve inode, bytes,
mode/uid/gid/nlink/mtime; externally injected race writes are separately expected.
Foreign-owner rejection uses a metadata seam, not privileged chown. Ignored SIGPIPE
and /dev/full reporting fail without undoing the edit or changing signal policy.
Installed internal argtable OOM was reprobed in a child and exits status 1.

Harness corrections are preserved, not presented as production TDD: a transformed
help expectation still contained the old replacement prose and was corrected to
the preselected insertion contract; the first no-rename mutant hit the size
assertion inside redirected stderr without its marker, so the marker now precedes
both size and byte-content assertions. Initial narrow patch application exposed
one newly added Makefile EOF blank line; removed and final gates rerun. Primary
retrieval initially failed through the extraction backend; urllib succeeded. One
case-sensitive research term was corrected before the complete research pass.

Inherited formatting failures: src/cmd_spec.h, src/traverse.c, src/traverse.h,
tests/test_help.c, tests/test_output.h, tests/test_recursive.c. No O1 sanitizer
claim: the known inherited optimized shell fault-harness limitation was not rerun.
No change to prior command/shared editing sources, legacy routing or signal policy.

## Review handoff

Implementation verification complete; independent review, exact-index verification
and any authorized commit remain the parent's gates. This worker never stages or
commits; edit-delete-line was not started. HEAD stays
5ef0df6b8de13cc708352de1133e0fd75181475a; index empty at final audit.

Exact allowlist, source/anchor/dependency hashes and final checkout paths are in
the structured ledger and ignored .tmp/EDIT_INSERT_LINE_HANDOFF.md. Narrow
HEAD-based README/FILES candidates and integration.patch are in
/home/name/.hermes/cache/scratch/edit-insert-line-integration/. Preserve the mixed
working README/FILES bytes; never stage those whole. The scratch normal/sanitizer
checkouts prove dependency closure, not independent review or exact-index delivery.
One complete raw execution log and one structured ledger retain full streams,
actual statuses, intended REDs, setup/harness failures and final passes.
