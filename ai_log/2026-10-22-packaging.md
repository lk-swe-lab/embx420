# Packaging decisions — 2026-10-22

## Authority and coordination

- The user authorized autonomous implementation, routine decisions, tests, builds, lint, log maintenance, all planned delivery steps, and local commits. The user now permits push to `feat/aishell-packaging`; the user will merge later. No merge is authorized.
- The parent coordinates workers with disjoint file ownership, separate independent review, final verification, and local commits. The user approved temporary `subagent_auto_approve: true`; the parent must restore the prior setting at task end. This does not expand file ownership or authorize a merge.
- Use current working files as the baseline. Preserve unrelated edits and deleted commands. This log is a new decision record, not a transcript or import of older AI logs.

## Selected decisions

- First command/package: `hello`, version `0.1.0`; source manifest `packages/hello/pkg.json`. A small command limits the first metadata integration.
- Use local directory packages, manifest-owned payload, schema version 1, `metadata/commands.json`, and generated `docs/<command>.md`. Keep the contract’s initial bounds. Allow safe listed support files; no dependency solver, second database, or large framework.
- Use a small optional metadata visitor and shared logical argument definitions. Delegate its internal C signature and ownership details to the implementation owner; do not migrate all builtins.
- All package snapshot/read/write operations use one nonblocking exclusive root lock. Removal alone allows missing payload in ANY package, so two partial packages do not block each other. Manifest/path/type/collision checks remain mandatory.
- Update command packages by removal then installation; failed replacement can leave them unavailable. No shell self-update.
- After the milestone is verified, create a separate full-shell `aishell` release, version `0.1.0`, with existing builtins and generated docs/catalog. Provide separate shell and command-package install/update instructions. Verify a clean relocated artifact outside source/build under an isolated HOME before claiming delivery.

## Evidence at this coordination checkpoint

- Parent-reported observation: `hello` help was RED, exit status 2; usage/metadata markers were missing. This is intended failure evidence, not a pass.
- No source implementation is recorded as completed. Complete test, build, lint, and local-commit results are not yet verified at this checkpoint.
- This documentation task updates only the plan, contract, this new log, and packaging links in `FILES.md`. It runs no code tests, stages nothing, and makes no commit.
- Pending: implementation evidence, named acceptance cases, independent review, lifecycle/regression gates, demo and usage delivery, then the full-shell artifact and relocation checks. Append actual commands/results as they become available; do not infer success.

## Hello review follow-up

- Parent verification passed: `/home/name/.hermes/profiles/embx420/cache/scratch/hello-parent-69PSJz`.
- Independent local review passed with no blocker: `/home/name/.hermes/profiles/embx420/cache/scratch/hello-review-36l1zv9l` (1,445 grammar edges, 2,187 environment cases, normal/sanitized fault probes).
- Reuse one long-help constant. Explicitly support Argtable unambiguous long prefixes, retaining canonical metadata/help names. Initial abbreviation tests passed unchanged code: characterization, not RED.
- A separate regression exposed `--n`/`--na`/`--nam` with separated `-h-` values: three failures, status 2 instead of 0. Extended only the existing safety guard's value skip; Argtable still parses options. Regression then passed.
- Fresh evidence: `/home/name/.hermes/profiles/embx420/cache/scratch/hello-low-fixes-kfozfmp0`. Run `python3 "$TMPDIR/hello-low-fixes-kfozfmp0/verify-resume.py"`: strict builds, 12 CLI tests per normal/sanitized build, five module modes per build, scoped format/lint/whitespace all passed. Earlier evidence was preserved.
- Local hello slice reviewed; plan item 2 remains incomplete for shared export, generated docs, and builtin integration. Follow-up changes are unstaged; no commit or push. Full packaging/release gates remain open.

## Required documentation split

- The user requires two separate ASD-STE100 English documents: a brief package usage guide with main design ideas at `docs/commands/pkg.md`, and an in-depth private wiki article on how we build the system (exact path TBD).
- Explain implementation-backed learning and verified results. Mark planned work as planned; make no unimplemented feature claims or unsupported compliance claims.
- Before a wiki write, the parent must read `wiki/skills.md`, query the wiki database, and resolve the exact edit scope, index, and log API. This follow-up writes no wiki content. Keep the wiki private; do not include it in a branch push.

