# Package shell integration

## Result

Item 5 is implemented in the current working tree. Independent review and
parent delivery gates are separate. No stage, commit, or push was done.
The current removed commands remain removed. `hello` is not a builtin.

- `pkg` uses its existing command specification in the static registry.
- Shell listing and installed help use a fresh, owned package snapshot.
  Installed help renders validated metadata. It does not run the payload.
- `main --commands-json` has an explicit route before legacy option parsing.
  It rejects extra operands. The minimal catalog includes builtins, controls,
  and installed commands in ASCII name order. Strings are JSON-escaped.
- Installed commands use `fork`, recorded absolute-path `execv`, and `waitpid`.
  They inherit standard streams. No PATH search, `system`, pipeline, or job
  handling was added. Child exit status is preserved; signals use 128+signal;
  launch failure is 126; an unknown name is 127. State/system failure is 1.
- Snapshot loading releases its descriptors and lock before fork. Parent-owned
  snapshot bytes are freed before waiting. Each installed lookup reloads state.
- Known builtin dispatch and builtin help avoid package state. They still work
  when installed records are corrupt or the package lock is busy.
- Make links the actual package source/header closure into the shell. It adds
  `test-package-shell`, `test-pkg`, and test artifact cleanup. Both tests are
  prerequisites of `test`. Lint now includes `-Isrc` for module-test headers.

## Changed paths

`src/main.c`, `src/shell.c`, `src/shell.h`, `src/registry.c`, `Makefile`,
`tests/test_shell.py`, `tests/test_shell_module.c`,
`tests/test_package_shell.py`, `tests/test_package_shell_module.c`, and this
report. The required session lookup also appended the verified subagent ID
`20261023_004738_6af94f` and title to `docs/sessions.txt`.
No backend, metadata, hello, or existing source-command file was edited by this
worker. The shell probe initializer now supplies explicit NULL metadata; no
warning suppression was added.

## TDD evidence

Baseline `make test-shell` passed before integration: 33 CLI tests and the
registry/parser module harness. Evidence is `baseline.log`.
Each feature below was implemented only after its stated observed failure.

| Slice | Observed RED | GREEN |
| --- | --- | --- |
| `pkg` builtin | Real main returned 127 for `pkg list`. | Empty list, builtin help/listing, and no builtin hello pass. |
| Installed help | After real install, `help hello` returned 127. | Stored help matches the hello help fixture; listing sees hello. |
| Catalog | `--commands-json` reached legacy parsing and returned 2. | Schema, fields, origins, sorting, deterministic bytes, extra operands, install/remove refresh pass. |
| Installed execution | One live install/help/execute/remove loop omitted the greeting and diagnosed hello as unknown. | The same loop now prints the greeting and returns 127 after removal. |
| Catalog help route | Shell help omitted `main --commands-json`. | Help now advertises the route; exact builtin help tests pass. |

The missing `test-package-shell` Make target was also observed before adding
it. This was build integration evidence, not a behavioral RED.
Additional fault/boundary tests characterize the implemented path; their first
passes are not claimed as pre-fix REDs.

## Verification

Actual commands and results:

- `make test all`: status 0, including current regression suites, 33 shell CLI
  tests, the shell module harness, 39 backend CLI tests, and 13 package-shell
  tests. The package-shell test includes six C module modes.
- `make test-package-shell`: status 0. Tests cover same-loop and external-process
  refresh; metadata-only help/catalog; recorded-path lookup after `cd`; poisoned
  PATH; child exits, signals, and exec failure; inherited input; unchanged input
  offsets for discovery and `pkg`; `/dev/full`; corrupt state; lock contention;
  and a child package read while its parent waits.
- The C module harness uses the real shell and backend. Narrow linker wrappers
  test fork failure, EINTR wait retry, an error reported after actual reaping,
  exec failure, and catalog-owned allocation failure. It checks borrowed argv,
  getopt state, cwd and descriptors, including 20 repeated execute/help/catalog
  cycles. Descriptor checks cover the fixture's descriptor range below 256.
