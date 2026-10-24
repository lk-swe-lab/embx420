# Current status

## Current packaging implementation

The registry has 19 builtins: 18 adopted baseline commands plus `pkg`.
Seventeen baseline builtins have standalone wrappers; `cd` and `pkg` do not.
`rmdir` is adopted baseline work, not a new packaging command. The separate
`build/hello` is package-only and becomes available in main only after install.
The current baseline intentionally retires `env`, `echo`, `date`, `printf`
and `sleep`; packaging does not restore them.

See [current commands](../README.md#supported-commands), the brief
[package guide](commands/pkg.md), [release guide](reports/shell-release.md)
and [plan](reports/packaging-plan.md). Local install/list/info/remove, refreshed
installed help/catalog, and recorded-path fork/exec are implemented. No general
PATH fallback, remote download, dependency solving or shell self-update is added.

## Evidence and open delivery gates

- Backend follow-up reports 44 normal and 44 ASan/UBSan tests and six sensitive
  safety mutants. Source/root overlap and command-name bounds were fixed.
- Test review fixed isolated shell HOME handling and builder test sensitivity.
  The Make openat-driver prerequisite gap was then fixed; its owner reports
  full strict `make test all` and `make lint` passing. See
  [backend evidence](reports/pkg-implementation.md) and
  [integration evidence](reports/package-shell.md).
- Release review found a provenance blocker: an unrelated generator could supply
  help/catalog for another shipped binary. Repair was assigned. The release
  owner now reports removal of that override, exact-byte snapshot generation,
  a passing regression and a current-artifact independent rerun. See
  [release evidence](reports/shell-release.md); this does not finalize delivery.
- The [lifecycle demo](../demo/PACKAGING.md) records 17 commands and 21 checks,
  including the relocated release. This is not a full regression claim.
- Parent reports the private wiki article `_drafts/aishell-packaging.md`
  delivered as DRAFT, revision 1; exact readback and validation passed.
  Human review is pending. Wiki data remains excluded from Git and push.
- Parent verified the exact runtime/build index and pushed commit
  `6f579356abd67ceb1f7123db53f55896c247b967` to `feat/aishell-packaging`;
  parent reports the remote hash matches. Local Git confirms tree
  `9b457a89b41217ad1dd8572dad968bf6734ca555` and 92 source/build/test paths.
  This includes the retired-command and adopted-rmdir baseline closure.
- Parent exact-tree checks: strict `make test all`, release tests/build,
  `make lint` and changed-C formatting returned 0. Global formatting returned 2
  on unchanged files. Evidence: `$TMPDIR/packaging-code-index-checks`.
- Docs, demo and the present dependency-bootstrap candidate are not yet committed.
  Their final checks/publication and remaining acceptance dispositions are open.
  No task completion or merge is claimed; the user merges later.

## Final acceptance

The parent accepted the milestone and extra shell release. See
[verification and limits](reports/packaging-verification.md). Independent
bootstrap, documentation, and demo review passed after the checkpoint above.
Fresh pinned dependency builds and strict full-project tests passed in isolated
snapshots. The brief guide and private wiki draft are delivered. Final scoped
documentation publication is separate; the user will merge the feature branch.
Global formatting failures in unchanged files remain disclosed.

## Historical checkpoints

October 9, 2026 checkpoint `e1c3ac9` described 22 builtins and 21 wrappers,
with `rmdir` skipped. That is history, not the present working tree or a final
packaging commit. Old reports retain their original findings and gate text.
Some old guide/report links refer to intentionally retired files and can be
stale. Do not use them as current usage instructions.

These selected command subsets are not full GNU replacements, a POSIX shell
or a sandbox.
