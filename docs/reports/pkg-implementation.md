# Package manager implementation candidate

## Status

The isolated C package manager passes 44 test methods in normal and
ASan/UBSan builds. Six safety mutants fail their intended assertions.
Scoped formatting, analyzer checks and `make lint` pass. This is an
implementation candidate, not acceptance of the full packaging milestone.

No existing integration file was changed. No Git stage, commit or push was
made. Main-loop discovery, execution, catalog export and full regression
remain for the parent integration owner and independent review.

## Files and interfaces

- `src/cmd_pkg.[ch]`: `cmd_pkg_spec`, exact CLI alternatives and streams.
  An exact selector rejects invalid forms before an invocation-owned Argtable
  parses a private canonical vector. The selector also protects the dependency
  from malformed option clusters. Help is fixed text; metadata adoption of
  this builtin is not part of this slice.
- `src/pkg_store.[ch]`: owned records, snapshots, installation and removal.
- `src/pkg_fs.[ch]`: component-wise no-follow access, bounded regular-file I/O,
  permission normalization and the persistent exclusive fcntl lock.
- `src/pkg_json.[ch]`: bounded strict JSON trees. There is no JSON dependency
  or Python package-operation runtime.
- `src/pkg_schema.[ch]`: manifest and full command-metadata validation.
- `tests/test_pkg.py`, `tests/test_pkg_module.c`, `tests/pkg_driver.c`,
  `tests/pkg_json_driver.c`, `tests/pkg_faults.c`, `tests/pkg_build.py`,
  `tests/pkg_negative.py` and `tests/pkg_openat_driver.c`: isolated builds,
  tests and fault controls.

The integration API is in `pkg_store.h`:

```c
int pkg_snapshot_load(pkg_snapshot *out, FILE *errors);
void pkg_snapshot_free(pkg_snapshot *snapshot);
int pkg_store_install(const char *source, FILE *out, FILE *errors);
int pkg_store_remove(const char *name, FILE *out, FILE *errors);
```

Initialize the real builtin registry before calling these functions. Collision
checks call its actual `find_command`; the isolated harness reserves `cat` as
one representative builtin. `help`, `exit` and `pkg` are always reserved.
The package backend does not register external specifications in static slots.

A successful snapshot owns all storage and holds no descriptor or lock.
Packages and commands are in ASCII name order. Each `pkg_command` provides
name, summary, long help, usage, package, version, absolute executable path,
and a validated metadata object with ordered argument/option arrays. Package
records also expose the parsed manifest, parsed metadata and unchanged manifest
bytes. Member strings borrow snapshot storage. Free only through
`pkg_snapshot_free`, after all readers finish. Do not overwrite a live snapshot
without freeing it first. Repeated free is safe.

Snapshot failure returns -1 and an empty snapshot. An absent root gives an
empty successful snapshot without directory creation. Mutation helpers return
0 after a completed operation, or -1 on failure. Their caller must check and
flush its output stream. `cmd_pkg_spec.run` does this and returns CLI statuses
0/1/2. Output errors after publication/removal explicitly report the commit.

## Verified behavior

- Local hello install, sorted list, exact info, direct installed-binary execution,
  remove, retry and remove-then-install. Source/build payload bytes stay unchanged.
- Required and support-file inventory; fixed private installed modes, set-id
  stripping and restrictive-umask behavior without changing the process umask.
- Duplicate/unknown/missing JSON fields; strict number grammar; UTF-8, escapes,
  surrogate pairs, NUL, BOM, trailing data and nesting checks. Duplicate decoded
  keys are rejected too. Schema checks include metadata identity, order, aliases,
  counts, public text and generated-file mappings.
- Exact/beyond bounds for JSON inputs, nesting, path length, file count,
  description/summary/help/usage/argument text, argument-entry count, package
  commands, installed commands, installed packages, file bytes and total bytes.
  A 60,001-string JSON array also passes under a 128 MiB address-space limit.
- Builtin/control/package collisions, including installed command collisions
  before removal. Invalid records fail without partial list/info output.
- No-follow source ancestors and leaves, hardlinks, FIFOs, wrong entry types,
  nonexecutable binaries, unlisted entries and unsafe root/lock permissions.
  Unlisted removal content and its record are retained.
- One nonblocking exclusive persistent lock for reads and mutations. Real
  cross-process contention covers a reader holder and a stopped writer holder.
  Killing the writer releases the same lock inode. Reads ignore hidden stages;
  mutations report stale stages and require manual cleanup.
