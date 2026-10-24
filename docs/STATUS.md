# Current status

## Shared-module working tree

The current host is `build/aishell` (`build/main` alias), with 22 standard `.so`
commands and no statically linked command fallback. Seventeen retain standalone
wrappers. Help, exit, cd, pkg and traverse are modules too; hello is package-only.
Standard modules load eagerly at startup, intentionally not lazily. All callbacks
run sequentially in the parent; no threads or process sandbox.
`env`, `echo`, `date`, `printf` and `sleep` remain retired.

Native hello 0.2.0 builds at `build/native-candidates/hello-0.2.0`; the complete
release is `build/so-reviewed-releases/aishell-0.2.0`. Live pkg accepts schema 2
shared modules, not executable schema-1 fixtures. Install selects immutable versions;
`pkg use NAME VERSION` selects retained versions; remove deactivates, retaining
payloads. Replacing an already published version requires restart. See
[packages](commands/pkg.md), [shell](commands/shell.md), and
[approved design/current implementation](reports/so-redesign.md).

Native install/use compare stored metadata with the actual spec exporter before
selection, through the appended size-gated ABI-v1 `inspect_command` callback.
Builders refuse stale/unknown/unsafe module outputs before compiling; releases
require the exact standard inventory derived from source specs. A post-commit
close error reports `selection committed; close failed`, returns 1 and emits no
success; inspect pkg info before retry. Earlier native-packages/so-releases
outputs remain immutable historical artifacts, not the current demo inputs.

## Verified working-tree evidence

The [demo](../demo/PACKAGING.md) ran against existing real build outputs:
**11 processes, 22 loop inputs, 27 checks**, exit 0. It proves live install/run/
remove/reselect in one shell, fresh-session selection, and a relocated complete
release. All **80 input SHA-256 entries** (23 host/modules, 4 package, 53 release)
were unchanged; release bytes matched the current build/package. The runner
itself launched from unrelated cwd with isolated HOME and PATH=/nonexistent;
inner/outer cleanup passed. This is not a full regression, ABI audit,
distinct-version replacement test or final review.

Parent execution of `make -j1 all test lint release-shell` passed after the
review repairs. Independent review reran module/build/release and native fault
checks; its remaining numeric-equivalence finding was reproduced and repaired
with failing-then-passing install/use tests. Native metadata accepts schema-valid
`-0` counts as equivalent to `0`, without relaxing string/type/grammar checks.
The native transaction suite exercises 8 tests and 212 measured project
allocation failure points. Focused ASan+UBSan passes are retained in scratch
evidence. Global formatting still fails on five unchanged baseline files;
changed C formatting and `git diff --check` pass.

Verification logs and source-snapshot results are recorded in
[the implementation report](reports/so-redesign.md). No commit, push or merge
has been performed for this redesign.

## Limits and history

Trusted native constructors may run during install validation. Native callbacks
can terminate/corrupt the shell. Linux `/proc/self/exe`, `renameat2`, compatible
ELF/libc and dependency libraries are required. Active selection is atomic, not a
full crash-durable journal/repair facility. There is no purge, remote repository,
dependency solver, signature verification or self-update. Retained versions
accumulate; never alter mapped payload bytes in place.

Old reports are historical snapshots of executable packages/static registries,
not current usage or pending-commit instructions. No historical report was
rewritten for this migration. These command subsets are not full GNU tools or
a POSIX shell.
