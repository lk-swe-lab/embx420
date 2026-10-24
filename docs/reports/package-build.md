# Shared metadata and hello package build

## Result and boundaries

`make package-hello` builds `build/packages/hello-0.1.0/`. Its source record is
`packages/hello/pkg.json`. The build copies this record without changing bytes.
It generates `metadata/commands.json` and `docs/hello.md`, and copies the built
`bin/hello`. It does not install a package or change the builtin registry.
The command is package-only. No new dependency is required.

Source files, generated output, and installed copies are separate. Do not edit
generated files. Change the command definitions or source manifest, run the
tests, then build in a new output root or move the old target manually.
The builder never replaces an existing target. The version comes only from the source manifest.
Ordinary builds do not increase it. Package install/update/remove belongs to
the separate package-manager stage; this build does not update the shell.

## Shared interface

`cmd_spec_t` now has this optional member:

```c
int (*visit_metadata)(cmd_metadata_visitor visit, void *context);
```

The visitor receives a terminated Argtable (`void **table`) and its context.
The provider owns all entries and strings. They live only during the callback.
The visitor must not change or retain them. The provider must supply logical
counts, not argc-sized storage limits. `INT_MAX` means unbounded repetition.
A NULL callback, constructor failure, or nonzero visitor result returns -1.
The hello provider frees its table on every path.

`cmd_metadata_write(FILE *, const cmd_spec_t *)` writes one command JSON object.
It returns 0 after flush, or -1 on missing metadata, allocation, stream, or close
failure. Failed output can contain partial bytes. Callers must stage output.
It filters remarks and the terminator, splits aliases in definition order,
escapes JSON, and derives syntax directly with `arg_print_syntax`. It does not
parse terminal help or C source. Hello parsing, help, the retained local visitor,
and shared metadata all use `arguments_build`. Help uses the specification name.
Defaults and semantic validation remain in public help; this is not an LLM
input schema.

Existing static partial builtin initializers have a NULL optional member under
C aggregate initialization rules. They were not edited because they are outside
this worker's scope. GCC reports `-Wmissing-field-initializers` for positional
initializers. Normal `make all` and `make test` pass, but a global `-Werror` build
needs parent-owned explicit NULL initializer maintenance. No warning suppression
was added. New/owned C paths pass strict `-Werror` builds.

## Build validation and publication

`scripts/build_package.py` uses Python's standard library. It rejects malformed,
duplicate-key, over-limit, invalid-Unicode, or unsupported source manifests.
It validates the exported command schema and its exact command-name agreement.
The initial exporter supplies one hello command, not a general command compiler.

The manifest defines the payload inventory. Explicit support files come from
paths relative to its directory. Generated paths come from the binary/exporter.
Unlisted source entries, missing support files, unsafe paths, symlinks,
hardlinks, special files, nonexecutable binaries, and size/count excesses fail.
Only necessary parent directories are created. Source input is bounded and
observed mutation is rejected. Before any output write or exporter execution,
source/output ancestry overlap is rejected in both directions. Binary and
exporter inputs inside the output root are rejected too.
The build stages complete output in the output root and renames it to an absent
target. An existing target is never moved or deleted. It is accepted only if its
record, exact inventory, safe types, link counts, bytes and modes match the staged
artifact. This is a no-op, including unchanged directory and file inodes.
Any difference or unlisted human content fails. Choose another `--output-root`
or move the old target manually to build changed inputs at the same version.
There is no backup or restore path. Thus cleanup cannot delete an old backup,
and no backup recovery location is needed. Only this build's new stage is cleaned.

This is local build automation, not the installer's security boundary. Source
and output directories must not have concurrent hostile writers. The path
checks do not establish dirfd-based race resistance. Exporter output is written
to a temporary file, read with a byte bound, and run under a timeout; it is the
trusted built exporter, not an untrusted package binary. A crash can leave a
private stage. No concurrent-build lock, hostile-writer race resistance, crash
recovery, or install/update atomicity is claimed.

## Actual TDD evidence

Scratch evidence is at:
`/home/name/.hermes/profiles/embx420/cache/scratch/package-build-evidence/`.
Each named RED below ran before its production change. Later broad boundary and
fault tests are characterization, not invented RED claims.

