# edit-replace-line independent review

## Verdict

PASS for the selected, documented trusted/stable-path contract. No blocking
command-scope defect found; production source and existing tests were not changed.
This is a source/snapshot review, **not** exact-index approval. Nothing staged or
committed. Parent must independently verify the exact proposed index before commit.

## Executed independently

Fresh snapshots start at HEAD `8c5f12bbe43fe467de80055fd0723284d02b93b9`;
only the reviewed narrow integration patch and explicit command files are added.
Installed argtable3/CUnit dependency directories are linked, not dirty sources.
Full commands, streams, statuses, reproduced helper scripts and hashes are in the
[review ledger](../../logs/commands/edit-replace-line-review-evidence.json) and
[raw review execution](../../logs/commands/edit-replace-line-review-execution.log).

| Gate | Observation |
| --- | --- |
| Fresh unchanged HEAD `make all test lint` | PASS; establishes clean regression/analyzer baseline. |
| Fresh HEAD plus narrow command `make all test lint` | PASS; includes 13 command CLI groups, 355 direct module invocations, and 25 real-main shell groups. |
| Same 13 command groups through `build/main --prefix edit-replace-line` | PASS, including actual access failures as uid 1000, large-file byte hash under bounded address space, JSON and help output failures. |
| Default O0 ASan/UBSan fresh `make all test` | PASS; leak detection and halt-on-error enabled, non-PIE. Vendor libraries uninstrumented. |
| Scoped clang-format plus 80-column audit | PASS on module/header/wrapper, registry and changed C module tests. |
| Global `make format-check` | Status 2 on both HEAD and command snapshots: only the same six unchanged inherited files. |
| Adversarial byte oracle | PASS: 5,166 invocations across 501 exhaustive tiny-binary/random fixtures, independently calculated selected byte ranges, exact stderr/status/content, rejected-range inode/mtime preservation and no stage leftovers. |
| Additional adversarial checks | PASS: old open descriptors retain old inode, no-op still replaces inode, setgid/sticky and dangling symlink rejection, escaped control/Unicode JSON path with raw non-UTF-8 TEXT. |
| Five test-sensitivity controls | PASS: compiled argv/getopt/no-rename/no-recheck mutants abort with intended defect-specific markers; no-op executable fails intended independent byte assertion. No fixture crashes accepted. |
| Real syscall trace | PASS: source not written/truncated; same-directory exclusive staging written, fsynced and closed before rename, no stage leftover. |

Normal snapshot: `/home/name/.hermes/cache/scratch/edit-independent-normal-e0gx_rhc`.
Sanitizer snapshot: `/home/name/.hermes/cache/scratch/edit-independent-sanitize-e8hu2x7r`.
Baseline snapshot: `/home/name/.hermes/cache/scratch/edit-independent-baseline-w7l_vdii`.
The ledger records final documentation patch validation and repeated regression.
No GNU oracle was substituted for this custom command.

## Source and test review

- Argtable syntax is invocation-local and reused for public help. Mandatory logical
  operands are shown in help without changing parser behavior; help/error precedence,
  exact arity, decimal overflow/sign/prefix rejection, empty TEXT, `--`, and literal
  `-` are exercised. JSON validates UTF-8 path before effects, quotes controls and
  emits one result for parsed operational success/failure, not argument errors.
- Byte streaming preserves every nonselected byte; CR immediately before selected
  LF is preserved across buffer boundaries. Lone final CR is content, not a newline.
  Empty files and the nonexistent segment after trailing LF are rejected.
- Final lstat/open/fstat preflight rejects unsupported type/link/owner/mode; source
  open is read/write but never truncated/written. Stage name excludes basename, so
  maximum legal source basename works. Stage is exclusive and starts 0600, receives
  original group/ordinary mode, complete writes, fsync and successful close before
  consistency checks, successful source close and rename. No source truncation path.
- Deterministic failures cover owned allocations/constructors, read/write progress,
  EINTR, sync/metadata/close/rename failures, real source-content/path replacement,
  observed final metadata changes and diagnosed failed unlink cleanup. Postcommit
  stdout failure returns 1 without pretending rollback. Close is not retried.
- Caller argv pointers/bytes, exported getopt values, borrowed stdin offset and
  descriptors, cwd/HOME/umask/SIGPIPE and owned allocations/descriptors are checked
  through 100 success/error/help reuse cycles. Internal argtable OOM is deliberately
  probed in a child and characterized as host exit 1, not recoverable parser OOM.
- Registry/build integration is narrow; discovery/run/help/standalone parity,
  repeated loop statuses, 13 builtins/12 wrappers/32 capacity and a child-owned
  stdin/stdout controlling PTY with exact prompts/bytes pass in the real main.
  Existing main/shell/helper anchors match HEAD and the implementation manifest.
- Added source/patch scan found no credentials, shell-command execution, unsafe
  deserialization or unrelated implementation changes. Literal byte expectations
  and mutation assertions were inspected; implementation evidence source hashes
  independently match current files. Existing RED/GREEN records are historical
  worker evidence; this reviewer does not claim to have rerun pre-fix TDD.

## Explicit limits and remaining gates

Observed source/path changes abort, but there is no check-to-rename race closure,
locking, snapshot isolation or hostile/unstable namespace guarantee. Parent links
are followed; pathname directories are not pinned. New inode replaces source;
ACL/xattr/timestamps/sparsity are not preserved. Reading may update source atime,
staging affects parent metadata, and no directory-fsync crash guarantee is made.
Staging cleanup failure is diagnosed and may leave a file; output failure after
commit cannot roll back. SIGPIPE is inherited. Getopt preservation covers exported
values, not private interrupted-scan state or threads. Installed internal argtable
OOM may exit the host. Vendor code is not sanitizer-instrumented. No optimized O1
sanitizer claim; the known baseline fault-harness issue is outside this review.

Inherited format failures: `src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`,
`tests/test_help.c`, `tests/test_output.h`, `tests/test_recursive.c`; all compared
byte-for-byte to HEAD. No formatting suppression or unrelated source fix added.

Final allowlist, candidate documentation bytes, patch and full SHA-256 manifest
are in the review ledger and scratch delivery manifest. README/FILES working
copies contain unrelated work and **must not** be staged whole. Use the final
HEAD-based candidates/patch named by the ledger. The only incidental working edit
is authorized current-session registration in `docs/sessions.txt`; exclude it
from this command delivery. No AI_LOG reads, wiki edits, pushes or next command.
