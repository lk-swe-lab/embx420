# mkdir independent review

Reviewed 2026-10-08 by an independent reviewer (Hermes, gpt-6.1-sol,
reasoning unknown), after the prior provider HTTP 429 interruption.
Base HEAD: `18d70a9` (ls). No source/test changes, staging or commits.

## Verdict and readiness

**PASS for the documented course-minimal mkdir contract. No blocking source,
security, integration or regression defects found.** Ready for the parent's
isolated final-index verification and commit; this review does not authorize
advancing to another command or claim that the final Git index was verified.
Global formatting and fatal dependency OOM remain explicit limitations, not
passing checks. No separate fix worker is required for the selected scope.

Reviewed `src/cmd_mkdir.c`, `src/cmd_mkdir.h`, `src/mkdir_main.c`,
`tests/test_mkdir.py`, `tests/test_mkdir_module.c`, shared cmd_lines/spec
interfaces, working integration and HEAD integration. Read AGENTS, session,
anatomy/API, adding-command, coding, TDD/design, verification and logging
procedures, `docs/commands/mkdir.md`, and saved execution evidence.
The five reviewed source/test SHA256 values match the implementation log's
TESTED SOURCE SHA256 section exactly.

## Findings

- Parser and help share invocation-owned argtable definitions. Borrowed argv
  is validated, copied before parsing, and not permuted in place. Unknown,
  deferred and repeated options fail before mutation; help cannot mask parse
  errors. Help cardinality does not depend on argc and matches public usage.
- Parent creation preserves kernel dot/dotdot, slash and symlink resolution.
  Only newly created intermediate parents receive owner write/search; existing
  modes, process cwd and umask are not modified. Independent operand failures
  continue with saved errno; completed creation is intentionally not rolled back.
- JSON validates every input path before mutation, preserves operand order,
  escapes names, and reports independent success/error outcomes. Output helpers
  handle EINTR, short writes and zero progress. Returned output failures are
  nonzero; SIGPIPE follows the caller's inherited policy.
- Returning invocations release owned allocations. Module fixtures exercise
  constructor-NULL, module malloc/calloc/strdup, mkdir/stat/chmod and write
  faults, recovery, borrowed descriptor/stdin-position preservation and 80
  repeated success/error/help invocations. Static security-pattern scan and
  manual source review found no secrets, shell execution or injected code;
  the reviewed C files meet the 80-column rule.
- No actual shell registry exists. Exported cmd_spec plus a thin standalone
  wrapper and direct module dispatch are the real, tested integration seam.

## Actual execution results

All commands ran in `/home/name/Documents/420-EMBDX/A`, unless marked scratch.
The reviewer ran as uid 1000, so the CLI permission-denial fixture executed.

| Command/check | Actual result |
| --- | --- |
| `make -B test-mkdir-impl` | Fresh CLI/module builds; 55 project fixtures and module suite passed, exit 0 |
| `make test-mkdir` | 26 GNU-reference common-subset fixtures passed, exit 0 |
| `make test && make all` | Complete working-tree regression passed; all targets available, exit 0 |
| `make -B all` | All working-tree delivery binaries freshly compiled, exit 0 |
| `make lint` | Working-tree clang-tidy passed, exit 0 |
| `clang-format --dry-run --Werror src/cmd_mkdir.c src/cmd_mkdir.h src/mkdir_main.c tests/test_mkdir_module.c` | Scoped formatting passed, exit 0 |
| `make format-check` | Exit 2: only the seven inherited files below failed |
| `python3 tests/test_mkdir.py /usr/bin/true` | Intended negative control failed directory-existence assertion at line 40, exit 1 |
| Independently rebuilt ASan/UBSan CLI and module | 55 fixtures and module suite passed; leak detection enabled; all compile/run exits 0, no sanitizer findings |
| Additional seeded differential exercise | 128 `-p` scenarios against `/usr/bin/mkdir` passed; compared success/failure, stderr presence, complete entry types/modes/link targets/sentinel bytes under seven umasks |
| Scratch HEAD-plus-mkdir: `make test-mkdir test && make -B all` | Reference, HEAD regression plus mkdir, and fresh delivery build passed, exit 0 |
| Scratch HEAD-plus-mkdir: `make lint` | All 14 prospective C sources passed, exit 0 |

The differential exercise used seed 420, disposable scratch trees and bounded
subprocess deadlines. It included repeated/trailing slashes, dot/dotdot,
existing directories, symlink traversal, dangling links, file components and
oversized names. It supplements, rather than replaces, fixed expected fixtures.

