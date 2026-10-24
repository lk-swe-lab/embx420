# Build and test the shared-module shell

## Prerequisites

Native Linux with compatible ELF/libc, C17 compiler, GNU make/binutils, Python
3.9+, HTTPS/CA trust, and writable local disk. Loader/store use `/proc/self/exe`,
`renameat2` and GNU linker facilities. No Windows/macOS/cross-platform claim.
CUnit bootstrap also needs Perl, M4, Autoconf, Automake and Libtool; analysis
needs clang-tidy/clang-format. No system package installation is automated.

From a fresh checkout with the named dependency prefixes absent:

```sh
python3 scripts/bootstrap_dependencies.py --build-root build/module-deps --only argtable3
python3 scripts/bootstrap_dependencies.py --build-root build --only cunit
make all
make test
make lint
make release-shell
python3 demo/packaging-demo.py
```

Make defaults are `build/module-deps/argtable3/{include,lib}` (PIC static
Argtable) and `build/cunit/{include,lib}`. The bootstrap now compiles Argtable
PIC objects directly with cc/ar; it does not need CMake. CUnit retains its
Autotools static build. Do not use an old non-PIC Argtable archive for modules.
No Python third-party package is needed for bootstrap/demo.

Existing prefixes are refused, never replaced—even empty ones. Failed prefixes
remain for inspection. Choose a fresh root, not deletion of unknown artifacts.
Symlinked ancestry and unsafe archive entries are rejected; embedded SHA-256
pins are checked before extraction. Licenses remain under `share/licenses/`.
These are repository pins, not signed upstream checksum claims:

| Dependency | Pinned source | SHA-256 |
| --- | --- | --- |
| Argtable3 3.2.2 | commit `f25c624e3a5a65f4d1897aa7d51c897f8f8098d0` | `a0fc43317d31447dc2dda5852e71e4089e7eb4ed8429e2315a36533cfb4ce9e3` |
| CUnit 2.1-3 | `CUnit-2.1-3.tar.bz2` | `f5b29137f845bb08b77ec60584fdb728b4e58f1023e6f249a464efa49a40f214` |

The script's `SOURCES` records download locations. Argtable's BSD-3-Clause plus
bundled notices and CUnit's Library GPL v2-or-later notices must be retained.
Dependency-internal warnings/recovery are not project-code guarantees.

For an alternate *fresh* root, bootstrap both dependencies there and override
`ARGTABLE3_CPPFLAGS`, `ARGTABLE3_LDLIBS`, `CUNIT_CPPFLAGS`, `CUNIT_LDLIBS` in Make.
Use absolute include/archive paths without spaces. Changing flags/dependencies
requires a fresh project build snapshot; don't clean/rebuild a shared review
workspace. Immutable package/release outputs reject different bytes at the same
version: use a new version/output root, never overwrite mapped libraries.

## Targets and focused checks

- `make test`: accumulated standalone, shell, loader, package and release checks.
- `make test-package-shell`: native host/package integration, modules, loader and
  native transaction fault checks (not the legacy backend suite).
- `make test-pkg`: retained executable-backend fault fixtures, **not** live pkg
  schema-1 compatibility. `make package-hello-legacy` uses `build/legacy-packages`.
- `make test-package test-shell-release`: package builder and complete-release
  source-derived exact inventory/provenance/relocation and dependency checks.
  Module builds refuse stale/unknown/unsafe outputs before cc; choose a fresh
  owned output directory or explicitly handle stale files, never auto-prune.
- `make format-check`: non-mutating formatting check; `make lint`: static analysis.

Demo syntax without bytecode:

```sh
python3 -c 'import ast; from pathlib import Path; ast.parse(Path("demo/packaging-demo.py").read_text())'
```

`tests/test_modules.py`, `tests/test_loader.py`, `tests/test_shared_package.py`
and supporting module/fault fixtures cover native loading/version behavior.
`tests/test_native_transactions.py` exercises real native pkg allocation/I/O
failures and honest post-commit close diagnostics; `tests/test_module_build.py`
covers stale output inventory and Make dependency sensitivity. Metadata mutation
checks must reject drift from the actual loaded spec on install and use.
All standard modules intentionally load eagerly at startup, not lazily.
Inspect each runner's actual invocation before using it independently.
Required boundaries include ABI/entry/conflict rejection, same-session install/
remove, cd/exit state, distinct-version restart/rollback, stdin and standalone
parity, exact payload namespaces, output/fault cleanup and relocated releases.
Trusted native code can terminate/corrupt the parent; no sandbox is tested.

## Evidence scope

Parent `make -j1 all test lint release-shell` passed in the working tree and a
separate complete source snapshot. The snapshot used the installed dependency
prefixes; this is not a new network bootstrap claim. Required ignored build
helpers were included and are now explicitly unignored.

Parent ASan+UBSan builds passed modules, loader, native transactions, shared
packages and shell suites after the final numeric-equivalence fix. Dependency
archives and the optional hello payload were not sanitizer-instrumented.
The real demo passed **11 processes, 22 loop inputs, 27 checks**, exit 0, with
all 80 input hashes and cleanup verified. The demo alone is not a full regression
or distinct-version replacement test. Global formatting retains five unchanged
baseline failures; changed C formatting and whitespace checks pass.
See [status](STATUS.md), [verification report](reports/so-redesign.md) and
[demo](../demo/PACKAGING.md) for scope and evidence.
