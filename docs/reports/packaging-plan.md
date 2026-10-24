# Week 4 packaging plan

## Scope and authority

Add local package management to the Week 1–2 shell in A. Do not change the separate Week 3 shell. Use current working files as the baseline, including intentional additions and deletions. Do not restore commands from HEAD.

The user authorized autonomous implementation, routine design decisions, tests, builds, lint, log maintenance, all planned delivery steps, and local commits. The user now permits push to `feat/aishell-packaging`; the user will merge later. No merge is authorized. The parent coordinates disjoint implementation owners, separate independent review, final verification, and local commits. Preserve unrelated dirty changes. No repeated user approval is needed for routine decisions within this scope.

Source: `04/Session 4_ Package Management-20260928T012617Z-1-001/Session 4_ Package Management/`, relative to the course root:

- `Session 4_ Package Management_updated.pptx`: slides 6–8, 10–18.
- `PackageManagement.ipynb`: Markdown cells 4–5, 10–12, and 17.

The milestone covers local operations, generated docs, a valid catalog, lifecycle evidence, a demo artifact, and a decision log. Directory packages are the selected design. A separate full-shell release follows the verified milestone. No remote downloads, dependency solving, or self-update.

## Decisions

- The first command and command package are `hello`, version `0.1.0`. Keep its authoritative manifest at `packages/hello/pkg.json`. Generate `build/packages/hello-0.1.0/`.
- Use the [contract](packaging-contract.md), including schema version 1, bounded input, `metadata/commands.json`, and `docs/<command>.md`. Safe listed regular support files are allowed. Do not add a framework or dependency.
- One command definition supplies parsing, help, metadata, and generated docs. Use a small optional metadata visitor; migrate `hello` first, not all builtins. The implementation owner can settle its internal C signature and document ownership.
- Make builds binaries and generates output. `pkg` never compiles source. Keep source, generated output, and installed files separate.
- Install under `~/.mysh/pkgs/<name>-<version>/`. The installed manifest is the record; no second database. Reject duplicate package names and command collisions, including builtins and controls.
- Validate paths, manifests, metadata, and payload before publication. Use a private same-root stage and rename. Do not follow symlinks or publish partial copies.
- Use one nonblocking exclusive root lock for every package snapshot/read/write operation. No shared reader locks.
- Removal validates all manifests, safe present entries, and collisions, but allows missing payload in ANY package. Missing manifests remain errors. Other operations remain strict. Preserve unlisted files and retain the record after incomplete removal where possible; no rollback promise.
- Preserve builtins. Discover installed commands from records and execute their recorded paths with fork/exec, never PATH. Include them in help and `--commands-json`.
- Updates are remove-then-install. Failed replacement can leave a command package unavailable. Package management does not update AiShell itself.
- Keep command-package and shell release versions separate. Both initial versions are `0.1.0`. Use PATCH for compatible fixes, MINOR for compatible features and pre-1.0 incompatible changes, and MAJOR for incompatible changes after 1.0. Ordinary edits do not increase versions.
- Use brief Simplified Technical English. Do not claim certified or checked compliance.

## Delivery stages and evidence

The parent accepted the implemented milestone and extra release after exact-tree
verification and independent review. See [final evidence](packaging-verification.md).
Global formatting failures in unchanged files and coverage limits remain explicit.
Final documentation commit and branch readback are publication gates, not hidden
feature requirements. The user merges the branch.

- [x] 1. Review the contract and named acceptance matrix independently. Resolve findings within the authorized scope; record material changes.
- [x] 2. Implement `hello` and shared metadata access with TDD. Prove agreement between parser, help, metadata, and generated docs, including stable logical limits and unchanged builtin behavior.
  Hello, shared export and generated docs are implemented. See
  [hello evidence](hello-packaging.md) and [build evidence](package-build.md).
  Parent exact-tree acceptance and independent review passed.
- [x] 3. Generate the command package through Make. Check inventory, manifest agreement, repeatable bytes, and failure cleanup.
- [x] 4. Implement `pkg list`, `info`, `install`, and `remove`. Check grammar, invalid state, collisions, exclusive locking, path safety, copy failure, cleanup, and two partial-package removal in either order.
- [x] 5. Connect installed discovery, help, execution, and catalog export. Test real one-shot main and a persistent loop, child statuses, refresh, and builtin preservation.
- [x] 6. Verify build → install → list → info → help → execute → catalog → remove. Prove discovery disappears and remove-then-install works. Run applicable regression, build, lint, formatting, and safety checks; record failures and omissions.
- [x] 7. Deliver milestone docs, demo, decision log, and exact reproducible verification steps. Independent review passed. Publication readback follows the final scoped commit.
- [x] 8. Build and independently verify the separate full-shell release. Runtime and release builders are in the verified branch commit. The user merges later.

## Historical coordination checkpoint

- Item 3 is blocked on builder data-loss publication repair and new failure
  tests. Independent review must run again after repair.
- Item 4 has 39 passing tests reported by its owner; review is pending.
- Item 5 is assigned to integration. Verification is pending.
- The 15 existing positional builtin specifications now set the optional
  metadata callback to `NULL`. Strict standalone object compilation and
  scoped format, analyzer, line-length, and whitespace checks passed. The
  [decision log](../../ai_log/2026-10-22-packaging.md) gives flags and evidence.
  This compatibility check does not close shared metadata or integration work.