Exact independent build/lint/sanitizer commands and output are saved under
`/home/name/.hermes/cache/scratch/mkdir-independent-review-20261008/`:
`targeted-reference-regression.txt`, `delivery-build.txt`, `lint.txt`,
`scoped-format.txt`, `global-format.txt`, `negative-control.txt`,
`sanitizers.txt`, `differential.txt`, `static-scan.json`,
`prospective-test-build.txt`, and `prospective-lint.txt`.
Sanitizer builds used `-O1 -fno-omit-frame-pointer
-fsanitize=address,undefined -fno-pie -no-pie`; runtime settings were
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. The full linker wrapper list is in
`sanitizers.txt` and the existing Makefile module target.

## Limits and non-blocking concerns

- Saved implementation log sections 05–07 show actual internal argtable3 OOM:
  process exit 1, GDB stack `exit <- panic <- arg_lit0 <- arguments_build`.
  This reviewer inspected that evidence but did not repeat the destructive
  internal-library allocation probe. Current module tests intentionally exclude
  dependency-internal allocation faults while testing module-owned recovery
  and constructor-NULL handling. Graceful recovery from *every* OOM is not
  established; an embedding shell could terminate on this dependency failure.
  A future stronger embedding contract requires separate dependency work.
- Global clang-format failures remain in `src/main.c`, `src/traverse.c`,
  `src/cmd_spec.h`, `src/traverse.h`, `tests/test_help.c`,
  `tests/test_recursive.c`, and `tests/test_output.h`. No inherited files were
  reformatted or assertions weakened.
- ASan/UBSan instrument the command, helper and tests, not the installed
  argtable3 static archive. ACL equivalence, hostile concurrent pathname
  replacement, threaded/reentrant parsing, broad fuzzing/coverage and actual
  shell registration are not established or promised by this contract.
- Error messages use inherited-locale strerror. CLI/reference checks use
  LC_ALL=C; non-UTF-8 translated messages in an embedding caller's locale are
  not validated before JSON quoting. Non-C diagnostic locales remain an
  explicitly documented unverified edge, not a stable message schema.
- Scratch prospective-tree results prove the proposed mkdir-only build scope
  works without unrelated untracked commands. They are not a substitute for
  extracting and verifying the parent's exact final index after staging.

## Minimal integration relative to HEAD

Do **not** stage the whole dirty working Makefile or FILES.md.
The tested proposed Makefile is saved as scratch `prospective/Makefile`;
`mkdir-only-Makefile.patch` records its exact diff against HEAD.

Only these Makefile additions are necessary:

1. Append `build/mkdir` to HEAD's `all` prerequisites.
2. Add the existing `build/mkdir` rule, linking mkdir_main, cmd_mkdir and
   already-tracked cmd_lines with argtable3.
3. Add `.PHONY: test-mkdir test-mkdir-impl`, the reference target,
   `build/test_mkdir_module` with its existing fault-wrapper linker flags,
   and `test-mkdir-impl` CLI/module recipes.
4. Append `build/mkdir build/test_mkdir_module` to HEAD's `test` prerequisites
   and its project CLI/module recipes to the test body.
5. Add `$(RM) build/mkdir build/test_mkdir_module` to `clean`.

No cat/head/tail/pwd rules, variables, reorderings or test integrations belong
in this mkdir commit. `src/cmd_lines.c`, `src/cmd_lines.h` and `src/cmd_spec.h`
are already tracked in HEAD and need no new dependency changes.
The scratch tree linked only installed `build/argtable3` and `build/cunit`
dependency directories, not the working-tree binaries or unrelated commands.

For FILES.md, retain HEAD content and append only a mkdir section documenting:

- `docs/commands/mkdir.md` and `docs/commands/mkdir-review.md`;
- `logs/commands/mkdir-20261008-execution.txt` and
  `logs/commands/mkdir-20261008-interrupted.txt`;
- `src/cmd_mkdir.c`, `src/cmd_mkdir.h`, `src/mkdir_main.c`;
- `tests/test_mkdir.py`, `tests/test_mkdir_module.c`.

The proposed registry is scratch `mkdir-only-FILES.md`, with exact
`mkdir-only-FILES.patch`. Parent command-commit allowlist: these nine new
paths plus mkdir-only Makefile/FILES.md changes. Exclude sessions.txt and all
unrelated dirty/untracked paths. Verify the final staged tree before commit.

## Files changed by this review

Created this report. Added its one registry entry to working FILES.md without
replacing unrelated entries. Registered verified current assignment session
`20261008_184129_607d91`, title
`Subagent: Retry independent mkdir review after provider…`, in docs/sessions.txt.
Builds changed generated binaries only; supporting artifacts are in scratch.
No AI_LOG content read, source fixes, staging, commits, next-command work,
pushes, publication or wiki changes were performed.
