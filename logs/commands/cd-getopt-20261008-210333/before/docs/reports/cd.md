# cd implementation report

## Delivery status and authority

The cd-owned module, tests and guide are implemented and scoped checks pass.
**Ready for independent review, not integrated or committed.** No repository
registry, Makefile, main, shell, README, FILES, AI log or wiki was edited by this
worker. Shared integration is supplied as a scratch patch. A concurrently
changed FILES.md was observed and left untouched.

Owned paths: `src/cmd_cd.c`, `src/cmd_cd.h`, `tests/test_cd.py`,
`tests/test_cd_module.c`, `docs/commands/cd.md`, this report.

Session lookup verified `20261008_203032_ade341`, title
“Subagent: Resume and finish cd-owned module/tests/docs…”. Registration is
left to the parent because this delegation explicitly excludes shared writes.

## Exact interruption recovery

Inspected `/home/name/.hermes/cache/delegation/live/deleg_9dd3c349/task-0.log`
and the saved source, header, Python test and report. The old worker stopped
with a runnable no-op module and one persistence test. Its real
`cd-20261008-193837/slice1-red.log` records the expected cwd mismatch; there
was no saved GREEN or complete implementation. The stopped transcript is
truncated in places, so no hidden completion is inferred from its summaries.
The saved no-op binary was rerun and failed the same intended assertion.

Old scratch main/shell/other command sources were frozen into the new scratch
integration tree. No standalone cd binary was substituted for main.

## Sources and selected scope

Read project anatomy, API, coding, design, TDD, verification and add-command
procedures and the command-development/TDD skills. Read-only course lookup:

```sh
.venv/bin/python scripts/lecture_data.py query 'chdir OR "cd"' --limit 5
```

Results include `data/02/CLI_Tools.docx` paragraph 138 (cd) and
`data/02/LSP2_ FileIO.pptx` slide 79 (process chdir). The prior approved
selection records CLI_Tools paragraphs 138–140 including help.

Reference: installed GNU Bash **5.2.21(1)-release**, `help cd`; recorded raw
output in `installed-reference.stdout`. GNU Bash has a builtin cd, not a
standalone GNU coreutils cd. Primary reference locations:

- https://www.gnu.org/software/bash/manual/html_node/Bourne-Shell-Builtins.html
- https://pubs.opengroup.org/onlinepubs/9799919799/functions/chdir.html
- https://www.argtable.org/docs/arg_api_parsing.html

The previous worker recorded failed live fetches. This worker's extractor was
unavailable for its configured search-only backend; targeted searches returned
primary POSIX/Argtable content, and a broader retry returned the GNU manual.
Installed Bash documentation supplies the actual reference-version inventory.
Search provenance is preserved in `research.json`; no third-party discussion
is used as the technical authority.

| Reference feature / workflow | Keep or defer | Reason |
| --- | --- | --- |
| One directory operand, absolute/relative | Keep | Essential cwd change. |
| No operand uses HOME | Keep | Essential default; unset/empty is failure. |
| Physical resolution (`-P` mechanism) | Keep as default, no flag | Kernel chdir; no logical path state. |
| `-L` logical resolution/default Bash behavior | Defer | Requires extra logical state and path rewriting. |
| `-P` selector, `-e` physical-cwd determination check | Defer | No selector or post-change getcwd is needed. |
| `-@` extended attributes as directories | Defer | Platform-specific extension. |
| `-` maps to OLDPWD and prints the destination | Defer | Reject explicit dash; no OLDPWD state. |
| CDPATH search and destination printing | Defer | Deterministic direct pathname use. |
| cdable_vars fallback | Defer | No shell variable expansion/search. |
| PWD/OLDPWD updates | Deliberately omit | Approved physical cwd-only module. |
| Successful silent cwd change | Keep | No data output is needed. |
| Error diagnostics, nonzero result | Keep | Explicit project statuses below. |
| `-h`, `--help`, option terminator `--` | Keep/project contract | Course help and safe dash-prefixed paths. |
| JSON, standalone cd | Omit | No data output; standalone cannot change parent. |

## Contract, ownership and design

Syntax: `cd [-h|--help] [--] [DIRECTORY]`; at most one operand. Argtable3
provides the shared parsing/help definitions. Parser failures precede help;
help otherwise ignores one operand and never performs chdir. Explicit `-`,
including after `--`, is rejected with status 2. `./-` is a literal directory.
HOME is a raw pathname, not option syntax: HOME=`-` enters a literal `-`.
An explicit empty operand or unset/empty HOME fails with status 1.