- No delivery stage is complete at this checkpoint. The parent owns final
  verification and any branch push. The user will merge later.

## Implementation evidence and earlier publication checkpoint

- Registry: 19 builtins, including `pkg`; 17 builtin wrappers. The 18-command
  baseline includes adopted `rmdir`. Retired commands remain absent. `hello`
  is a separate package-only executable, not a builtin.
- Local install/list/info/remove and real installed help, catalog and recorded-
  path fork/exec are implemented. Integration refreshes state in a persistent
  loop. See [backend](pkg-implementation.md) and [shell](package-shell.md).
- Review fixes cover immutable package output, source/root overlap, the
  60-byte command-name bound, independent docs-path sorting and test sensitivity.
  Backend follow-up reports 44 normal and 44 sanitizer tests and six mutants.
  Shell tests now isolate HOME. Make now builds the openat fault driver;
  its owner reports full strict `make test all` and `make lint` passing.
- Release provenance review found that a separate generator could invent help
  and catalog for another binary. Repair was assigned. The release owner now
  reports exact-byte snapshot generation, removal of `--generator`, a passing
  regression and an independent current-artifact rerun. See
  [release report](shell-release.md). Parent final acceptance is still open.
- [Lifecycle demo](../../demo/PACKAGING.md): 17 actual commands and 21 checks,
  including the relocated release. Generated artifacts are not Git deliverables.
- Parent verified and pushed runtime/build commit
  `6f579356abd67ceb1f7123db53f55896c247b967` to `feat/aishell-packaging`;
  parent reports a matching remote hash. Local Git confirms tree
  `9b457a89b41217ad1dd8572dad968bf6734ca555` and 92 source/build/test paths.
  This includes the retired-command and adopted-rmdir baseline closure.
- Parent exact-tree checks returned 0 for strict `make test all`, release
  tests/build, lint and changed-C formatting. Global formatting returned 2
  on unchanged files. Evidence: `$TMPDIR/packaging-code-index-checks`.
- Docs/demo/bootstrap remain uncommitted. The bootstrap helper now exists but
  its final execution checks remain open. Parent owns their exact-index audit,
  publication and remaining acceptance dispositions. No merge is authorized.

The preceding bullets preserve the earlier checkpoint. Independent bootstrap,
documentation, and demo review subsequently passed. Fresh checksum-pinned
dependencies built successfully and strict full tests passed in an isolated
snapshot. The parent accepted the documented limits; no global formatting
pass, exhaustive fault matrix, or certified language compliance is claimed.

## Required documentation deliverables

Write two separate documents in ASD-STE100 English. Use short, direct
sentences. Do not claim certified compliance without a separate check.

1. `docs/commands/pkg.md`: a brief package usage guide and the main design
   ideas. Show verified commands, limits, and error recovery.
2. Private, in-depth wiki article: `_drafts/aishell-packaging.md` in the wiki
   database. Parent reports DRAFT revision 1 delivered from the reviewed body
   and metadata; exact readback passed. Index/log are revision 2;
   `WikiStore.validate` returned no errors or warnings. Human review is pending.

Base both documents on the implementation and recorded execution results.
Label planned work as planned. Do not present an unimplemented design as a
working feature. The brief [package guide](../commands/pkg.md) exists. This
reconciliation writes no wiki data. Keep the wiki private and excluded from Git
and branch push. Draft delivery does not finalize the milestone.

## Extra stage: full-shell release

Use release identity `aishell`, version `0.1.0`. Generated directory: `build/releases/aishell-0.1.0/`. This is a distribution artifact, not a command package for `pkg install`.

Bundle the built shell, its existing builtins, generated command docs/catalog, version information, and brief release install/update instructions. Include a required runtime library only if needed and permitted; do not introduce a new dependency. Builtins remain part of the shell. Do not restore deleted commands or bundle source-tree caches, logs, credentials, or local installed packages.

Provide separate instructions for shell release installation/update and for command-package installation/update/removal. Use a versioned user-owned release directory and an explicit launcher path; no root install or system PATH edits by default. A manual shell update installs the new directory and changes the user's launcher choice. Preserve the old release and `~/.mysh/pkgs`. Do not claim `pkg` updates the shell.

Verify the clean artifact after copying it to a different path with spaces, outside source/build directories, under an isolated HOME. Run the shell, representative builtins, help/catalog, and the `hello` package lifecycle there. Check runtime-library resolution, generated docs, artifact inventory, and absence of source/build-path dependencies. Record commands, statuses, runtime platform constraints, and omissions. An artifact is not relocatable merely because its build passed.

## Earlier work and evidence checkpoint

Use small RED → GREEN → refactor steps. Keep implementation and independent review separate. The parent handles final checks and commits. The user approved temporary `subagent_auto_approve: true` for this autonomous task; the parent must restore the prior setting at task end. This exception does not expand file ownership or authorize a merge.

The initial checkpoint recorded hello help/usage/metadata RED. The local hello candidate now passed parent verification and independent review; follow-up CLI/module, sanitizer, scoped format and lint checks also passed. Evidence paths and the characterization versus new guard-regression RED distinction are in the [hello report](hello-packaging.md) and [decision log](../../ai_log/2026-10-22-packaging.md). Item 2 stays open for shared export and generated-doc integration. No follow-up stage or commit is claimed. Historical status documents do not prove live Git state. This paragraph records
the earlier hello checkpoint; see current working-tree evidence above.
