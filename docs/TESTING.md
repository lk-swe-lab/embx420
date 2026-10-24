# Build and test from a clean checkout

## Requirements and scope

The verified path is native **Linux x86-64/glibc**, GCC 13.3, GNU make,
Python 3.14, CMake 3.31.6, Autoconf 2.72, Automake 1.18.1, Libtool 2.5.4,
and M4 1.4.21. Other distributions/tool versions are not certified here;
Windows, macOS and cross-compilation were not tested. Project tests use Linux
facilities and GNU linker `--wrap`.

Have Python 3.9+ (with HTTPS, gzip and bzip2 support), CMake 3.16+, a native C17
compiler/libc development headers, GNU make, binutils (`ar`, `ranlib`), Perl,
M4, Autoconf, Automake, Libtool (`libtoolize`), and ordinary POSIX build tools
available on `PATH`. CMake 4 was not tested. No Python third-party package is
needed by the bootstrap. Optional analysis needs `clang-tidy`/`clang-format`.

On an Ubuntu installation, a prerequisite package command for a human/admin is:

```sh
sudo apt-get install build-essential cmake python3 ca-certificates autoconf automake libtool m4 perl
```

This is not run by the bootstrap. Verification installed **no system packages**;
missing build tools were supplied privately in scratch. HTTPS access to GitHub
codeload and SourceForge (including its download mirrors), working CA trust,
and writable local disk are required. Dependency sources are downloaded and
built, not vendored. No `sudo`, Git credentials, or package-manager invocation
is used by the script.

## Bootstrap and run

From the repository root, with no existing `build/argtable3` or `build/cunit`:

```sh
python3 scripts/bootstrap_dependencies.py
make all
make test
make test-shell-release
```

The script builds static libraries into the Git-ignored Makefile defaults:
`build/argtable3/{include,lib}` and `build/cunit/{include,lib}`. Argtable's upstream
archive is named `libargtable3_static.a`; a local `libargtable3.a` symlink makes
it usable with the existing `-largtable3`. No Makefile change is necessary.
Library licenses are retained under each prefix's `share/licenses/`.

**Existing prefixes are always refused**, even empty or previously bootstrapped
ones. Both are checked before downloads/builds; symlinked destination ancestry
is refused too. The script never deletes/replaces a prefix. On failure, partial
prefixes remain for inspection; select a fresh root rather than deleting unknown
content. Only its own randomly created temporary build directory is cleaned up.
Downloads must match the embedded SHA-256 before extraction; archive absolute
paths, traversal, links and special files are rejected.

A fresh custom root and exact Make overrides (use paths without spaces, as the
Makefile expands compiler flags unquoted):

```sh
DEPS="$PWD/build/deps-fresh"
python3 scripts/bootstrap_dependencies.py --build-root "$DEPS" --jobs 2
make all test test-shell-release \
  ARGTABLE3_CPPFLAGS="-I$DEPS/argtable3/include" \
  ARGTABLE3_LDLIBS="$DEPS/argtable3/lib/libargtable3.a -lm" \
  CUNIT_CPPFLAGS="-I$DEPS/cunit/include" \
  CUNIT_LDLIBS="$DEPS/cunit/lib/libcunit.a"
```

Absolute archive overrides guarantee these newly built libraries are used.
The same four Make variables support an independently managed installation.
System `libcunit1-dev` is an alternative on Ubuntu, with `CUNIT_CPPFLAGS=` and
`CUNIT_LDLIBS=-lcunit`; that alternative was **not** exercised in this verification
(the system package was absent). Do not substitute Argtable2: sources include
`argtable3.h` and require the Argtable3 API.

`make clean` preserves dependency prefixes. Changing libraries/compiler flags
requires rebuilding project binaries; immutable generated package outputs can
reject changed bytes at the same version. Use a fresh source/build snapshot for
that check, rather than overwriting an existing package directory.

## Pins, primary sources, and licenses

These SHA-256 values were computed from actual HTTPS downloads, then enforced
again by the executed bootstrap. They are repository pins, not claims of signed
upstream checksums.