| Slice | Observed RED | Observed GREEN |
| --- | --- | --- |
| Shared metadata | `metadata-red.log`: absent shared header/API fails compilation. | `metadata-green.log`: strict C test succeeds. |
| Complete package | `package-red.log`: missing `package-hello` target fails the end-to-end assertion. | `package-green.log`: exact JSON, inventory, executable and docs/help assertions pass. |
| Manifest validation | `manifest-red.log`: boolean schema accepted with status 0 instead of 1; other invalid manifests also expose missing validation. | `manifest-green.log`: both accumulated tests pass. |
| Source inventory | `inventory-red.log`: unlisted file and linked source path each publish instead of failing. Existing missing-file preservation passes as characterization. | `inventory-green.log`: accumulated five tests pass. |
| Export validation | `export-validation-red.log`: unknown metadata field publishes instead of failing. | `export-validation-green.log`: accumulated six tests pass. |
| Usage contract | `usage-newline-red.log`: terminal LF in usage publishes instead of failing. | `usage-newline-green.log`: all nine package tests pass. |
| Stable modes | `modes-red.log`: copied manifest has mode 0664, not the selected 0644. | `modes-green.log`: all nine tests pass; directories/binaries use 0755, other files use 0644. |

The original package suite had nine tests with multiple independent subcases. It
checks fixed JSON fields with a separate Python oracle, help/docs agreement,
canonical option order, escaped text, aliases, remark filtering, and logical
unbounded counts. It checks repeatable payload bytes/modes, manifest identity,
explicit support bytes, missing/unlisted entries, unsafe types and source paths,
invalid metadata, preserved prior output, and no leaked stage after failures.
Inclusive byte/path/file limits include 65536-byte manifests, 255-byte paths,
64 files, 16777216-byte files, and 67108864 total payload bytes, with excess
cases. A real `RLIMIT_FSIZE` failure tests failed payload writes without
publication. The C test injects constructor, memstream allocation, and close
failures, repeats error/success calls, and uses `/dev/full` for flush failure.

## Verification

From the repository root:

```sh
make all
make test-hello test-package
make test
python3 "$TMPDIR/package-build-evidence/verify.py"
```

The final runner passed 31 commands. It built all four new executable paths
with strict C17 warnings and `-Werror`, then repeated them with ASan/UBSan.
Each build ran 12 hello CLI tests, all five existing hello module modes,
metadata failure tests, a synthetic metadata fixture, and the exporter.
Scoped clang-format and clang-analyzer checks passed. `make all`, the focused
suites, full current `make test`, and scoped whitespace checks passed.
The full regression exercises existing command modules and real shell tests
after the optional member change. Its positional-initializer warnings remain
as stated above. Sanitizers cover project code, not proof of an instrumented
Argtable dependency.

Exact commands, streams and statuses are in `final-execution.log` and
`final-evidence.json`. `freeze.py` also passed a forced `make -B package-hello`
rebuild with identical payload bytes and modes; see `forced-repeatability.log`.
The exact 16 candidate-file and four artifact-file hashes are in
`candidate-manifest.json`. The parent must independently review this candidate.
No stage, commit, push, branch change, plan/log/registry/shell edit, or other
command source edit was made by this worker. Session lookup verified
`20261022_232954_8e3654`, titled `Subagent: Implement remaining item2/shared
metadata and…`; registration was left to the parent because session-file edits
are outside this worker's explicit ownership.

## Remaining gates

- Parent independent review and exact candidate/index checks.
- Explicit NULL maintenance in other positional builtin/test initializers if
  required for global warnings-as-errors; current behavior is safe and tested.
- Installer, installed discovery/dispatch, lifecycle and full-shell release.
- No exhaustive JSON fuzzing, all combinatorial limits, hostile-writer race,
  simultaneous builds, or crash test is claimed here. Build outputs contain
  local ELF binaries, not a portable ABI.

## Publication review fixes

Evidence directory:
`/home/name/.hermes/profiles/embx420/cache/scratch/build-publication-fix/`.
Logs contain actual commands, output and exit status.

Each row below is a small RED then GREEN change, not a reconstructed result.

| Slice | Observed RED | Observed GREEN |
| --- | --- | --- |
| Ancestry overlap | `overlap-red.log`: all three initial layouts publish, including a source at `OUTPUT/hello-0.1.0/source/pkg.json`. | `overlap-green.log`: all reject before writes. The final suite also checks a source equal to the target. |
| Immutable target | `immutable-red.log`: an arbitrary target loses `human-notes` and returns 0. | `immutable-green.log`: notes survive, identical builds keep the inode, changed bytes fail without replacement. |
| Independent docs order | `prefix-red.log`: valid docs order for `a` and `a-a` raises `required generated docs mismatch`. | `prefix-green.log`: sorted paths pass; unsorted paths fail. |
| Command name bound | `name-red.log`: 61-byte metadata command name is accepted. | `name-green.log`: 60 bytes pass through a real build, 61/63/64 fail; package names and long aliases still allow 63 bytes. |