- Source change observation, short/EINTR reads and writes, zero writes,
  open/read/write/close/rename errors, stage cleanup failure, removal unlink
  failure, final-rmdir record restoration and failed restoration.
- Process death immediately before/after publication leaves an absent or complete
  visible package. A final directory that appears after preflight is not replaced.
- Two packages with missing binaries/metadata can be removed in either order.
  Other operations remain strict while either package is partial.
- Help/list/info and post-commit install/remove failures on `/dev/full`.
- Invalid module vectors, repeated calls, exported getopt sentinels, caller argv
  bytes/pointers, cwd, HOME, borrowed descriptors and descriptor counts. A parser
  wrapper deliberately permutes its private vector after real parsing.
- Owned snapshot strings remain valid after a later package removal.
- Seeded JSON differential checks against the independent build reader.
- Negative controls: followed link, ignored copy error, early record deletion,
  and ignored busy-lock error. Each fails a specific assertion, not a crash.

Installation uses Linux `renameat2(RENAME_NOREPLACE)` for atomic no-replace
publication. There is no unsafe fallback. Parent directories are resolved from
file descriptors; payload deletion uses only manifest paths and derived empty
parents. No recursive deletion is used. Stale stages are never traversed.
Power-loss durability and defense against hostile same-user mutation are not
promised. The production test binary has no system/popen/exec/fork/spawn symbol.
The test runner executes hello separately; package operations never execute it.

## TDD evidence and limits

Raw evidence is under `$TMPDIR/pkg-evidence`; sanitizer evidence is under
`$TMPDIR/pkg-sanitize`. These are scratch evidence, not tracked deliverables.

| Logs | Observed RED then GREEN |
| --- | --- |
| `01-*` | Fixed help: status 2 became status 0 with exact bytes. |
| `02-*` | Strict JSON reader: valid `{}` was rejected, then accepted. |
| `03-*` | Empty list and grammar: list status 2 became empty success. |
| `05-*` | Local hello install/list/info: install-not-implemented became success. |
| `06-*` | Complete and paired-partial removal: not-found became success. |
| `07-*` | Bounded JSON memory: valid large array failed under 128 MiB, then passed. |
| `11-*` | Parser constructor-NULL fault: false help success became status 1. |
| `12-*` | Prefix-name docs and negative-zero counts: valid packages rejected, then accepted. |
| `15-*` | Stale-stage failure lacked context, then named manual cleanup. |
| `17-*` | Restrictive umask blocked install, then exact modes and lifecycle passed. |

The original `04-red.log` is a missing-lock `FileNotFoundError`, not a valid
assertion RED. The test now asserts lock existence first. Later real contention
checks and the busy-lock mutant prove sensitivity, but do not repair that
historical TDD gap. Many defense-in-depth, fault and boundary checks were added
as characterization of the defensive install/remove slice. They are not claimed
as separate pre-implementation RED cycles. Strict TDD for every matrix variant
is therefore **not** complete.

Two test/tool setup failures were retained:

- The first JSON valid-fixture literal had an invalid backslash escape. It was
  corrected; the initial `{}` assertion RED remains genuine.
- Fortification changed instrumented reads to `__read_chk`, bypassing a read-only
  fault seam. The seam now wraps both entry points and forwards real bounds
  checking. The original sanitizer failures remain in scratch.

The initial wrapper-first workaround for the Clang-tidy 18 variadic warning
was not a global-lint fix. The follow-up below replaces that workaround with
a justified one-line suppression. Original diagnostic logs remain unchanged.

## Contract/build findings

1. Historical finding, now resolved: the build reader rejected a valid
   multi-command inventory for `a` and `a-a` because sorted docs paths differ
   from command-name order. `scripts/build_package.py` now independently sorts
   generated docs paths. Builder regression
   `test_prefix_command_docs_are_independently_sorted` covers the fix;
   backend `test_multi_command_prefix_docs_order` covers the C reader.
2. Commands now have an explicit 60-byte limit in manifest and metadata
   validation. The `.md` suffix fits the unchanged 63-byte component limit.
   Package names and long option aliases retain their separate 63-byte limit.
   Names and versions are validated exactly; no trimming, case conversion or
   release-version normalization is done.
3. JSON integer `-0` has the nonnegative value zero. The C count validator accepts
   it, consistent with the build reader, and compares arbitrary-length integer
   counts without overflow. Fractional/exponent forms remain invalid counts.

## Reproduce

Run from the repository root. The existing Argtable static library and generated
hello directory must be present. Build/test tools are not package runtime code.
The build helper prints every exact cc invocation and never edits integration.

