# Independent cp review

**Verdict: no blocking defect found in the selected stable-tree contract.**
Ready for the parent's exact-index verification, not an unconditional commit
approval. No production source, shared implementation, index or commit changed.
Reviewed base: `c18b7e83d4661089c9984fe5e6391e4c0a88e9d9`.
The [implementation contract](cp-implementation.md) remains authoritative;
this is independent review evidence, not a replacement TDD history.

## Findings

- `src/cmd_cp.c:37–144`: regular source descriptor validation and both pathname
  and opened-destination device/inode checks precede sizing. No O_TRUNC open.
  Empty inputs truncate the destination without mapping. Each offset increments
  by the exact copied length; full windows are page multiples at most 1 MiB,
  and the final mapping cannot exceed the remaining file bytes. Source mappings
  are private/read-only, destination mappings shared/read-write; memcpy is
  followed by MS_SYNC and releases of both mappings. No data-transfer fallback.
  Errors preserve the first failure, release descriptors, and report partial
  effects rather than falsely claiming rollback. The sysconf failure can occur
  after resizing; this is within the documented partial-effects contract.
- `:169–385`: destination ancestry checks precede root creation, including
  nonexistent multi-component descendants and intermediate directory aliases.
  Recursive source links are reproduced, not descended into; nonrecursive links
  follow regular referents. Recursive destination directories reject symlinks.
  Unsupported types and same-file aliases fail. Directory depth/ancestor guards,
  readdir errno/EOF distinction, sibling continuation, closedir and final-mode
  cleanup are correct by inspection. New directory owner access is temporary;
  final rwx uses initial mkdir/umask result, existing directory modes are retained.
- `:62–79`: force retry is restricted to failed opens of an existing regular
  non-symlink entry. It does not remove after truncate/map/sync errors, nor remove
  directories. Retained hardlinks verify old data survives unlink/recreation.
  Destination read-and-write permission requirement, dangling-link rejection,
  depth limit and omitted metadata/topology preservation are documented GNU
  differences, not defects.
- `:388–551`: argument definitions are invocation-local and shared with help;
  exact SOURCE/DEST cardinality and UTF-8 JSON validation precede copying.
  Help syntax is stable across argc and public callback. Copied pointer array
  protects borrowed argv; exported getopt sentinels survive success/failure.
  Owned tables/arrays are released. Help output errors are detected; JSON output
  failure may follow a completed copy and correctly returns 1. No stdin content
  consumption or cwd/umask/signal/standard-descriptor changes were observed.
- Registry/build/test changes contain only cp additions and the 10→11 inventory
  update. Existing standalone parity coverage is retained; cd stays a builtin
  without an artificial standalone requirement. Capacity remains 32; the probe
  registration leaves exactly 20 successful extra registrations before rejection.
- The fault harness is meaningful: real mmap/msync/unmap/close calls underlie
  injections; no content read/write is allowed while active; map/fd counters,
  caller pointer identities and exported state are asserted. Its committed
  fixture tests only the first map pair (4097 bytes). An independent scratch-only
  2 MiB+1 variant passed faults at all six mapping positions, with zero counters
  after each failure. This is additional evidence, not a weakened test or source
  change. Recursive allocation/opendir/readdir/chmod fault injection and exact
  live-map identity tracking would strengthen coverage but are nonblocking gaps.

## Fresh execution

Tests ran on two fresh **HEAD archives plus exactly the proposed allowlist**,
with HEAD-based README/FILES versions, installed argtable3/CUnit directory links
only, and no copied project binaries. Normal and sanitizer runs were separate.
[Commands and statuses](../../logs/commands/cp-review/execution-commands.json),
[extra probes](../../logs/commands/cp-review/extra-commands.json), and
[raw outputs](../../logs/commands/cp-review/) are durable. Every stdout/stderr
and numeric status is saved without tool-output truncation.

