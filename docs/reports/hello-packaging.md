# Hello candidate: local metadata slice

## Scope and decisions

The user authorized autonomous routine work. This worker owns only
`src/cmd_hello.c`, `src/cmd_hello.h`, `src/hello_main.c`,
`tests/test_hello.py`, `tests/test_hello_module.c`, and this report.
The current dirty working tree is the baseline. No shared ABI, registry,
Makefile, source deletion, package output, stage, commit, or push is changed.
The parent owns shared adoption, package identity/version, package generation,
and the extra full-shell release. This candidate is not that release.

This is a project command, not a GNU compatibility implementation.

- No arguments: print `Hello, world!` and LF.
- `--name NAME` and `--name=NAME`: print `Hello, NAME!` and LF.
  An empty name is valid. No positional operands exist.
- `-h` and `--help`: print parser-derived help, including the default name.
  A supplied name does not change help.
- Support Argtable's unambiguous long-option prefixes: `--n`, `--na`,
  `--nam` for `--name`, and `--h`, `--he`, `--hel` for `--help`.
  Name prefixes accept separated or `=` values, including empty names.
  Duplicate, missing-value, help-priority, and `--` rules still apply.
  Generated metadata/help retain canonical names, not extra alias entries.
- Unknown options, missing values, duplicate options, and extra operands:
  status 2, stderr, no stdout. Errors take priority over help.
- Success: status 0. Allocation or output failure: status 1.
- Each call owns its Argtable entries and argv pointer copy. Borrowed strings,
  pointers, and exported getopt state remain unchanged.

## Local interface

```c
struct arg_hdr;
typedef int (*hello_argument_visitor)(const struct arg_hdr *, void *);
int hello_visit_arguments(hello_argument_visitor visit, void *context);
```

Parsing, help, public usage, and metadata use one argument builder. The visitor
sees name then help, not the terminator. Headers and strings are borrowed only
during each callback. Copy retained data; do not modify headers. NULL context
is valid. NULL visitor returns -1 with EINVAL. A nonzero callback stops the walk,
frees entries, and returns -1. Allocation failure returns -1 with ENOMEM;
success returns 0. The header declares this contract. Tests now use that strong
public declaration, not the earlier test-only weak reference.

No `cmd_spec_t` change is made. The parent can adopt this interface after review.
Headers are not a complete input schema. Defaults remain in help. This fixed
0..1 table does not solve argc-sized logical limits or `arg_rem` filtering.
Thread safety and private interrupted getopt scan restoration are not promised.

## TDD and decision ledger

Earlier evidence at `$TMPDIR/hello-j077byzq/` is preserved. In particular,
`green-name.log` contains an empty-name failure, not a passing result.

The parent supplied observed RED before this continuation:

- Help: `HELLO_HELP_STATUS` and `HELLO_HELP_WITH_NAME`, status 2 instead of 0.
- Usage: `HELLO_USAGE_DEFINITIONS`.
- Metadata modes: `HELLO_METADATA_API_MISSING`.
- Both compilations succeeded. Empty name previously returned 2.

This worker then observed:

1. After the shared builder/help/visitor implementation, help and help-with-name
   passed. New `test_empty_name_forms` failed twice with `HELLO_EMPTY_NAME`,
   status 2 instead of 0, for separated and attached empty values. The new
   duplicate/missing/unknown/extra-operand cases passed before normalization.
2. The installed header exposes `arg_end.count`, `error`, `parent`, and `argval`.
   The supporting archived upstream parser registers `ARG_EMISSARG` for an empty
   required long value, then scans that value. The fix accepts only one error,
   `ARG_EMISSARG`, with exactly one scanned empty name. All other errors remain.
3. The seven-test CLI suite and four metadata/usage modes then passed.
4. Added characterization tests for invalid arguments, output failures through
   `/dev/full`, repeated calls, borrowed argv/getopt state, and invalid module
   arguments. The expanded nine-test suite and module state mode passed.
5. A malformed `-h-` regression timed out at three seconds before the guard.
   The dependency parser did not complete; this is a runtime defect, not a
   compilation failure. `-h--` and a control-byte variant were also tested.
   A narrow preflight rejects internal hyphens in short-option clusters before
   parsing. It skips required name values and stops at `--`; a name of `-h-`
   remains valid. The nine-test suite then passed with these cases.
6. The first scoped format check failed. Applied clang-format only to owned C
   files. The second format check and scoped clang-analyzer check passed.

The empty-value exception does not make `--name` optional. The short-cluster
guard is a dependency safety check, not a second general option parser.

## Evidence and reproduction

Final verification command, from the repository root:

```sh
python3 "$TMPDIR/hello-j077byzq/verify-resume.py"
```

