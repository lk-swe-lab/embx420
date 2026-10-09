# edit-delete-line independent review

Verdict: **PASS for the selected contract**, with the limits below. No blocking
command-scope defect was demonstrated; production source/tests were not changed.
Base: `2c6f01ce87fe6d8f9f8b1bd6f38a717d7ed5233d`. No staging or commit.

## Reviewed scope

Indexed CLI_Tools.docx paragraphs 103–107 require FILE N, help and JSON. The
[implementation contract](edit-delete-line-implementation.md) supplies the
explicit LF, unterminated-line, safety, metadata and module-ownership policies;
these are not invented GNU compatibility claims. The checked implementation
removes the selected existing 1-based line including its LF, preserving every
other byte, and rejects empty/absent lines without replacing the source.

Reviewed parser/help/decimal/arity/UTF-8 and JSON status/output paths; borrowed
argv/getopt/descriptors and owned allocation cleanup; bounded streaming; exclusive
same-directory stage creation, complete writes, mode/group restoration, successful
fsync and stage/source close, observed descriptor/path consistency, checked rename
and failure cleanup. Main registry/build/test integration is narrow. Source opens
with read/write permission but never writes/truncates the original inode.

Reviewed module SHA-256:
`a42a1b43d3caf514906723be3398277e294b2b3dfe0d3c5e94e9d4ea7f009c5c`.

## Independent execution

Full streams: [review raw log](../../logs/commands/edit-delete-line-review-execution.log).
Commands/statuses, hashes and reproduction scripts:
[review ledger](../../logs/commands/edit-delete-line-review-evidence.json).

| Gate | Actual observation |
| --- | --- |
| Fresh unchanged HEAD | `make all test lint` passed; establishes independent baseline. |
| Fresh HEAD + supplied narrow patch | Exact candidate/anchor/dependency hashes matched; `make all test lint` passed. |
| Command through main | All 15 CLI groups passed; standalone also passed within full regression. |
| Direct module | 354 invocations passed, including 100 success/error/help cycles, constructor/module-owned OOM, syscall faults, cleanup, resources and getopt sentinels. Internal parser OOM exited the child with 1. |
| Main shell | 29 groups passed: 15 builtin inventory, 14 wrappers/parity, discovery/help/status, repeated loop, child-owned stdin/stdout PTY; registry module exercised all 32 entries. |
| Independent byte oracle | 8,010 CLI executions: 5,268 successes and 2,742 absent-line failures, standalone/main. Exhaustive lengths 0–5 over NUL/LF/CR/0xff plus random binary inputs at 65,535–131,073-byte boundaries. Independent byte-offset expectations, JSON and forbidden-effect checks passed. |
| Six fresh negative controls | argv/getopt/no-rename/no-recheck mutants aborted with intended diagnostic markers; no-op and retained-selected-LF binaries failed exact middle-line byte assertions. These are characterization sensitivity, not fabricated production RED/GREEN fixes. |
| Actual syscall trace | O_EXCL same-directory staging, write, fsync, successful close and rename in order; no original write/truncate/unlink. |
| Fresh default O0 ASan/UBSan | Full `make all test`, main's 15 CLI groups and scoped formatting passed with leak/halt-on-error and non-PIE. Third-party libraries uninstrumented. |
| Formatting/static checks | Scoped format, new C 80-column limit, secret/injection scan and source/documentation patch whitespace passed. |

CLI regression includes the 150 seeded cases, CRLF/unterminated/only/empty lines,
32 MiB+4-byte input with streaming retained-byte SHA-256 under 24 MiB normal
address-space cap, NAME_MAX, inode/owner/mode/xattr/open-descriptor behavior,
permissions and postcommit output failure. Review execution uid was 1000: no root
permission-fixture skip. ASan's address-space cap is intentionally omitted.

Global `make format-check` fails on the same six byte-identical HEAD files:
`src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`, `tests/test_help.c`,
`tests/test_output.h`, `tests/test_recursive.c`. No formatting change is included.
Real raw compiler/trace streams retain trailing whitespace; only the two explicit
command raw-log paths are exempt from patch whitespace checking.

## Limits and delivery

Trusted stable paths, not hostile namespaces: the final check-to-rename race
remains; no locks/snapshot/directory fsync or power-loss durability guarantee.
Source read atime and parent metadata may change. Replacement changes inode and
drops original ACLs/xattrs/sparse layout/timestamps; ordinary mode/uid/gid remain.
Postcommit output failure cannot roll back. SIGPIPE is inherited. Internal argtable
OOM can terminate the host; recoverable constructor NULL/module-owned OOM seams
are distinct. Exported getopt globals are restored, not private interrupted-scan
state/thread safety. Diagnosed cleanup failure can leave staging. The known O1
baseline shell fault-harness limitation was not rerun; no optimized sanitizer claim.

Final reviewed patch/manifest and frozen-artifact verification are external in
`/home/name/.hermes/cache/scratch/edit-delete-line-review/` to avoid self-referential
hashes. Exact delivery is 18 paths: the original 15 candidate paths plus this report
and the two review evidence files. README/FILES are narrow scratch candidates,
not mixed working files. Only command docs were updated in place; session
registration is separately authorized and excluded. Unrelated fixed dirty/deleted
paths are preserved, HEAD/index unchanged. Exact-index verification and commit
remain the parent's separate gate; this review does not authorize either.