## Earlier coordination update

- Item 3: independent builder review found a data-loss publication blocker.
  Repair and publication-failure tests are assigned. Review must run again.
- Item 4: the runtime owner reports 39 tests passed. Independent review is
  pending. This report does not close the item.
- Item 5: shell integration is assigned. Its verification is pending.
- Existing builtin compatibility: 15 positional specifications now set the
  optional metadata callback to `NULL`. Only initializer layout also changed.
  Designated initializers need no change. No in-scope test specimen needs a
  positional fix; `tests/test_shell_module.c` belongs to integration.
- All 15 modules failed strict compilation before the change with a missing
  `visit_metadata` initializer. All now compile to private scratch objects.
  Flags: `-std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow
  -Wconversion -Wstrict-prototypes -Werror -g3 -O0
  -Ibuild/argtable3/include`. Run `cc <flags> -c <file> -o <scratch-object>`.
- Scoped `clang-format --dry-run --Werror`, analyzer-only `clang-tidy` with
  warnings as errors, the 80-column check, and `git diff --check` passed.
  A comparison with each saved input proved no change outside its initializer
  and no initializer token change except the appended `NULL`.
- Evidence: `/home/name/.hermes/profiles/embx420/cache/scratch/metadata-initializers-o8b3rmqx`.
  `results.json` records exact commands and statuses. No global build or test
  ran in this slice. No stage, commit, push, or merge was made.
- All delivery checkboxes remain open. The parent owns final verification,
  publication, and restoration of the temporary auto-approval setting.

## Review and integration checkpoint

- Builder repair uses immutable output directories. Identical output is a no-op;
  changed or unlisted output is preserved and rejected. Worker reports 16 builder
  tests passing, including source overlap and prior-output preservation. Evidence:
  `$TMPDIR/build-publication-fix`. Independent review is assigned.
- Runtime review passed 39 normal and 39 sanitizer tests, independent JSON and
  filesystem probes, and fault checks. It found one blocker: `HOME=SOURCE`
  installation creates root files inside the source. Repair is assigned.
  Evidence: `$TMPDIR/pkg-review/review.md`.
- Shell integration worker reports 13 package-shell tests, 33 existing shell
  tests, 39 runtime tests, and `make test all` passing. Independent review is
  assigned. Evidence: `$TMPDIR/package-shell-evidence`.
- Runtime repair also aligns the 60-byte command limit and checks the variadic
  fault-harness analyzer finding. Existing global formatting failures remain
  separate from owned-file checks.
- The extra full-shell release builder is assigned. Final milestone checks,
  demo, wiki draft, exact commit-tree verification, and publication remain open.
- These are worker reports unless explicitly identified as parent verification.
  No packaging commit or merge is claimed.

## Latest review fixes and delivery state

- The current registry has 19 builtins: 18 baseline commands plus `pkg`.
  There are 17 builtin wrappers; `cd` and `pkg` have none. `hello` is package-
  only. `rmdir` is adopted baseline work. Intentional retirement of `env`,
  `echo`, `date`, `printf` and `sleep` is preserved, not reversed from HEAD.
- Package output is immutable. The builder rejects changed/unlisted targets
  and source/output overlap. Follow-up tests also fix independent docs-path
  sorting, the 60-byte command-name bound and prior-output test sensitivity.
- Backend overlap repair checks physical ancestry before store creation.
  Snapshot ownership is explicit. Follow-up normal and ASan/UBSan suites each
  report 44 passing methods; six safety mutants fail their intended assertions.
  The openat seam now forwards `O_TMPFILE` mode. Its analyzer false positive
  has one justified local suppression, not a disabled global check. Evidence:
  `$TMPDIR/pkg-fix`; [backend report](../docs/reports/pkg-implementation.md).