| Dependency | Pinned source | SHA-256 |
| --- | --- | --- |
| Argtable3 3.2.2 (`v3.2.2.f25c624`) | commit `f25c624e3a5a65f4d1897aa7d51c897f8f8098d0` | `a0fc43317d31447dc2dda5852e71e4089e7eb4ed8429e2315a36533cfb4ce9e3` |
| CUnit 2.1-3 | official `CUnit-2.1-3.tar.bz2` | `f5b29137f845bb08b77ec60584fdb728b4e58f1023e6f249a464efa49a40f214` |

- Argtable3 [download](https://codeload.github.com/argtable/argtable3/tar.gz/f25c624e3a5a65f4d1897aa7d51c897f8f8098d0),
  [pinned build instructions](https://github.com/argtable/argtable3/blob/f25c624e3a5a65f4d1897aa7d51c897f8f8098d0/README.md#build-from-release-archives-or-source),
  [CMake options](https://github.com/argtable/argtable3/blob/f25c624e3a5a65f4d1897aa7d51c897f8f8098d0/CMakeLists.txt),
  [license](https://github.com/argtable/argtable3/blob/f25c624e3a5a65f4d1897aa7d51c897f8f8098d0/LICENSE):
  BSD-3-Clause plus the bundled getopt/Tcl/hash-table/string-library notices.
- CUnit [release listing](https://sourceforge.net/projects/cunit/files/CUnit/2.1-3/),
  [download](https://downloads.sourceforge.net/project/cunit/CUnit/2.1-3/CUnit-2.1-3.tar.bz2),
  [API guide](https://cunit.sourceforge.net/doc/introduction.html).
  The archive's `README` and `bootstrap` document the GNU build sequence;
  it lacks generated `configure`, so the script runs libtoolize, aclocal,
  autoheader, automake, autoconf, then configure/make/install. Its `COPYING`
  contains the [GNU Library GPL v2](https://www.gnu.org/licenses/old-licenses/lgpl-2.0.html);
  source headers permit v2 or later.

Argtable uses upstream CMake Release/static builds, tests/examples disabled,
explicit `lib` installation. CUnit uses `--enable-static --disable-shared
--disable-curses --disable-examples --disable-test` with an explicit prefix and
libdir. Do not export project `-Werror` flags while building old dependencies:
CUnit emits upstream format warnings and Autotools emits deprecation warnings;
those are not project compile warnings and were not patched or hidden.

The checked project API surface is Argtable `arg_lit0`, `arg_str0/arg_strn`,
`arg_file0/arg_filen`, `arg_end`, `arg_parse`, `arg_nullcheck`, `arg_freetable`,
and syntax/glossary/error printers; CUnit registry/suite/test registration and
`CU_ASSERT*` macros. The existing ignored CUnit header identified 2.1-3. No
reliable version/provenance metadata identified the old ignored Argtable binary;
verification used only the newly pinned build, not that binary.

## Verified execution

In an isolated copy of current `src/`, `tests/`, `scripts/`, `packages/` and
`Makefile`, without copying ignored dependency installations, bootstrap succeeded
both with a fresh custom root and with default `build/` prefixes. Actual CMake,
Autotools, configure, make and install were executed. Then:

```sh
make -B \
  CFLAGS='-std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes -Werror -g3 -O0' \
  all test test-shell-release
```

Exited **0**, using default newly bootstrapped libraries. This rebuilt hello,
main and CUnit tests with strict project diagnostics, ran the full `make test`
(including CUnit help/traversal/cat/module checks), and ran release tests.
The Python runners reported **299 tests across 20 suites**, all `OK`; C module
checks passed as well. Targeted custom-root checks also passed: hello, cat,
package build (18), package shell (13), pkg (44), shell release (12), plus direct
CUnit help/traversal executions. Counts are runner-reported Python cases, not a
combined count of all C assertions.

Safety checks passed for repeated-prefix byte preservation, existing second
prefix preflight, symlinked roots/ancestors, invalid jobs, missing tools, changed
download bytes, and malicious archive traversal/absolute paths/symlinks. These
checks do not certify untested platforms or upstream dependency internals.
