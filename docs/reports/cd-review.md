# cd independent review

## Verdict

**Selected behavior passes; unconditional module-ownership approval is held.**
One medium-severity finding remains: parsing overwrites the host's exported
getopt globals. Correct it with regression tests, or explicitly accept and
record this limitation before delivery. No source fix was made. Repository
main still does not register cd; scratch integration is not final integration.
The supplied integration patch also needs parent-owned shell-test updates:
adding cd produces three exact-help failures and the C registry-count abort.

Reviewed unchanged bytes: `src/cmd_cd.c`, `src/cmd_cd.h`, `tests/test_cd.py`,
`tests/test_cd_module.c`, `docs/commands/cd.md`, `docs/reports/cd.md`.
SHA-256 values, exact commands, statuses and complete raw output are in
[`evidence.json`](../../logs/commands/cd-review-20261008-205019/evidence.json).

## Finding: exported parser-global ownership

`src/cmd_cd.c:72` calls arg_parse without saving/restoring `optarg`, `optind`,
`opterr`, or `optopt`. The installed static archive exports these symbols and
getopt/getopt_long. A fresh probe sets pointer and integer sentinels before
successful `cd .`, invalid `cd --bad`, and help. All three overwrite state:

| Call / status | optind (was 41) | opterr (was 73) | optopt (was 89) | optarg pointer preserved |
| --- | --- | --- | --- | --- |
| `cd .` / 0 | 2 | 0 | 89 | no |
| `cd --bad` / 2 | 2 | 0 | 0 | no |
| `cd --help` / 0 | 2 | 0 | 89 | no |

Probe exits **1**, intentionally reporting failed preservation; see
`global_probe.c`, `global-probe.stdout`, and `global-probe.stderr` in the
[evidence directory](../../logs/commands/cd-review-20261008-205019/).
The existing C tests assert argv/environment/descriptor ownership but never
seed or inspect these globals. Their green result does not cover this finding.

Upstream parser source also explicitly resets optind and opterr before its
getopt loop.[2] That live master source is explanatory, **not provenance for
the installed archive**; the installed-archive hash and actual probe are the
acceptance evidence. This is not a demonstrated serial-shell dispatch failure:
ordinary persistent dispatch passes. It is a host-state borrowing defect if
module-global preservation is required. Some existing modules also lack a
save/restore guard; they are outside this review's editable scope.

Suggested disposition: add nondefault sentinel assertions for success, help
and parse errors; restore the four exported values immediately around parsing.
Do not claim that restoring them preserves getopt's private interrupted-scan
state, or makes process-wide cwd changes/threaded parsing safe. If the parent
chooses a narrower contract instead, document that choice rather than presenting
these tests as proof of complete global isolation.

## Fresh executed checks

All builds/tests used a new scratch copy of the **current** repository sources,
not the implementer's binary. Only the scratch registry/Makefile received the
handoff patch. The snapshot and patch are preserved in the evidence directory.

| Check | Actual result |
| --- | --- |
| `git apply --check` against repository | 0; read-only, patch currently applies. |
| Scratch `make test-cd` | 0; 7 Python main-based tests plus C module harness. |
| Module reuse | 100 loops / 400 repeated calls pass; malformed argc/argv/NULL elements also pass. |
| Module fault/resource checks | Three constructor-NULL seams, owned-copy malloc failure, chdir EACCES/EIO, /dev/full help failure/reuse pass. Wrapped allocations return to zero; inherited descriptors remain open and stdin position is unchanged. |
| Environment/argv | HOME/PWD/OLDPWD/CDPATH strings unchanged; caller strings and pointers preserved despite post-parse pointer permutation seam. |
| Main persistence/HOME/syntax/status | Absolute/relative paths, unset/empty/literal-dash HOME, spaces, `--`, cardinality, help parity, discovery and final-status propagation pass. |
| Real filesystem failures | euid 1000: mode-000 EACCES exercised, not skipped; missing path, file/non-directory component, symlink loop and long path pass. Each tested failure retains cwd. |
| Physical traversal | Symlink entry, following `..`, and combined symlink/`..` pass. |
| Independent persistent scenario | One real main process: symlink, `..`, EACCES/ENOTDIR/ELOOP/ENOENT then HOME; exact stdout and diagnostic categories pass. |
| Bounded state-path property check | 100 deterministic absolute success/failure transitions in one main process; every physical pwd matches independent expected state. |
| Negative controls | No-chdir mutant rejected with intended cwd assertion (exit 1); direct-borrowed-argv mutant rejected with pointer-identity assertion (SIGABRT / -6). |
| Scoped analyzer / format / Python syntax | All 0; clang-tidy on owned C files, clang-format dry-run on owned C/header, py_compile with scratch cache. |
| ASan/UBSan | Fresh instrumented main and module harness builds/run pass; all 7 integration tests pass again. No sanitizer error reported. |