The runner saves exact argv, full stdout/stderr, exit statuses, and final owned
file SHA-256 values in `resume-execution.log` and `resume-evidence.json` in that
same scratch directory. It builds standalone and C module executables with
strict C17 warnings. It runs the nine CLI tests and five module modes in normal
and ASan/UBSan builds, then scoped format, analyzer, and whitespace checks.
Final runner passed: all normal and sanitizer builds/tests, five module modes
per build, scoped format, analyzer, and whitespace checks returned status 0.
The CLI suite ran nine tests in each build with no skips. No command was denied.
The sanitizer pass covers project code linked to the installed Argtable library;
it does not prove that dependency library was built with sanitizer coverage.

Build shape without the runner:

```sh
cc -std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion \
  -Wstrict-prototypes -g3 -O0 -Isrc -Ibuild/argtable3/include \
  src/hello_main.c src/cmd_hello.c -Lbuild/argtable3/lib -largtable3 -lm \
  -o "$TMPDIR/hello-j077byzq/hello-check"
python3 tests/test_hello.py "$TMPDIR/hello-j077byzq/hello-check"
```

For module tests, replace `src/hello_main.c` with `tests/test_hello_module.c`.
Run modes `usage`, `metadata`, `metadata-null`, `metadata-stop`, and `state`.
Sanitizer builds add `-fsanitize=address,undefined -fno-omit-frame-pointer
-no-pie`. The module state mode deliberately prints three greetings and three
invalid-module diagnostics.

## Review disposition and follow-up verification

Parent verification passed at
`/home/name/.hermes/profiles/embx420/cache/scratch/hello-parent-69PSJz`.
Independent review passed with no blocking defect at
`/home/name/.hermes/profiles/embx420/cache/scratch/hello-review-36l1zv9l`:
1,445 grammar edges, 2,187 environment cases, and normal/sanitized fault probes.
The local hello slice is reviewed; shared integration is not complete.

Low findings resolved:

- Reuse one static `long_help` string in the specification and rendered help.
  This is a behavior-preserving refactor, not a new RED claim.
- Retain Argtable abbreviation handling rather than introduce an option parser.
  New abbreviation and error tests passed on the prior parent executable:
  11 tests, characterization only, saved in `characterization.log`.
- A further test found a real guard inconsistency: separated `-h-` values
  after each name prefix returned 2 instead of 0 (three failures in
  `hyphen-red.log`). The dependency-stall guard now skips those values as
  it already did for `--name`; Argtable still validates and parses options.
  The new regression passed after that narrow fix.

New evidence is isolated at
`/home/name/.hermes/profiles/embx420/cache/scratch/hello-low-fixes-kfozfmp0`.
Original implementation, parent, and review evidence remain unchanged.
Reproduce from the repository root:

```sh
python3 "$TMPDIR/hello-low-fixes-kfozfmp0/verify-resume.py"
```

The copied runner passed strict standalone/module builds, 12 CLI tests in
each normal and ASan/UBSan build, all five module modes in both builds,
scoped format, analyzer, and whitespace checks. Exact commands and streams
are in `resume-execution.log`; hashes are in `resume-evidence.json`.
No stage or commit was performed. Parent verification of the final candidate
and shared adoption remain separate gates.

## Shared-export continuation

The local-only scope above is historical. The follow-up adds the optional
`cmd_spec_t.visit_metadata` provider and `cmd_metadata_write`. Hello parsing,
help, and both visitors still use one builder. Help now reads the command name
from its specification. `make package-hello` generates the source-manifest-driven
hello 0.1.0 directory, JSON, and Markdown. Hello remains package-only; no shell
or registry source was changed. See [package build](package-build.md) for the
callback ownership, TDD slices, exact evidence paths, warnings and limits.

The final follow-up runner passed 31 commands: strict normal and ASan/UBSan
builds, 12 CLI cases and five local module modes per build, shared metadata
failure tests, nine package tests, scoped format/analyzer, `make all`, and the
full current `make test`. Existing positional builtin initializers safely leave
the optional member NULL, but emit missing-field warnings. Parent-owned explicit
NULL maintenance is needed before a global warnings-as-errors build. Independent
review of this shared/package candidate and later lifecycle stages remain open.

## Prior local-slice gates and limits

These gates describe the earlier local slice. The continuation above supersedes
its shared-export and build status.

- The original local slice passed independent review. This follow-up remains
  an unstaged candidate for parent final verification.
- Shared ABI adoption, shell integration/parity, generated metadata/docs,
  install/remove behavior, package artifacts, and full-shell release testing
  belong to the parent and are not proved by standalone tests.
- Independent review added bounded grammar/environment exploration and fault
  probes. Deliberate lifetime misuse, forced pointer-permutation negative
  controls, exhaustive fuzzing, and full-project regression are not claimed.
- Dependency-internal allocation aborts are not a general recoverable-OOM
  guarantee. Public usage has a void API and cannot return output failures.
- Scratch evidence must be retained or collected by the parent before release.
  No historical AI log was read or changed; this report is the local task ledger.
