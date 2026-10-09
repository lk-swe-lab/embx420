# Independent touch review

Reviewed 2026-10-08 (MDT), against HEAD
`b0db03c99bfd2e1a4add175448717cdf6e8e2718` (mkdir commit).

## Verdict and delivery gate

**No blocking defect found in the selected touch module/standalone contract.**
Fresh independent builds, GNU common-subset tests, project CLI/module tests,
scoped analyzer/format checks, and ASan+UBSan passed. After the parent explicitly
reported shell integration finished and authorized full gates, this reviewer
also independently passed working-tree `make -B test`, `make all`, `make lint`,
`make test-shell`, and all 11 touch fixtures through `./build/main touch`.
**Touch shell registration/discovery/help/dispatch/parity is now verified**,
including repeated success/error/help/success invocations in one shell process.

Touch is ready for parent isolated-index verification and delivery, with no
blocking touch defect found. **Global format-check is not green:** it fails
only in six files proven byte-identical to HEAD; all scoped touch formatting
passes. Parent must record/resolve that inherited gate disposition and verify
the actual final staged tree. No staged checkout or commit was created here.
Full gates were withheld during concurrent shell edits and only executed after
the parent lifted that gate; the later integration evidence supersedes the
initial source-only registration observation.

## Scope and evidence integrity

Read src/cmd_touch.c/.h, src/touch_main.c, tests/test_touch.py,
tests/test_touch_module.c, docs/commands/touch.md, docs/reports/touch.md,
shared cmd_lines/spec, HEAD/working Makefile, registry, and the existing
`logs/commands/touch-*.log` evidence. Loaded command-development and
requesting-code-review skills and project anatomy/API/TDD/coding/verification
procedures. The no-fixes/no-staging instruction overrides the review skill's
usual auto-fix/commit steps; the dirty working tree was not stashed or reset.

All implementation/tests/shared sources and both original touch documents
were SHA256-identical before/after independent execution. Existing shared
cmd_lines.c/.h and cmd_spec.h also match HEAD. Hash ledger, exact compiler/link
commands, complete new subprocess outputs, and the minimal HEAD-only Makefile
patch are preserved in [the independent raw log](../../logs/commands/touch-review.log).
The worker's final source/test hash ledger matches this review's ledger.

Only repository files created by this reviewer:

- docs/reports/touch-review.md (this report).
- logs/commands/touch-review.log (untruncated independent scoped evidence).
- logs/commands/touch-review-integration.log (untruncated later full/shell gates).

No repository source, test, Makefile, main/shell/registry, README/FILES, AI_LOG,
wiki, index, commit, or unrelated repository edits. Outside the repository,
the loaded command-development skill gained a reusable raw-log/negative-control
evidence lesson. Session lookup verified ID
`20261008_191411_ee6a38`, title `Subagent: Independently review touch implementation using…`;
registration was not appended because this task explicitly prohibits unrelated
file edits. Parent can register it within separately authorized session scope.

## Code and contract review

| Area | Finding and independent evidence |
| --- | --- |
| Timestamp and content preservation | utimensat(AT_FDCWD,path,NULL,0) handles existing targets without content open. Existing binary fixture checks current atime/mtime bounds before reading, inode/uid/gid/full mode/size, and all original bytes. No O_TRUNC exists; exclusive creation cannot truncate an existing object. Setuid mode is included. |
| Missing files and modes | Creation uses O_WRONLY\|O_CREAT\|O_EXCL\|O_CLOEXEC with 0666; futimens updates the owned descriptor, followed by one Linux close. Real umasks 0, 027, 077, 777 and owner mode-000 updates pass. Missing-parent failure leaves no parent and continues later operands. |
| Links and nonregular targets | Real directories, hardlinks, valid symlink referents, readerless FIFO and live Unix socket are updated through metadata calls, without blocking/opening content. Final dangling symlink remains, referent is not created; loops fail. Link mtime remains unchanged. Do not promise unchanged link atime: the worker correctly corrected that oracle after GNU and project traversal both changed it. Extra directory-symlink-prefix/trailing-slash/dot fixtures pass. |
| Creation race | ENOENT transitions to exclusive create; EEXIST retries pathname timestamp update, never opens an existing object's contents. Fault-injected competitor writes exact race-data, which survives. Extra 256 real CLI processes across four missing/existing contention pairs pass; existing binary content/inode/full mode/ownership survive. This is evidence for ordinary contention, not a transactional guarantee for adversarial pathname replacement. |
| Parsing and help | Whole parse and all operand validation occur before filesystem effects. argv pointer copy avoids parser permutation of caller storage. Missing/unknown/repeated options, empty paths, malformed module argv, invalid UTF-8 JSON paths fail with status 2. Stable help across argc and print_usage agrees byte-for-byte. Valid help precedence, literal '-', '--', and deferred GNU flags agree with the recorded selected contract. Extra repeated-help/-hh/valued-flag/version/-a cases fail without effects. |
| JSON and errors | Invocation-local result array is allocated before operations; errors are saved per operand, emitted in input order after operations, with null success messages and escaped paths. Invalid locale errno-message UTF-8 uses a generic valid fallback. Exact independent JSON bytes/objects, error continuation, escaped controls and Unicode boundary fixtures pass. Filesystem/output failure returns 1; no rollback is promised. |
| Borrowed state and reuse | Direct module test makes 90 mixed success/error/help calls, checking unchanged argv pointers/strings, cwd, umask, stdin offset, borrowed fd validity and SIGPIPE disposition. Allocation/fd tracking reaches zero after each invocation. Later calls succeed after failures. |
| Fault handling | Four constructor-NULL positions plus module malloc/calloc faults produce no filesystem effects and recover on next invocation. Injected utimensat EACCES, open ENOSPC, futimens/close EIO propagate failure and allow later operands; post-create failures leave files as documented. EINTR is retried for timestamp/open/futimens; close is not retried. JSON write EINTR/short progress recover; zero/EIO fail. /dev/full help+JSON and ignored-SIGPIPE broken pipe fail without undoing completed effects. |
| Static/security review | No hardcoded credentials, subprocess/eval/serialization mechanism, truncation path, unexpected signal/cwd/umask mutation, or persistent parser allocation found. User pathname resolution and symlink following are intended functionality, not an added confinement promise. Strict C17 builds with -Werror, scoped analyzer and format all pass; four C/header files meet the 80-column rule. |

