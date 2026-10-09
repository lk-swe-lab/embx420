# edit-delete-line implementation

## Selected contract (before TDD)

Authorized autonomous scope is this command only: implementation/tests, narrow
integration/docs/evidence. No staging, commits, wiki, AI_LOG or later commands.
Indexed course lookup: docs/data.sqlite lecture_content/source join,
CLI_Tools.docx paragraphs 103–107: FILE N, -h/--help, --json. Retain the
preceding editors' JSON shape {path,line,status,message}, ok/error, null success
message (paragraph 95). There is no GNU reference executable for this custom CLI.
Exact independent byte fixtures, not a synthesized reference, are authoritative.

Syntax: edit-delete-line [-h|--help] [--json] [--] FILE N. Exactly two operands;
nonempty FILE; positive ASCII decimal N through UINTMAX_MAX, leading zeroes
accepted. Signs, whitespace, hexadecimal, units, fractions and overflow rejected.
Help allows missing operands, not unknown options or extra operands. '-' is a
literal path. JSON requires a UTF-8 path; non-JSON paths and content are arbitrary
bytes. Invalid arguments fail with 2 and no JSON or mutation; empty files and
absent lines are operational failures (1), never successful no-ops.

LF defines lines; a nonempty unterminated suffix is a line; trailing LF does not
create another line. Delete existing 1-based N including its terminating LF when
present. CRLF therefore disappears completely with its selected line; bare CR
is content. Preserve every byte outside the selected line, including NUL/invalid
UTF-8, mixed newlines and the final-newline state. Deleting the only line leaves
an empty existing file, not an unlinked path. Fixed bounded streaming buffers.

Reuse only established cmd_spec/cmd_lines helpers and reviewed staging/parser
safety from edit-insert-line/edit-replace-line, without a shared editing framework.
Reject final symlinks, hard links, nonregular, foreign-owned or special-mode
files; follow parent symlinks. Require readable/writable source and writable
parent. Stage exclusively in the same directory, fully write, restore ordinary
mode/group, fsync with EINTR retry, successfully close stage/source, recheck source
and path identity/size/mode/uid/gid/nlink/mtime/ctime, then rename. Preserve uid/gid
and ordinary permissions, replace inode. Old open descriptors retain old inode.
ACLs/xattrs/sparse layout/timestamps are not preserved; atime may change on read,
and parent metadata may change. Precommit failure leaves source unedited and
removes stage, unless diagnosed cleanup failure prevents removal. Trusted stable
paths only: no locks, snapshots, hostile namespace guarantees, directory fsync
or final check-to-rename race elimination. Injected external races are not editor
writes and cannot be rolled back.

Non-JSON success is silent; errors diagnose stderr. Operational attempts with
parsed valid operands produce one JSON object plus LF; preparse/argument failures
produce none. Status 0 success, 1 operational/output failure, 2 argument error.
Postcommit JSON output failure does not undo the edit. Inherit SIGPIPE, preserve
borrowed argv pointers/bytes, descriptors/stdin offset, exported getopt globals,
cwd/environment/umask/signals. Private getopt scan state/thread safety not promised.
Installed argtable internal OOM can exit host; constructor NULL and module-owned
OOM recovery are separate tested seams. No backups, multi-line ranges or regex.

Primary APIs consulted: argtable.org/tutorial, author-maintained Linux man-pages
rename(2), mkstemp(3), fsync(2). Parser grammar is invocation-local; exclusive stage
creation and file sync do not imply directory-entry durability.

## Acceptance plan

Vertical slices: existing LF-terminated middle deletion; then unterminated last
line removal. Characterize inherited safety/parser/JSON using direct-module fault
checks and intended-marker negative controls. CLI acceptance: exact LF/CRLF/bare
CR/unterminated/binary/empty/bounds/large byte results, decimal/arity/help variation,
JSON/path validation, permissions/links/metadata/output failures. Module acceptance:
reuse, borrowed resources, allocation/descriptor balance, short/EINTR/zero I/O,
precommit faults, cleanup failures, observed concurrency and postcommit output.
Main: real discovery/help/dispatch/parity/inventory/capacity, repeated loop and
child-owned stdin/stdout PTY. Full make all test lint, scoped formatting, inherited
global formatting separately, fresh HEAD+narrow patch normal/default O0 ASan/UBSan.
Known O1 baseline fault-harness limitation is not a new-command gate.

Evidence: [raw execution log](../../logs/commands/edit-delete-line-execution.log)
and [structured ledger](../../logs/commands/edit-delete-line-evidence.json).
[Independent review](edit-delete-line-review.md) passes the selected contract;
exact-index verification remains pending.

## Executed results

