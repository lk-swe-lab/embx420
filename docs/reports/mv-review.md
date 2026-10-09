# mv — independent review

Verdict: **PASS for the selected stable-local rename contract**; no blocking
security or logic defect found. No production source correction was needed.
Implementation patch reviewed:
`6305f3f62ea84529cccb9090b68d2ba0859383299d4f6d8581c4c8cc322f3fa4`.
Base HEAD: `ae3fe5ef739602aef2473d5486c4fa95070a8d82`.

## Review and executed gates

Course SQLite was queried first: CLI_Tools.docx paragraphs 48–52 require
move/rename, help, force and JSON. GNU installed 9.4 manual, current GNU manual,
and POSIX Issue 8 rename/stat/lstat contracts were inspected. GNU default
noninteractive replacement, same-inode success and copy/delete fallback differ
intentionally from this project's refusal/same-inode-error/rename-only subset.

Reviewed parser/help and ownership, exported getopt restoration, argv isolation,
UTF-8-before-effects and JSON byte escaping, final-link versus directory
resolution, trailing slashes, type/empty-directory constraints, inode/metadata,
error/status/output behavior, registry capacity and build dependency closure.
Cyclic final target links are replaceable entries; cyclic or non-directory
prefixes still fail. The module imports no open/read/unlink/chmod/copy mechanism;
only rename mutates the namespace. No path-derived subprocess or credential was
found. Preflight is deliberately not concurrency-safe.

Fresh HEAD archives received the actual patch; all 16 delivered blobs, 580
unchanged anchors and 13 installed dependency files matched SHA-256/blob/modes.
Normal and sanitizer suites used independent fresh directories, not worker builds.

| Executed command/gate | Result |
| --- | --- |
| `make -j2 all test test-mv` | 0; complete regression, 15 mv project groups, 9 GNU groups with 6 intentional skips, module fault/reuse tests; 32 real-main groups including discovery/help/parity/repeated loop and stdin+stdout PTY. |
| `make lint` | 0; 59 translation units; inherited suppressed dependency/NOLINT warnings only. |
| Scoped clang-format on changed C files | 0. |
| `make format-check` | 2; exactly six unchanged inherited files, listed below. |
| Fresh O0 `make -j2 all test test-mv` with ASan/UBSan | 0; full existing regression, real main/PTY, registry overflow/capacity, 500 module swap/back/help loops. |
| `python tests/test_mv_review.py .` | 0; 1,808 independent fixture invocations, standalone and main. |
| Same independent test under O0 ASan/UBSan | 0; all 1,808 invocations. |
| No-rename mutant against production tests | 1; `rename must remove source name` assertion. |
| No-rename mutant against independent oracle | 1; source/target identity assertion. |
| Borrowed-argv mutant against module harness | SIGABRT (-6); `MV_ARGV_POINTER_CHANGED`, not setup failure. |
| Strict source/docs/JSON patch whitespace | 0; raw logs are explicit exceptions only. |

Independent oracle covers file/directory/nonempty tree/FIFO/Unix socket/live,
dangling/cyclic/unresolvable symlink entries against absent and existing targets;
default and force; real and symlink-directory effective targets; unchanged
failed-operation namespace/inodes/bytes; every permitted single byte in path
names and JSON UTF-8 rejection. Expectations come from fixture types and the
selected policy, not GNU or the production resolver. Parent timestamps and
symlink atime are intentionally not preservation assertions. This finite matrix
is not exhaustive hostile-tree or concurrent testing.

Sanitizer flags:
`-std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion
-Wstrict-prototypes -g3 -O0 -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie`, link
`-fsanitize=address,undefined -no-pie`; environment
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
Use `make clean` before switching flags. Dependencies are installed argtable3 and
CUnit from the repository's build dependency directories.

## Evidence and delivery

[Complete raw streams](../../logs/commands/mv-review-execution.log) and
[command/status/base64-stream ledger](../../logs/commands/mv-review-evidence.json).
[Independent oracle](../../tests/test_mv_review.py).
[Usage](../commands/mv.md); [implementation evidence](mv-implementation.md).

The reviewed final delivery adds only this report, the raw log/ledger and the
independent test to the original 16-path candidate, plus narrow review references
in HEAD-based README/FILES proposals. Mixed live README/FILES are untouched.
External final manifest and exact patch are in
`/home/name/.hermes/cache/scratch/mv-delivery/reviewed-manifest.json` and
`reviewed.patch`. The external ledger records final-patch reapplication and fresh
normal/sanitizer gates without self-referential evidence hashes. The manifest
contains the full explicit allowlist, every blob/SHA-256/Git mode, unchanged
anchors, installed dependencies and evidence hashes. Exact-index verification
belongs to the parent; no staging/commit/publication occurred here.

## Exceptions and limits

Inherited formatting: `src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`,
`tests/test_help.c`, `tests/test_output.h`, `tests/test_recursive.c`.
Raw stream whitespace is preserved; exceptions are only implementation and
review raw logs. Source/docs/JSON whitespace must pass separately.

The real EXDEV test is a procfs memfd-link to local scratch rename, not migration
between two writable ordinary filesystems; ordinary-path EXDEV is injected too.
Internal argtable allocation OOM can terminate the host; module-owned allocations
and constructor-NULL recovery pass but do not prove all-OOM recovery.
No cross-device copy/delete, atomic no-replace under concurrency, fsync durability,
EIO/NFS or post-rename output rollback, private getopt scan restoration or thread
safety. Signals are inherited. The known inherited O1 shell fault-harness issue
was not claimed fixed or reverified; the delivered O0 sanitizer gate passes.
Session `20261009_024851_b87819` title was verified as
`Subagent: Independently review and execute verification… #5`; registration was
excluded by the assignment and not modified. AI_LOG content was not read.