The chosen command bound is 60 bytes. This leaves space for `.md` in a 63-byte
component without truncation. The parent must apply and test the same bound in
the C package backend. This worker did not edit backend files.
Direct command JSON may be compact. Packaged metadata keeps deterministic
two-space JSON and a final LF. The C renderer was not changed.

The paired publication/restore fault test arms the exact reviewed `payload`
and `previous` rename failures. For an identical target, neither rename is
called and all old bytes/inodes remain. A publication failure on an absent
target cleans only the new stage. `paired-negative-control.log` runs this new
regression against the saved reviewed builder and observes both rename errors.
This is an additional negative control, not a pre-change TDD claim.
Further checks reject extra files/directories, missing files, changed records,
wrong modes, symlinks, hardlinks and FIFOs without deleting the target.

`make test-package test-hello` passes: 16 builder tests, 12 hello CLI tests,
the metadata C tests and all five hello module modes. See
`final-focused-green.log`. The same targets pass in a separate scratch build
with `-Werror`, ASan/UBSan and leak checks; see `final-sanitized-green.log`
and `sanitized.py` for the exact compiler flags. The existing Argtable library
was reused, not rebuilt with sanitizers. The repository output was not replaced
by sanitized binaries. No broad backend or shell verification is claimed here.

Only `scripts/build_package.py`, `tests/test_package_build.py`, this report,
and `packaging-contract.md` were edited in the repository. No stage, commit or
push was made. Session lookup verified `20261023_004738_addf02`, titled
`Subagent: Fix item2/3 build publication blockers with…`. Session registration
is left to the parent because this worker owns only the four files above.

## Independent-review preservation-oracle correction

`tests/test_package_build.py` no longer treats access time as evidence of a
builder write. The preservation oracle compares device, inode, mode, link
count, size, nanosecond mtime/ctime, regular-file bytes and symlink targets.
It never reads FIFO contents. Each unsafe-inventory subcase owns a separate
output fixture; context-manager `finally` cleanup and output restoration run
even when an assertion fails, before the next subcase starts.

Observed RED: the unmodified builder test happened to pass on the current
relatime state (`builder-red.log`). Aging only the test's changed record to
atime/mtime 1 ns reproduced a read-induced full-stat mismatch, followed by four
cascading subcase failures (`builder-aged-red.log`). The separate atime
regression failed against the original full-stat oracle
(`atime-oracle-red.log`). No production builder change was needed.

Observed GREEN: the original 16 builder tests plus two regressions pass
(`builder-green.log`). One regression proves a real read advances atime while
the preservation oracle stays equal; a separate stdlib-only probe confirms
all seven stable fields are unchanged (`independent-atime.json`). The other
injects an assertion failure in the first subcase, then verifies exactly that
one failure, all eight subcases attempted, eight independent output roots and
complete cleanup. A private in-memory shared-output/no-finally mutant fails
that regression (`cleanup-negative.log`); this is a post-fix negative control,
not a claimed pre-change RED. The unchanged corrected tests pass afterward
(`regressions-final-green.log`).

Commands from the repository root:

```sh
python3 tests/test_package_build.py -v
make test-shell test-package test-hello test-package-shell
python3 "$TMPDIR/test-review-fixes/checks.py"
```

All pass: 18 builder, 34 shell, 12 hello and 13 package-shell tests, plus the
associated C checks. Python syntax, explicit HOME environments at all shell
launch sites, and scoped whitespace pass. Full `make test` was attempted but
stopped outside this correction: the backend's new
`test_openat_variadic_forwarding` needs `build/pkg-tests/openat`, absent from
the current `test-pkg` prerequisites. `full-test-green.log` records this failed
attempt; its initial filename is not a success claim.

Raw evidence and runners:
`/home/name/.hermes/profiles/embx420/cache/scratch/test-review-fixes/`.
Only the two owned tests and their two reports were edited. No production,
Makefile, session registry, historical AI log, stage, commit or push changes
were made for this correction. Verified session: `20261023_012822_118e44`,
`Subagent: Fix two independent-review test defects with…`; registration remains
a parent action because of the explicit four-file ownership.