## Independently executed checks

Scratch root `S=/home/name/.hermes/cache/scratch/touch-independent-review`.
From `/home/name/Documents/420-EMBDX/A`, execute:

```sh
python3 /home/name/.hermes/cache/scratch/touch-independent-review/run_review.py
python3 /home/name/.hermes/cache/scratch/touch-independent-review/extra_checks.py
python3 /home/name/.hermes/cache/scratch/touch-independent-review/verify_head_integration.py
```

Each of these scoped scripts exited 0. Scratch scripts are reproducibility
aids, not committed source changes; the durable raw logs contain exact
commands/results even if scratch expires. No project fixture or assertion was
weakened or edited. Later full/shell checks use `S/verify_integration.py` (exit 0);
it records global format exit 2 rather than asserting that gate passed.

| Command/check | Actual result |
| --- | --- |
| Fresh standalone and wrapped module cc builds into S | Both exit 0, no diagnostics; C17, Wall/Wextra/Wpedantic/Wformat=2/Wshadow/Wconversion/Wstrict-prototypes, -Werror, -g3, -O0. |
| python3 tests/test_touch.py /usr/bin/touch --reference | Exit 0; 6 compatible tests pass, 5 explicit project-only skips. GNU coreutils 9.4, uid 1000; permission-denied test runs rather than root-skips. |
| python3 tests/test_touch.py S/touch | Exit 0; all 11 pass, no skips. Includes 128 seed-420 independent file fixtures and duplicate operands. |
| S/module | Exit 0; reuse, borrowed state, allocation/syscall/output/resource assertions pass. |
| clang-format --dry-run --Werror src/cmd_touch.c src/cmd_touch.h src/touch_main.c tests/test_touch_module.c | Exit 0, no output. |
| clang-tidy on cmd_touch.c, touch_main.c, test_touch_module.c, project analyzer checks and warnings-as-errors | Exit 0; one warning suppressed in non-user code. Exact invocation in raw log. |
| ASan+UBSan fresh builds and tests | Builds exit 0 using -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -no-pie. All 11 CLI tests and wrapped module pass with detect_leaks=1:halt_on_error=1 and UBSan halt_on_error=1:print_stacktrace=1; no sanitizer diagnostics. |
| Extra independent scratch fixtures | 7 parser failures/no effects; 8 malformed UTF-8 rejects/no earlier creation; 8 valid scalar boundaries; prefix/final directory links, slash/dot; NAME_MAX=255 and overlong error/later success; 256 contention processes all pass. |
| HEAD plus touch-only Makefile integration: make test-touch test-touch-impl | Fresh scratch checkout with only touch sources/tests/docs added; exit 0, GNU 6/project 11/module pass. No shell, registry, cat/head/tail/pwd source exists there. Only installed argtable/CUnit dependency directories are linked. Full all/test intentionally not run. |

### Post-integration gates (independently rerun after authorization)

[Full raw evidence](../../logs/commands/touch-review-integration.log):