- Shell discovery/help/catalog and recorded-path fork/exec are real. Test review
  fixed all shell launch sites to use isolated HOME. The original poisoned-HOME
  failure is preserved; 34 shell tests now pass. Builder follow-up reports 18
  tests; hello has 12 CLI tests plus module/metadata checks; package-shell has
  13 tests. Evidence: `$TMPDIR/test-review-fixes` and
  [integration report](../docs/reports/package-shell.md).
- The full test attempt then found a missing Make prerequisite for the openat
  driver. Repair was assigned and completed by its owner, who reports full
  strict `make test all` and `make lint` passing. Evidence:
  `$TMPDIR/pkg-make-closure`. This is not final exact-index verification.
- Release review found a provenance blocker: an unrelated generator could
  supply invented help/catalog while another binary shipped. Repair was
  assigned. The release owner now reports removal of `--generator`, execution
  of an exact-byte binary snapshot, a passing regression and an independent
  rerun on the current relocated artifact. The 12-test release suite and
  scoped checks pass per [release report](../docs/reports/shell-release.md).
  Earlier stale-snapshot results are not approval of changed source.
- The [lifecycle capture](../demo/PACKAGING.md) records 17 actual commands and
  21 checks with status 0 overall, including the relocated release. The intended
  hello lookup after removal returns 127. This is not full-regression evidence.
- Parent reports the private wiki database article `_drafts/aishell-packaging.md`
  delivered as DRAFT revision 1. Body/metadata readback matched exactly;
  index/log are revision 2; `WikiStore.validate` returned errors `[]` and
  warnings `[]`. Human review is pending. Wiki data stays out of Git and push.
- README, API/anatomy, shell guide, live status and grouped FILES entries now
  distinguish current implementation from historical checkpoints and retired
  guide links. The dependency bootstrap helper now exists as a candidate; final
  execution checks and publication remain open.
- Parent now reports exact-index verification and push of runtime/build commit
  `6f579356abd67ceb1f7123db53f55896c247b967` to `feat/aishell-packaging`, with
  matching remote hash. Local Git confirms that commit and tree
  `9b457a89b41217ad1dd8572dad968bf6734ca555`: 92 source/build/test paths,
  including retired-command and adopted-rmdir baseline closure.
- Parent exact-tree checks: strict `make test all`, release tests/build, lint
  and changed-C format each returned 0; global format returned 2 on unchanged
  files. Logs: `$TMPDIR/packaging-code-index-checks`.
- Docs/demo/bootstrap are not yet committed. Their final checks, exact index
  and publication remain pending; do not infer whole-task completion from the
  runtime push. All plan checkboxes await parent final acceptance evidence.
  No merge is authorized. Restore temporary auto-approval at task end.
  This reconciliation itself runs no code/tests and stages or publishes nothing.

## Final acceptance and publication preparation

- Parent exact-index checks passed strict `make test all`, release tests/build,
  lint, and changed-C formatting. Global formatting still fails in unchanged
  legacy files. Runtime commit `6f579356abd67ceb1f7123db53f55896c247b967`
  was pushed and its remote hash was read back. No merge was made.
- Independent bootstrap review rebuilt pinned dependencies from checksum-matching
  cached archives. Its fresh strict full-project and release checks passed.
  Safety checks, documentation links and the demo rerun passed. Evidence:
  `$TMPDIR/bootstrap-independent-review`. No fresh network rerun or universal
  platform guarantee is claimed.
- Parent accepted the eight delivery stages with the documented coverage and
  platform limits. The brief guide and private wiki draft are delivered. Wiki
  readback and validation passed; human article review remains pending.
- The unrelated `demo/demo01.sh` has a working-directory defect. It is excluded
  from this delivery. The new packaging demo is independent and passed.
- Parent restored `delegation.subagent_auto_approve` to `false` in the active
  embx420 profile and read it back. `approvals.mode` remains `smart`.
- Remaining publication work at this checkpoint: exact documentation/bootstrap
  index verification and scoped commit/push/readback. See
  [final verification](../docs/reports/packaging-verification.md).

## References

- [Plan and release stage](../docs/reports/packaging-plan.md)
- [Contract and acceptance matrix](../docs/reports/packaging-contract.md)