- Private full-shell and module builds with `-Werror`, ASan and UBSan passed all
  13 package-shell tests. `-no-pie` is used for these instrumented test builds.
- Owned C/header `clang-format --dry-run --Werror`: status 0.
- Owned-source `clang-tidy` analyzer checks with warnings as errors: status 0.
- A private stale-snapshot mutant failed the external-update test as intended:
  after removal it returned 126/launch failure instead of 127/unknown. The
  unchanged production implementation passed afterward.

## Open global checks

`make lint` first failed because several new module tests could not find source
headers. The Make include-path fix removed those errors. The remaining failure
is `tests/pkg_faults.c:124`, `clang-analyzer-valist.Uninitialized`. That test file
belongs to the backend owner. No global analyzer check was disabled.

`make format-check` fails on existing files outside this slice:
`src/traverse.c`, `src/traverse.h`, `tests/test_help.c`,
`tests/test_recursive.c`, and `tests/test_output.h`. This worker did not reformat
those files. The owned-file format check passes.

No new shell fuzz framework was added. Current shell quote/operator/generated
filename tests and backend bounded JSON tests run through `make test`.
Backend review, global lint disposition, global formatting disposition, exact
index verification, and publication remain parent gates.

## Raw evidence

Private evidence directory:
`/home/name/.hermes/profiles/embx420/cache/scratch/package-shell-evidence/`.
It contains `baseline.log`, full streams in `execution.log`, command/status/source
hash entries in `ledger.jsonl`, the runner, private build variants, and a final
external manifest. Reports do not embed their own hashes. Evidence is scratch
output, not a tracked log or a committed delivery tree.

## Independent-review test fixture correction

`tests/test_shell.py` now creates a separate empty HOME per test, outside its
working directory, and passes the same explicit HOME/`LC_ALL=C` environment to
all ten CLI and PTY launch sites. Cleanup is registered immediately. A new
regression exercises every launch site while the inherited HOME is poisoned,
checks the private HOME and locale, and checks that no package store appears.

Observed RED: running the original 33 tests with a real hello installed in a
scratch caller HOME failed three exact-help assertions (`shell-red.log`). The
new launch-environment regression also failed before the fixture correction
(`shell-regression-red.log`). Observed GREEN: all 34 tests pass with that same
installed caller HOME (`shell-final-green.log`). The runner independently
executes its installed hello first; exact before/after bytes and full stat
values, including atime, match for all 12 HOME entries. Its snapshot uses
`O_NOATIME` so the proof itself does not change the caller fixture.

Commands from the repository root:

```sh
python3 "$TMPDIR/test-review-fixes/run.py" shell-final-green
make test-shell test-package test-hello test-package-shell
python3 "$TMPDIR/test-review-fixes/checks.py"
```

These pass: 34 shell tests plus its C harness, 18 builder tests, 12 hello tests
plus metadata/module checks, and 13 package-shell tests. Scoped Python syntax,
all ten explicit subprocess environments, and scoped whitespace checks pass.
Full `make test` was attempted but stopped in an unrelated backend regression:
`test_openat_variadic_forwarding` cannot find `build/pkg-tests/openat`; the
current `test-pkg` prerequisites do not build it. No Makefile/backend change or
manual hidden prerequisite build was made by this worker.

Evidence and reproducible runners are in
`/home/name/.hermes/profiles/embx420/cache/scratch/test-review-fixes/`, including
`focused-final-green.log`, `full-test-green.log` (failed despite its initial
filename), `scoped-checks.log`, and incremental test diffs. This correction
changes only the two owned test files and their reports; no production code,
staging, commit, push, or historical AI log changes. Session lookup verified
`20261023_012822_118e44`, titled `Subagent: Fix two independent-review test defects with…`.
Registration is left to the parent because session-file edits are outside this
worker's explicit four-file ownership.