```sh
python3 tests/pkg_build.py "$TMPDIR/pkg-evidence"
python3 tests/test_pkg.py "$TMPDIR/pkg-evidence/pkg"
python3 tests/pkg_negative.py "$TMPDIR/pkg-negative-final"
python3 tests/pkg_build.py "$TMPDIR/pkg-sanitize" --sanitize
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
python3 tests/test_pkg.py "$TMPDIR/pkg-sanitize/pkg"
```

Final observed results: 44 test methods pass in each build; six mutants are
rejected; C files have no lines over 80 columns; scoped clang-format passes;
scoped clang-tidy and `make lint` pass with warnings as errors. The sanitizer
build instruments owned C sources, not the prebuilt Argtable library.

For an unwrapped production smoke binary with the narrow registry stub:

```sh
cc -std=c17 -Wall -Wextra -Wpedantic -Werror \
  -Wshadow -Wconversion -Wformat=2 -Wstrict-prototypes -g -O1 \
  -Isrc -Ibuild/argtable3/include \
  tests/pkg_driver.c src/cmd_pkg.c src/pkg_store.c src/pkg_fs.c \
  src/pkg_json.c src/pkg_schema.c \
  -Lbuild/argtable3/lib -largtable3 -lm \
  -o "$TMPDIR/pkg-evidence/pkg-plain"
```

That unwrapped binary passed list/install/list/info/remove/list under an isolated
HOME and poisoned PATH. It is not evidence of real main integration.

## Review-blocker follow-up

Evidence root: `$TMPDIR/pkg-fix` (active profile scratch directory).
The original evidence above remains historical. Current reproduce commands:

```sh
python3 tests/pkg_build.py "$TMPDIR/pkg-fix/normal"
python3 tests/test_pkg.py "$TMPDIR/pkg-fix/normal/pkg"
python3 tests/pkg_build.py "$TMPDIR/pkg-fix/sanitize" --sanitize
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
python3 tests/test_pkg.py "$TMPDIR/pkg-fix/sanitize/pkg"
python3 tests/pkg_negative.py "$TMPDIR/pkg-fix/mutants"
make lint
```

### Source/root separation

Install opens the source without following any symbolic link, then observes
physical directory ancestry before root creation, lock creation, or scan.
The comparison uses directory device/inode pairs and descriptor-relative `..`.
It does not compare path prefixes or resolve a link with `realpath`.
For a missing managed suffix, each existing ancestor is checked before the
first missing component. Source equal to or above HOME, `.mysh`, or `pkgs`
is rejected. Source equal to or below an existing `pkgs` is also rejected.
An ordinary source under HOME but outside the managed root remains valid.
Unsafe source/HOME/managed-parent links fail before state mutation.
Concurrent hostile same-user namespace changes remain outside the contract.

- `overlap-red.log`: original source-containing-HOME regression failed its
  fingerprint assertion for all three tested HOME placements.
- `reverse-red.log`: original root/source-equality regression failed because
  the persistent lock changed the source namespace.
- `overlap-green.log`: both methods passed after the ancestry preflight.
- `original-both-directions-red.log`: unchanged independent-review binary
  also fails the final isolated fixtures, including source below root.
  This later baseline check is sensitivity evidence, not a new test-first cycle.
- Fingerprints cover namespace, device/inode, mode, link count, size, mtime,
  ctime, payload bytes and link text. Traversal-induced atime is excluded.
- Two added mutants bypass the preflight or only the reverse ancestry check.
  Both fail their specific preservation assertions; all original four mutants
  still fail their intended assertions (`mutants.log`).

### Name bounds and snapshot ownership

`name-red.log` is an actual module assertion failure: a 61-byte metadata
command was accepted when checked independently from its docs path.
Manifest and metadata now use a command-specific 60-byte check. Package-name
and long-alias checks remain 63 bytes. The module tests command lengths
60 through 64. Lifecycle checks accept command 60, package/alias 63, and
version `65535.65535.65535`; reject longer names/aliases, leading-zero and
out-of-range versions, whitespace, signs and prerelease suffixes; and retain
source fingerprints. Existing valid/rejected variants are characterization,
not claimed as new production-defect RED cycles.

The public snapshot header now states that output must be unowned, normally
zero-initialized, and freed before reload. Load does not free live output.

### Variadic seam and analyzer diagnosis

