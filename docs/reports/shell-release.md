# Full-shell directory release

## Result and scope

`make release-shell` builds `build/releases/aishell-0.1.0/`. It is a complete
shell distribution, **not** an installable `pkg` payload or shell self-update.
`packages/aishell/release.json` is its only identity/version source. The copied
record is byte-identical. Builds never increase versions.

The artifact contains `bin/aishell` (the actual built main), generated builtin
pages under `docs/`, the clean-HOME runtime catalog at `metadata/commands.json`,
`README.md`, `SYSTEM.md`, and `examples/hello-0.1.0/`. The latter is a prebuilt,
optional demonstration package, not a builtin. The verified catalog has 19
builtins and two shell controls; the artifact has 28 regular files.

Only the new release builder, release record, release tests, this report,
`docs/commands/pkg.md`, and two narrow Make targets were added. No runtime,
hello/metadata, existing package-builder, wiki, AI log, staging, commit, or push
operation belongs to this slice. The active backend review remains a separate
parent delivery gate.

## Generation and safe publication

The Python builder captures the selected `--binary` bytes once, writes a private
executable snapshot of those exact bytes, and uses that snapshot for the catalog,
all builtin help and shell ELF requirements. Those same captured bytes become
`bin/aishell`; later source-path changes cannot select a different generator.
There is no public `--generator` override: argparse rejects it before publication.
The snapshot runs with a fresh private HOME and a PATH with no external tools.
`--commands-json` supplies registry summaries and the
inventory, including controls. `help BUILTIN` invokes existing usage callbacks;
its complete display text goes into a Markdown code block. It is not parsed for
arguments. No C parser, copied builtin inventory, or fabricated structured
argument schema is used. Controls remain in the catalog but lack builtin pages.
The real hello package retains its separately exported structured metadata.

The builder reuses the fixed package builder's bounded reads, duplicate-key JSON
checks, manifest checks, safe-path/link checks and exact demo source inventory.
It rejects source/output ancestry overlap in either direction before generation
or output writes. Binary/demo inputs cannot overlap output either.
Release source contains only its identity record. Complete payload generation
precedes private same-output-root staging.

An existing target is never moved, replaced, or deleted. Only exact safe
inventory, bytes, types, link counts and modes permit a no-op repeat. All other
existing targets fail, including unlisted human files and empty directories.
New publication uses Linux `renameat2(RENAME_NOREPLACE)`, so even an empty target
created during publication cannot be replaced. Ordinary failure cleans only the
new stage. There is no backup/restore deletion path. A process crash can leave a
private stage; hostile concurrent writers and crash durability are not promised.
Use a new version/output root for changed content; never remove unknown output.

## Runtime portability and operator guide

Both actual release executables passed `ldd`: only `libc.so.6` and the system
loader are dynamic dependencies; no project build-library path is required.
`readelf` gives x86-64, `/lib64/ld-linux-x86-64.so.2`, and a maximum required
symbol version of `GLIBC_2.34` for shell and hello. Thus the distribution requires
compatible Linux/x86-64, that loader and glibc 2.34 or later symbol support.
Package mutation also needs kernel/filesystem `renameat2` no-replace support and
POSIX record locks. `SYSTEM.md` records the exact inspected requirements. This
is not fully static, not cross-platform, and not an oldest-OS compatibility test.
The builder rejects RPATH/RUNPATH and path-bearing NEEDED dependencies.

The generated README explains user-owned whole-directory installation,
one-shot and loop usage, command help/catalog, the optional demo lifecycle,
manual command-package remove/install updates, and separate versioned-directory
shell updates with a retained rollback copy. No sudo, downloads, automatic
installation, or self-update is implied. Existing help can still say `main`;
the artifact executable is `bin/aishell`. Existing debug information can contain
build paths; it is not a runtime dependency. Source, compiler, Python and the
repository are not required to run the copied directory.

## Actual TDD and verification

Private raw evidence is in
`/home/name/.hermes/profiles/embx420/cache/scratch/shell-release-evidence/`.

- Before implementation, `red.log` shows all initial artifact-contract tests
  fail because the release builder is missing. These are artifact-entrypoint
  REDs, not claims of distinct pre-existing runtime defects.