Sanitizers: `-fsanitize=address,undefined -fno-omit-frame-pointer` plus project
C17 warnings/debug/O0 flags. Runtime `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The prebuilt argtable
archive is **not** instrumented. Expected `cd: help output failed` is the
intentional /dev/full check, not an unexplained sanitizer diagnostic.

Every cwd-changing module invocation ran in a subprocess. Python uses child
`cwd=...`, never os.chdir; the C harness saves/restores a cwd fd and cleans its
fixture. Both review drivers assert their own cwd is unchanged; a final live
`pwd` remains `/home/name/Documents/420-EMBDX/A`. No tests run a builtin directly
inside the parent agent process. No unrelated full-project pass is claimed.

## OOM, cleanup and documentation

- Real constructor-internal malloc failure freshly prints `Out of memory!`
  and exits **1 inside argtable**, rather than returning to cd. The shell can
  terminate under dependency OOM. Constructor-NULL seams and module-copy OOM
  recovery do not establish recoverability of that real failure.
- Void `print_usage` cannot report constructor-NULL failure; its surviving
  objects are freed, but help can be empty without a failure status. This is
  an acknowledged API/dependency limit, not a passing operational-help result.
- Parent acknowledgement of both limitations is still required. A guarantee of
  recoverable all-OOM behavior would block delivery; this review does not make
  that guarantee or authorize changing the shared library/API.
- Parser tables/copy are invocation-owned and freed on ordinary paths; strings,
  HOME and streams are borrowed. No owned fd is opened by cd. A single chdir
  determines success; no fallible post-success cwd lookup can convert a changed
  cwd into an error. Static review found no secret/injection/exfiltration issue.
- Documentation matches physical cwd-only behavior and deliberately unchanged
  PWD/OLDPWD, ignored CDPATH, unsupported explicit `-`, and no standalone.
  Its scratch-only integration status is accurate. Existing TDD evidence was
  inspected/preserved, not relabeled as this review's production TDD cycle.
- No exhaustive fuzzing, 32-bit portability run, threaded contract, all internal
  allocator fault combinations, or unrelated full-project regression is claimed.

## Integration regression blocker

Fresh unchanged eight-command baseline: **19/19 Python shell tests pass** and
C shell module passes. With only the cd registry/build patch, the Python suite
runs 19 tests with **3 failures**, all exact-help expectations:
`test_dispatch_help`, `test_loop_multiple_and_eof`, and
`test_loop_stdin_not_read_ahead`. The C shell module aborts at
`tests/test_shell_module.c:131`, `count == 8` (now nine builtins).

These are expected registry-inventory maintenance, not a cd runtime failure,
but they block final `make test`. The patch does not update them. Parent must
separate the builtin help inventory from standalone-parity command names in
`tests/test_shell.py` (cd has no standalone), and update the C count/discovery
assertions. Preserve standalone parity for the original eight commands.
Raw baseline and added-cd results are in `baseline-shell-regression.*`,
`baseline-shell-module.*`, `shell-regression-added-cd.*`, and
`shell-module-added-cd.*`.

The first shell-suite attempt omitted `tests/fixtures/tree` from scratch and
had an additional legacy fixture failure; that was setup, not a source defect.
Both initial outputs are preserved under `*-missing-fixture-setup.*`.
After copying the real fixture without modifying tests, the fresh baseline
passes and only the three inventory failures remain with cd.

Independent unset, empty, missing-directory and file-valued HOME checks also
run `cd` followed by `pwd`: each reports an error and retains physical cwd.
The extractor backend could not fetch reference pages; direct curl retrieved
the primary upstream source used for the explanatory citation.

## Evidence and integration plan

Durable owned directory:
`logs/commands/cd-review-20261008-205019/` contains raw subprocess output,
metadata/source hashes, review drivers, a frozen source/test/Makefile snapshot,
probe/mutants, patch, and selected original worker evidence. The latter is
clearly separated under `prior-worker/`; it is not fresh reviewer execution.
The original worker's command-owned source hashes match this review's hashes.
Scratch work: `/home/name/.hermes/cache/scratch/cd-review-20261008-205019/`.

For the recorded scratch tree, exact checks were:

```sh
T=/home/name/.hermes/cache/scratch/cd-review-20261008-205019/tree
make -C "$T" test-cd
make -C "$T" lint C_SOURCES='src/cmd_cd.c tests/test_cd_module.c'
make -C "$T" format-check C_FILES='src/cmd_cd.c src/cmd_cd.h tests/test_cd_module.c'
```

Parent integration sequence:

1. Resolve the parser-global finding and acknowledge dependency/usage OOM limits.
   If fixed, refresh reviewed hashes and rerun the sentinel probe and scoped suites.
2. Merge the supplied registry include/entry, main source/header dependencies,
   wrapped module-test target, `test-cd`/aggregate-test prerequisites and clean
   rule. Recheck patch context; do not overwrite concurrent integration. No cd
   standalone or special case in main/shell is required.
3. Update parent-owned shell-test inventory/count expectations as described
   above; separately update user-facing discovery/documentation status.
   Approve publication/index changes separately.
4. Run real integrated `TMPDIR=/home/name/.hermes/cache/scratch make test-cd`,
   accumulated `make test`, relevant lint/format, sanitizer and persistent
   main checks against the final combined tree before claiming ready/committing.

Only this report and its owned evidence were created. No repository source,
registry, Makefile, README, FILES, report index, AI_LOG, wiki, staging, commit or
push changes were made. Session lookup verified `20261008_205019_2a746d`, title
“Subagent: Independently review cd implementation/tests/doc…”. Registration
was deferred to the parent because this delegation forbids shared-file writes;
`docs/sessions.txt` was not changed.

## Sources

[2] https://raw.githubusercontent.com/argtable/argtable3/master/src/argtable3.c — Argtable upstream parser source (master; not installed archive provenance)