| Gate | Observed result |
| --- | --- |
| Vertical slice 1 | Real main dispatch RED: 127/unknown command. GREEN: exact staged middle-line deletion. |
| Vertical slice 2 | Unterminated last-line RED: ERANGE/status 1. GREEN: exact removal, including deleting the sole line to an empty file. |
| `make test-edit-delete-line-impl` | PASS: final 15 CLI groups, including 150 seeded byte properties; 354 direct-module invocations. |
| CLI suite through main | PASS: all final 15 groups with `--program ./build/main --prefix edit-delete-line`. |
| `make all test` / `make lint` | PASS: full regression and 53-C-file analysis; inherited suppressions unchanged. |
| Scoped clang-format | PASS: new C/header/wrapper/harness plus registry and shell-module integration. |
| Global format-check | Only six inherited files fail; all six bytes match HEAD. |
| Preliminary fresh HEAD+narrow patch | PASS: `make all test lint`, main CLI byte suite and scoped format, no dirty-worktree dependency. |
| Preliminary fresh O0 ASan/UBSan | PASS: full `make all test` and main CLI byte suite with leak/halt-on-error; third-party libraries remain uninstrumented. |
| Six negative controls | Four compiled argv/getopt/no-rename/no-recheck mutants abort with intended markers; no-op executable and retained-LF mutant fail exact independent byte assertions. |
| Actual syscall trace | Exclusive same-directory stage completely written, synced and closed before rename; source never written/truncated. |
| Main integration | PASS: 15-builtin inventory, 14 standalone parity, full 32-entry capacity, help/dispatch/error propagation, repeated loop and real child-owned stdin/stdout PTY exact prompts/bytes. |

The preliminary fresh gates preceded the added inode/xattr descriptor fixture;
that final fifteenth group then passed through standalone and main. Final frozen
candidate regression/sanitizer proof is recorded externally as described below,
so the evidence files do not need to hash themselves.

CLI tests cover LF/CRLF/mixed/bare-CR/binary/empty/unterminated cases, removal of
first/middle/last/only lines, buffer boundaries and exact 32 MiB+4-byte input
removal under a 24 MiB address-space cap. Independent streaming SHA-256 checks
all retained bytes; the cap is disabled only for ASan virtual-address reservations.
NAME_MAX basename succeeds. Decimal syntax/overflow, help across operand/argc
variation, arity/options/`--`/literal '-', exact JSON and UTF-8 path validation,
permission/link/nonregular/special-mode rejection and postcommit output failures
are exercised. Permission tests ran as uid 1000 with no root skip. Mode/uid/gid
and new inode are checked; an open descriptor still sees original bytes and its
xattr, while the replacement lacks the original user xattr and has new mtime.

The direct module suite revalidates inherited staging mechanics specifically
against deletion: 100 success/error/help cycles; borrowed argv identity/string
bytes under real-parser-plus-permutation; exported getopt sentinels; stdin
position/descriptors, cwd/HOME/umask/SIGPIPE; allocation/descriptor balance; all
five parser-constructor NULLs and both module malloc sites; public usage/help
parity. Tests inject open/stat/temp/read/write/fcntl/fchown/fchmod/fsync/close/
rename errors, short/EINTR/zero transfers, real content/path races, observed mode,
owner/link/size/mtime/ctime changes, and cleanup failure with a diagnosed leftover.
Ordinary precommit failures preserve source inode/content/mode/uid/gid/nlink/mtime;
externally injected changes are expected separately. `/dev/full` and ignored
SIGPIPE fail reporting without rollback or signal-policy mutation. Internal
argtable parser OOM was reprobed in a child and exits host status 1.

Characterization of reused safety is not misrepresented as new production TDD.
The two functional slices have actual preimplementation failures. Negative controls
assert exact defect markers/status, not any arbitrary crash. Harness/setup events:
one nonunique Makefile patch was rejected unchanged; a missing Python import was
corrected before executing its new fixture; a scratch fingerprint script omitted
an import and was corrected; HEAD-based README candidate initially used the mixed
working text's lowercase count and failed an anchor assertion, then was corrected
to HEAD's uppercase text. No production behavior or tests were weakened.

Inherited format failures: src/cmd_spec.h, src/traverse.c, src/traverse.h,
tests/test_help.c, tests/test_output.h, tests/test_recursive.c. The known inherited
O1 shell fault-harness limitation was not rerun; no optimized-sanitizer claim.
Raw evidence preserves trailing whitespace emitted by real command/compiler
streams. Only that raw-log path is exempted from candidate whitespace checks;
source/documentation must pass. The final race/no-directory-fsync/internal-library
OOM/postcommit-output limits remain explicit, not silently solved by tests.

## Review handoff

No stage, commit, wiki/AI_LOG access, publication or next command. Base HEAD is
2c6f01ce87fe6d8f9f8b1bd6f38a717d7ed5233d. Separate authorized session registration
was appended while preserving previous entries; it is excluded from the command
candidate. All unrelated tracked dirty files/deletions are fingerprinted separately.

Exact 15-path allowlist, source/anchor/installed-dependency hashes, narrow HEAD-based
README/FILES candidates and `integration.patch` are in
/home/name/.hermes/cache/scratch/edit-delete-line-integration/. Do not stage mixed
working README/FILES whole. Final frozen patch proof is external in that directory's
`final-proof.log` and `final-proof.json`, with final hashes in `manifest.json`.
Fresh `normal-final` and `sanitizer-final` checkouts verify the delivered patch,
not independent review or a future exact index. Review summary and concrete final
proof results are in ignored .tmp/EDIT_DELETE_LINE_HANDOFF.md. Reproduction scripts
are embedded in the structured ledger. Independent review is now linked above;
the parent still owns exact-index verification and any separately authorized commit.