- `green.log` exposed an incorrect generator assumption: `help exit` is not a
  supported builtin help route. The builder now generates pages only for
  registry builtins and retains controls in the catalog. No runtime was changed.
- `empty-help-red.log` shows the new empty-help test publish successfully when
  it should fail. The explicit nonempty-help check makes it pass.
- Subsequent invariant/fault cases are characterization tests, not invented
  pre-fix RED evidence. The final owned suite passes 12 tests, including subcases.
- The provenance regression first failed with `0 != 2`: an unrelated executable
  supplied a one-command `pwd` catalog and invented help while the builder shipped
  the real shell. After removal of `--generator`, this exact test passes with
  argparse status 2 and no output directory. Generator failure and empty-help
  fault injection now patch the internal runner only; the public CLI has no bypass.

The suite checks exact file inventory and modes, binary equality, unchanged
source record, all builtin summary/help agreement, version-driven directory and
README generation, no source-root paths in text output, exact repeat inode
preservation, changed-binary refusal, unlisted-content preservation, and rejection
of wrong bytes/modes, symlinks, hardlinks, FIFOs and extra directories. It tests
both source ancestry directions, missing binary, failing generator, empty help,
injected staging write failure and injected publication failure, with no visible
partial artifact or leaked ordinary stage.

The relocated test removes its original generated directory, uses fresh HOME
and poisoned PATH, checks every builtin help page/catalog and real pwd/loop
execution, then installs/executes/removes bundled hello. A clearly labeled
synthetic next-version record reuses the real hello binary to exercise manual
remove/install update. It switches to a second copied shell directory, deletes
the old copy, verifies installed hello persists and runs, then removes it.
This tests launcher/copy selection, not an invented second shell implementation.
A separate verification runner copies the actual published artifact outside the
repository and repeats pwd/catalog and the real hello help/execute/remove cycle.
After the provenance fix, the independent review probe was rerun against the
current repository (not its stale snapshot). It verified the relocated published
artifact's catalog, every builtin help page, pwd/loop and hello lifecycle; its
malicious generator was rejected with status 2 before any output directory existed.
An exact default repeat preserved the published inventory without replacement.

Verified commands:

```sh
make test-shell-release
make release-shell
python3 -m ruff check scripts/build_shell_release.py tests/test_shell_release.py
python3 -m ruff format --check scripts/build_shell_release.py tests/test_shell_release.py
python3 -m py_compile scripts/build_shell_release.py tests/test_shell_release.py
ldd build/releases/aishell-0.1.0/bin/aishell
ldd build/releases/aishell-0.1.0/examples/hello-0.1.0/bin/hello
```

Ruff was absent, as was `uvx`; pip installed Ruff only into the private evidence
`tools/` directory. Its path is supplied through PYTHONPATH in `verify.py`.
Scoped Ruff lint/format, Python compilation and Git whitespace checks pass.
`verification.log` holds full final command streams; `ledger.json` records actual
statuses and counts; `manifest.json` records candidate/artifact SHA-256 hashes.
Final project tests/builds in this slice use a private TMPDIR below that evidence
root. Initial real-main smoke passed pwd, pkg list, help pkg and catalog with
clean HOME and unavailable PATH before release implementation.

## Remaining parent gates

No owned release check is blocked. The repository ignores `scripts/`, so the
parent must explicitly include `scripts/build_shell_release.py` (and its existing
`build_package.py` dependency) when staging the complete milestone; merely adding
visible untracked files is insufficient. No ignore rule was changed.
Backend files changed during early checks;
one intermediate compile reported an unused helper while that work was in
progress. The later build and release checks passed. This is not independent
backend approval, a full global regression result, or exact-index verification.
If backend review changes runtime bytes again, the immutable release must fail
rather than overwrite itself: preserve/move the known old artifact or use another
output root, then rerun release verification against the final approved binary.
No sanitizer rerun is claimed here (no C/runtime edits), nor exhaustive fuzz,
hostile-writer, kill/crash, or old-distribution testing.

Session lookup verified `20261023_011315_e782b1`, titled
`Subagent: Implement extra full-shell release package…`. Session registration is
left to the parent because this worker's ownership excludes `docs/sessions.txt`.