A real forwarding defect was found in the test seam: `O_TMPFILE` needs its
mode argument even without `O_CREAT`. `seam-red.log` records the exact
`openat O_TMPFILE lost variadic mode` assertion before the fix;
`seam-green.log` passes afterward. `tests/pkg_openat_driver.c` checks real
no-mode opens, literal-int and mode_t `O_CREAT` modes, and `O_TMPFILE` mode.
It runs in both normal and ASan/UBSan suites. The host scratch filesystem
supports `O_TMPFILE`; the test does not silently skip this case.

The unrelated va_list analyzer diagnostic is a reproducible Clang-tidy 18
order-dependent false positive, not an observed uninitialized runtime list.
`va_min.c` is a 219-byte stdarg-only reproducer with adjacent va_start/va_end.
With the exact Makefile lint flags, it passes alone (`va-minimal-one.log`),
fails after `tests/pkg_driver.c` (`va-minimal-two.log`), and passes in reverse
order (`va-minimal-reverse.log`). The original real wrapper has the same
one-TU/order result (`lint-original-one.log`, `lint-original-two.log`,
`lint-original-reverse.log`). Unconditionally starting/ending the va_list
was also tried; full scoped analysis still fails at va_arg
(`lint-fixed-full-unsuppressed.log`).

One `NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)` is therefore placed
on that one test-wrapper va_arg expression, with a reason and this report
reference. No global check is disabled and no production analyzer warning is
suppressed. This is a narrow tool workaround, not a fix to Clang itself.
`lint-final.log` passes in normal source-first order with warnings as errors;
`global-lint.log` records successful unmodified `make lint`.
`format-final.log` records successful scoped clang-format checks.

Final `final-normal.log` and `final-sanitize.log` each report **44 tests, OK**,
including the original 39 methods, faults and module assertions. The sanitizer
build does not instrument the prebuilt Argtable library. No full application
regression or new independent review is claimed by this backend follow-up.

The verified follow-up session is `20261023_011314_f8f0a9`, title
`Subagent: Fix backend review blocker, command-name bound…`. Registration
is left to the parent to preserve this worker's explicit file ownership.

## Make dependency-closure follow-up

`make test-pkg` initially exited 2: 44 methods ran, but
`test_openat_variadic_forwarding` raised `FileNotFoundError` for
`build/pkg-tests/openat`. This is harness setup failure, not behavioral TDD
RED. `tests/pkg_build.py` already built that driver correctly.

The Makefile now explicitly builds it from `tests/pkg_openat_driver.c` and
`tests/pkg_faults.c`, using the existing compiler flags, package linker
wrappers and Argtable libraries. `test-pkg` requires it; `clean` removes it.
No production source or test was changed for this closure fix.

Evidence root: `$TMPDIR/pkg-make-closure`. Exact verification commands:

```sh
make test-pkg CFLAGS='-std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes -Werror -g3 -O0'
make clean
test ! -e build/pkg-tests/openat
test -f build/argtable3/lib/libargtable3.a
test -f build/cunit/lib/libcunit.a
make test all CFLAGS='-std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes -Werror -g3 -O0'
make lint
```

The first command failed before the edit (`before-test-pkg.log`). Cleanup
and dependency-preservation checks passed (`clean.log` records cleanup).
The full strict build/regression exited 0 (`after-test-all-strict.log`),
including the driver's compiler invocation and **44 package tests, OK**.
The builder suite, including its prefix-docs regression, passed 18 methods.
`make lint` exited 0 (`lint.log`); installed dependencies were retained.
The above full regression is integration evidence, not a new independent
backend review or completion of all milestone acceptance gates.

## Open gates

- Parent integration: real builtin inventory, registry capacity, main/persistent
  loop refresh, help, catalog, fork/exec status and child descriptor checks.
- Independent review and full integrated build/test/lint/format gates.
- Every malformed-schema/type combination and every syscall failure site are
  not exhaustively tested. In particular, there is no injected fstat/readdir/
  chmod/fcntl failure matrix, foreign-owner fixture, device/socket fixture, or
  interrupted private getopt scan guarantee. Current tests cover selected
  constructor-NULL and module-owned allocation ordinals, not internal Argtable
  panic/OOM recovery. The allocation sweep stops at the first ordinal with no
  injected failure on the install path; other operations are not fully swept.
- The build-reader docs-order finding above is resolved. No claim is made
  that all named milestone acceptance rows are complete.

The subagent session was verified as `20261022_235003_383a54`, title
`Subagent: Implement item4 pkg manager and persistent…`. Registration in the
existing sessions file was left to the parent because this worker owns only
new files.