| Actual command/check | Result |
| --- | --- |
| make -B test | Exit 0; actual forced rebuild and full current working-tree suite, including 17 shell CLI tests, shell module and all touch CLI/module tests. No HEAD-main substitution. |
| make all | Exit 0. |
| make lint | Exit 0; all 33 translation units processed, one suppressed non-user warning. |
| make test-shell | Exit 0; 17 shell CLI tests and registry/argv/EINTR/OOM module assertions pass. |
| python3 tests/test_touch.py S/touch-via-main | Exit 0; all 11 exact touch fixtures, no skips, through os.execv of working build/main with explicit touch dispatch. Fixtures/oracles unchanged. |
| Independent help/dispatch/loop parity | Exact standalone --help == main touch --help == main help touch; main help == main --help with literal touch listing. Exact invalid/operational status/stdout/stderr parity and no forbidden creation. Quoted JSON / invalid / help / JSON in one loop yields exact independent bytes and last-success status. |
| make format-check | Exit 2, inherited failures only: src/cmd_spec.h, src/traverse.c, src/traverse.h, tests/test_help.c, tests/test_output.h, tests/test_recursive.c. All six current file bytes explicitly compared equal to git show HEAD:path; scoped touch format is green. No formatting edits. |

All reviewed touch/shared source, test and original document hashes remained
unchanged after these later gates. Reproduce the later gates from the repository:
`python3 /home/name/.hermes/cache/scratch/touch-independent-review/verify_integration.py`.

Compiler GCC 13.3.0; clang-format/clang-tidy LLVM 18.1.3. The installed argtable
library was linked normally, not rebuilt with sanitizers.

### Test-strength controls

- `/usr/bin/true --reference --case test_existing`: exit 1 at stale timestamp
  bound assertion (not a missing executable or harness failure).
- Scratch wrapper first invokes the real project executable, then changes only
  binary's mode: test_existing exits 1 at full-mode preservation assertion.
- Scratch wrapper first invokes the real project executable, then changes its
  first byte without changing length, restores original mode: test_existing
  exits 1 at exact content assertion. Initial wrapper lacked write permission;
  that setup failure is preserved separately and is **not** counted as strength
  evidence. The corrected wrapper reaches the intended assertion.
- Existing TDD logs show genuine behavior failures and distinguish setup/oracle
  corrections. Some JSON-wrapped historical logs contain literal `[truncated]`
  excerpts; a stored tool exit code is not the full output. New independent
  raw logs above are complete. Earlier baseline-main substitution in worker
  scratch full regression is correctly labeled; it does not pass the current
  shell integration gate.

## Minimal isolated touch integration for parent

HEAD already contains unchanged cmd_lines.c/.h and cmd_spec.h; no new shared
source dependency is needed. An isolated standalone/module touch commit can add
these implementation/test/documentation files plus touch-only Makefile hunks:

- src/cmd_touch.c, src/cmd_touch.h, src/touch_main.c.
- tests/test_touch.py, tests/test_touch_module.c.
- docs/commands/touch.md, docs/reports/touch.md, docs/reports/touch-review.md.
- logs/commands/touch-review.log, logs/commands/touch-review-integration.log,
  and selected existing touch evidence logs.
- Makefile: add build/touch to all; standalone/module targets, test-touch and
  test-touch-impl (including wraps); add both target dependencies and execution
  to test; remove both executables in clean. No shell-main linkage or other
  uncommitted command hunks belong in this HEAD-based minimum.

Exact tested patch: `S/touch-only.Makefile.diff`, also embedded in the raw log;
scratch checkout: `S/head-touch-only`; hash ledger:
`S/head-touch-only-hashes.json`. This validates the dependency minimum, not an
index tree or commit. Parent must construct its isolated index from HEAD
without overwriting the mixed working Makefile, verify an explicit path
allowlist, extract/retest the actual final staged tree, and include the final
review report/log version. No staging was done here.

**Shell-required delivery is larger than this isolated minimum.** Its working
registration/links and actual discovery/help/dispatch/error/JSON/reuse tests
now pass, together with working full regression/build/lint. Retain that reviewed
integration: either land shell prerequisites first and add touch to that
registry, or use a separately reviewed combined integration. A HEAD-based
standalone-only commit still does not contain the shell requirement. Parent
must verify complete final staged regression/build/lint and record inherited
format failures separately; working-tree passes do not verify a differently
assembled index.

## Limits and remaining blockers

- **Pending:** parent final isolated-index/staged-tree verification and disposition
  of the six inherited global format failures. Working full regression/build/lint
  and executed touch shell registration/integration are independently verified.
- Internal argtable malloc failure may panic/exit. Constructor-NULL/module-owned
  allocation seams do not establish all-library OOM recovery. Void print_usage
  cannot return allocation failure; tested constructor failure releases resources
  and produces no usage bytes.
- No thread-concurrent getopt/parser guarantee, adversarial prefix replacement
  guarantee, rollback, privileged live-device mutation, live read-only filesystem,
  default-ACL experiment, unbounded fuzzing or whole-dependency sanitizer coverage.
  Ordinary nonregular targets and deterministic error seams are covered.
- Arbitrary CLI argv cannot contain NUL; module callers must provide valid borrowed
  arrays/storage. Malformed NULL elements are checked; inaccessible pointers and
  lengths inconsistent with actual allocated arrays cannot be validated safely.
- New independent review establishes no blocking module defect, not absence of
  all bugs. Original worker report is a historical implementation snapshot;
  its statements that review/registry was not yet present are superseded only
  to the precise extent stated above.