Status 0 means silent successful change or successful help, 1 means an
operational error, and 2 means invalid syntax/module arguments. Successful cd
has empty stdout/stderr; help goes to stdout; errors go to stderr. Main's
loop reports its last command status, so `cd absent` followed by successful
`pwd` exits 0 while retaining the diagnostic. `exit` preserves the cd status.

The module calls chdir in the invoking process. Kernel symlink traversal and
`..` resolve the physical destination; no realpath/getcwd preflight, PATH_MAX
buffer, CDPATH lookup or logical cwd is maintained. Nothing fallible that could
change the result is performed after successful chdir; cleanup only frees
invocation-owned objects. Failed changes retain cwd.

Each invocation owns parser objects and an argv pointer copy; strings remain
borrowed and are never written/retained. Malformed argc/argv/NULL elements are
rejected before parser access. All ordinary exit paths free the copy/table.
The module opens no descriptors, consumes no stdin, closes no borrowed stream,
changes no signal policy and does not write environment variables. PWD/OLDPWD
may intentionally become stale. Tests change cwd only in subprocesses; the C
harness also saves/restores a cwd fd before removing its fixture.

## Executed RED → GREEN slices

All evidence below is actual subprocess output, not reconstructed output.
Initial failure/return values are in the corresponding JSON/stdout/stderr files.

| Slice | Observed RED | Minimal implementation and GREEN |
| --- | --- | --- |
| 1: persistent absolute/relative cd | Old and resumed no-op binary prints unchanged cwd | chdir in module; persistence test passes. |
| 2: HOME default/unset/empty | No-operand cd leaves cwd unchanged | HOME selection and empty/unset diagnostic; accumulated two tests pass. |
| 3: syntax/help/terminator | `cd -- -dash` does not enter `-dash` | Local argtable definitions, parser, help, explicit-dash rejection; four tests pass. |
| 4: malformed module arguments | argc=0 returns 0 instead of expected 2 | Early guards, including every NULL argv element; direct and integration suites pass. |
| 5: help output failure | /dev/full help returns 0 instead of 1 | Flush/check/clear stdout error, diagnostic/status 1; subsequent successful help passes. |
| 6: raw HOME dash pathname | HOME=`-` is wrongly treated as special explicit operand | Restrict special-dash rejection to explicit operand; five tests pass. |

Kernel path resolution, cwd failure preservation, constructor-NULL handling,
argument-copy allocation failure and syscall faults already passed when added:
these are **characterization/fault checks**, not invented production RED cycles.
The first direct test abort hid its assertion in a temporary stderr capture;
the harness was improved and the intended argc/status failure rerun visibly.

Negative controls: a scratch no-chdir mutant fails the persistence assertion.
An initial direct-argv parser mutant survived because that tested invocation did
not mutate argv. The direct harness now models parser pointer-array reordering
*after real parsing*: the same mutant then fails `argv[i] == original[i]`, and
the unchanged production pointer-copy path passes. This is characterization
sensitivity, not a claimed production bug fix. All mutants remain in scratch.

## Acceptance coverage

| Requirement | Passing named evidence |
| --- | --- |
| Absolute/relative persistence in one main process | `test_absolute_relative_persistence`; raw persistent demo. |
| HOME default, empty/unset, failed cwd unchanged | `test_home_default`. |
| HOME literal dash versus unsupported explicit dash | `test_home_literal_dash`, `test_syntax_help_no_effects`. |
| Spaces, dash-prefixed paths, `--`, cardinality and invalid options | `test_syntax_help_no_effects`. |
| Help aliases, operand/no-effect, registry usage parity | `test_syntax_help_no_effects`; direct usage callback. |
| Symlink entry and both subsequent/combined `..` | `test_physical_and_failure_preservation`. |
| Missing/file/non-directory-component/symlink-loop failures | `test_physical_and_failure_preservation`. |
| Permission/path-length errors, recovery after failure | `test_permission_and_path_length_errors`; euid 1000, real mode-000 failure exercised. |
| Discovery and last-status/exit propagation | `test_registry_discovery_and_last_status`. |
| Repeated success/error/help/default, unchanged argv strings/pointers | C module harness: 100 loops, 400 repeated calls, real parser plus pointer-permutation seam. |
| Unchanged HOME/PWD/OLDPWD/CDPATH on repeated/fault paths | C harness assertions; integration ignores stale PWD/CDPATH. |
| Borrowed descriptors remain open; stdin position unchanged | C harness fcntl/lseek checks with real redirected input. |
| Constructor/copy allocation failure and released resources | Three constructor-NULL seams, copy malloc failure, tracked allocations returning to zero. |
| Permission/I/O syscall faults with cwd unchanged | Injected chdir EACCES and EIO; no fabricated chdir success. |
| Real help-output error and reuse after failure | Direct /dev/full then successful help; stdout error indicator cleared. |