| Check | Result |
| --- | --- |
| Fresh `make all test test-cp` | 0: full regression, GNU subset, cp project/module, main integration |
| Fresh default-O0 ASan+UBSan `make all test` | 0, leak detection and halt-on-error enabled |
| `make test-cp-impl test-cp test-shell` | 0; seven grouped cp tests and 22 shell tests, plus module assertions |
| `make lint` | 0; existing line-local test varargs suppression retained, no production suppression |
| Scoped clang-format dry run | 0 on cp/header/wrapper/registry/cp-module/shell-module files |
| `make format-check` | 2: exactly six inherited files, all byte-identical to HEAD |
| Main inventory / callback / standalone help | 0; exact 11 inventory and byte-identical cp help |
| Multiwindow syscall trace | 0; four source/destination pairs, four syncs, no source/destination read/write transfer |
| Independent 96 MiB+4097 copy under 24 MiB RLIMIT_AS | 0; exact size and streaming SHA-256 equality |
| Scratch stronger module: six map-failure positions | 0; normal 100-call reuse and resource/global-state assertions retained |
| Same inode through destination symlink / invalid option+help / repeated flag | Expected 1 / 2 / 2, no source or link mutation |
| Missing deep self-descendant | Expected 1 before creating its missing parent |
| Main success/error/help/JSON/pwd in one loop | 0 final status, exact final pwd and copied content |
| `/usr/bin/true`, no-memcpy negative controls | Both fail intended byte assertions (1), not setup errors |
| Fresh deterministic source-truncation child | **-7 (SIGBUS)**, core dumping disabled; not recoverable host behavior |

Sanitizer flags are normal project warnings plus `-g3 -O0
-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie`, link
`-no-pie -fsanitize=address,undefined`; ASAN_OPTIONS
`detect_leaks=1:halt_on_error=1`, UBSAN_OPTIONS
`halt_on_error=1:print_stacktrace=1`. Installed dependency libraries are
unsanitized. ASan is deliberately not run under the small virtual-address cap;
its full suite still copies the 64 MiB+1 fixture.

Inherited format failures: src/traverse.c, src/cmd_spec.h, src/traverse.h,
tests/test_help.c, tests/test_recursive.c, tests/test_output.h. No reformatting.
One reviewer SIGBUS build initially omitted the source include directory; the
failed output is preserved separately. Corrected build and fixture invocation
actually produced SIGBUS. The execution-helper read deduplication initially
returned no content key; prior saved procedure text was then read successfully.
Installed rendered cp(1), mmap(2), msync(2) were consulted and saved.

## Limits and readiness

Stable, trusted trees only. Pathname/type checks are **not** hostile-namespace
race protection; force unlink and recursive traversal are not dirfd-pinned
transactions. Concurrent truncation/storage faults can terminate the containing
shell through SIGBUS. No handler/recovery/atomicity/durability claim is made.
Persistent munmap failure cannot guarantee release; Linux close-error handling
assumes descriptor release. Internal argtable panic/OOM, private interrupted
getopt scans, thread safety, bind-mount construction, exhaustive fuzzing,
cross-platform execution and the inherited O1 shell fault harness are not proven
by this default-O0 review. These limits were accepted in the delegated context.
No newly discovered production defect needs a fix worker.

## Exact delivery scope

The original `.tmp/cp-allowlist.txt` has **219 unique existing paths**.
[Scope audit](../../logs/commands/cp-review/scope-audit.json) found no credential
pattern hits, missing new Markdown targets, unrelated integration hunks or
source changes since the tested snapshot. Historical cp inventory is unchanged.
This is a targeted credential scan, not a guarantee against every secret form.
README/FILES proposals contain only cp inventory/implementation additions;
unrelated dirty working versions must never be staged wholesale.

Final revised proposal: `.tmp/cp-review-allowlist.txt`; use
`.tmp/cp-review-index/README.md` and `FILES.md` (and their adjacent narrow patches)
instead of working README/FILES. These add this review/evidence registry entries
and README review link, retaining only HEAD plus approved cp changes. The final
allowlist includes the review and every regular file in its evidence directory;
its computed count/hashes/link closure are recorded in
[final scope evidence](../../logs/commands/cp-review/final-scope.json).
It does not include sessions, AI_LOG, wiki, data/procedure reorganizations,
unrelated command edits or ignored .tmp coordination files.

Parent must still stage only that explicit scope, verify exact staged names,
extract and exercise the actual index tree, check documentation link closure,
and compare final source hashes before committing. Review passes do not replace
that final index gate. HEAD and the real index remained unchanged/empty here.