## Verification, exact evidence and rerun commands

Evidence root:
`/home/name/.hermes/cache/scratch/cd-resume-20261008-203032`.
`evidence.json` contains complete invocation arrays/statuses, source/dependency
hashes, sanitizer environment and shared-file before/after hashes. Each label
has separate complete `.stdout`, `.stderr` and `.json` files. Scratch can be
pruned after inactivity; the parent should preserve approved evidence before
publication. `persistent-demo.json` stores real input, cwd, exact expected and
actual output, diagnostic and status for a single-process cd/pwd/error sequence.

Actual final scoped commands (all status **0**):

```sh
S=/home/name/.hermes/cache/scratch/cd-resume-20261008-203032
make -C "$S/tree" test-cd
make -C "$S/tree" lint C_SOURCES='src/cmd_cd.c tests/test_cd_module.c'
make -C "$S/tree" format-check \
  C_FILES='src/cmd_cd.c src/cmd_cd.h tests/test_cd_module.c'
git apply --check "$S/integration.patch"
```

`test-cd` runs **7 Python integration tests** and the direct C harness. Normal
compilation uses the project's C17 warnings including conversion/shadow/format
checks. Scoped analyzer and formatting pass; owned C/header lines are at most
80 columns. Python syntax compilation passes with its cache confined to scratch.
No unrelated global lint/format/test pass is claimed.

Actual ASan/UBSan builds and direct/integration runs pass. Compiler flags:
`-fsanitize=address,undefined -fno-omit-frame-pointer`, plus the normal project
flags. Runtime: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Exact build arrays are in the
`delivery-san-*.json` records. Clang was not installed; GCC/cc was used instead.
The prebuilt argtable3 archive is not sanitizer-instrumented. The diagnostic
`cd: help output failed` in the direct harness is intentional fault evidence,
not an unexpected regression. Sanitizers reported no errors in these runs.

After parent integration, the repository rerun command is:
`TMPDIR=/home/name/.hermes/cache/scratch make test-cd`.
No standalone cd target is needed. Exhaustive fuzzing, threaded invocation,
complete Bash parity and unrelated full-project regression are not claimed;
this process-wide cwd builtin is intended for the serial shell loop.

## Allocation limits requiring review acknowledgement

Module-owned copy allocation failure returns 1 and frees parser state.
Injected constructor-NULL boundaries also return 1 and free surviving objects.
**These do not establish recoverable real argtable-internal OOM.** Executing
`tree/build/test_cd_module --internal-oom` with a real constructor's malloc
forced to NULL prints `Out of memory!` and exits **1** inside the library,
without returning to the module. The shell can therefore terminate under
internal-library OOM; this is an observed dependency limitation, not recovery.
The parent/reviewer must acknowledge this limit before claiming acceptance.

The void `print_usage` callback likewise has no status channel for simulated
constructor-NULL failures: it releases surviving objects and emits no help;
main's existing generic help dispatch cannot distinguish that from success.
Actual parser-internal OOM still follows the dependency's exit policy. Normal
usage parity, borrowed stream preservation and callback cleanup are verified.
This limitation is explicit rather than silently counted as successful help.

## Exact shared integration handoff

`integration.patch` changes only `src/registry.c` and `Makefile`; its application
was checked read-only against the repository. It adds cmd_cd_spec to the builtin
registry, adds cd source/header to main's existing shell dependencies, supplies
the wrapped C test build and `test-cd`, adds it to `test`, and removes the test
binary in `clean`. Main/shell code needs **no cd special case**: current dispatch
already invokes module callbacks in-process. Scratch integration uses the same
registry change and frozen main/shell sources.

Do not apply blindly over another worker's later integration; merge the cd
entry/dependencies/test rule if context changed. Runtime main/shell/registry and
Makefile hashes remained unchanged during this worker. FILES.md changed
concurrently. No staging, commit, push, wiki or AI-log operation was performed.
Independent review, parent-owned integration and final combined regression
remain outstanding.
