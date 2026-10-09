# stat: inventory, contract, and report

Status: implemented and independently reviewed on 2026-10-08; verification
passed except pre-existing repository-wide formatting failures listed below.
Scope selected under the user's autonomous authorization in NEXT_BATCH.md.
The active user authorizes per-command commits by the main orchestrator;
review/staging workers do not commit. Publication and push remain unauthorized.

## Sources and purpose

Inspect file metadata, including symlinks. Course: CLI_Tools.docx paragraphs 25–28, retrieved from
docs/data.sqlite. Manual: installed man 1 stat, GNU coreutils 9.4,
footer August 2026, rendered with man-db on 2026-10-07. Every option and
functional behavior in that manual is catalogued below. Author/copyright/
bug-reporting/related-manual sections are provenance, not executable features.

## Manual inventory

| Feature | Decision | Human/AI task or reason |
| --- | --- | --- |
| File metadata; default inspects symlink itself | Keep | Course metadata task, without opening file contents |
| -L, --dereference | Keep | Inspect link target when explicitly requested |
| Multiple FILE operands | Defer | One metadata object per invocation for this slice |
| -f, --file-system | Defer | Filesystem capacity/identity is a separate task |
| --cached=always|never|default | Defer | No remote-filesystem synchronization policy in scope |
| -c, --format=FORMAT; --printf=FORMAT and escape interpretation | Defer | Use one documented human summary and course JSON schema |
| -t, --terse and its two documented format expansions | Defer | JSON supplies structured output |
| --help | Keep | Discovery; add course -h |
| --version | Defer | Not necessary for metadata inspection |
| -- ends options | Keep | Dash-prefixed filenames |
| --json (course addition) | Keep | path/type/size/mode/mtime/atime/ctime object |
| File format sequences: %a %A %b %B %C %d %D %Hd %Ld %f %F %g %G %h %i %m %n %N %o %s %r %R %Hr %Lr %t %T %u %U %w %W %x %X %y %Y %z %Z | Defer format language | Retain required metadata fields, not a formatting engine |
| Filesystem sequences: %a %b %c %d %f %i %l %n %s %S %t %T | Defer | Depends on filesystem mode |
| Shell builtin may supersede executable | Document | Use explicit reference path /usr/bin/stat |

## Selected contract

Syntax: `stat [-h|--help] [-L|--dereference] [--json] [--] FILE`.

- Status 0 succeeds, 1 reports I/O/resource/encoding failure, 2 reports invalid
  arguments. Diagnostics go to stderr; stdout contains data or requested help.
- Invalid arguments take precedence over help. Help performs no input reads.
- Options/help share argtable3 definitions. Preserve caller argv and inherited
  streams. Linux target; inherit signal dispositions; do not change shell state.
  With default SIGPIPE, a broken output pipe terminates by signal; when SIGPIPE
  is inherited as ignored, the same output failure returns status 1.
- Unsupported options are argument errors; inherited unambiguous long-option
  abbreviations are accepted by argtable3, not a new feature promise.
- Exactly one FILE; '-' is a literal filename, not stdin. lstat by default,
  stat with -L. A dangling symlink succeeds without -L and fails with -L.
- Human summary fields: File, Type, Size (bytes), Mode (octal), Access, Modify,
  Change (whole seconds since Unix epoch). This intentionally differs from
  GNU's full default display; compare to an explicit GNU --printf adapter.
- JSON fields: path, type, size (integer), mode (four-octal-digit string),
  mtime, atime, ctime (integer epoch seconds). Types use human-readable labels.
  JSON requires valid UTF-8 in the path; metadata lookup never reads file data.

## Implementation and verification

- Files: src/cmd_stat.c/.h, src/stat_main.c; shared helpers only where
  actual duplication warrants them. Integrate build/test targets in Makefile.
- Test driver: tests/test_stat.py, standard-library CLI fixtures with an
  explicit executable and reference/project modes; isolated cwd and locale.
- Sequence: fixed expected results against reference; negative control;
  runnable baseline RED; minimal implementation GREEN; regression and lint.
- Cover normal, empty, binary/Unicode where relevant, option errors, unusual
  paths, output failure, and repeated module calls. Compare stdout, stderr,
  exit status, and unchanged input files. Full GNU formatting is not our oracle
  for deliberately different human/JSON output.
- No registry currently exists; test cmd_spec_t and standalone parity without
  claiming shell registry integration. The review worker made no commit or push;
  the main orchestrator is authorized to commit each verified command.

## Results

### Independent review and coverage

Unattended `pwd` and a scratch write/read/delete probe succeeded without a user
approval prompt. The six-method existing implementation suite passed before
review changes. No production defect was reproduced; source and Makefile were
left unchanged. Corrected this report's stale status and driver name, and added
characterization tests to close coverage gaps:

| Contract / risk | Passing evidence |
| --- | --- |
| Exact human fields; binary/empty inputs; literal `-`, `--`, Unicode/control paths; no file-data reads/modification | `test_file_metadata_without_reading` |
| lstat/stat selection; directory, FIFO, socket, normal/dangling symlink; missing object | `test_types_and_dereference` |
| Missing/extra operands; unsupported options; invalid arguments before help | `test_arguments` |
| JSON keys/types, special mode bits, escaping, negative epoch values, invalid UTF-8 rejection | `test_json`, `test_invalid_utf8` |
| JSON file-type coverage, dereferencing, 24 deterministic generated control/Unicode paths | `test_json_types_and_generated_paths` |
| Empty path, non-directory component, missing component, symlink loop | `test_lookup_failures` |
| Sparse size 8589934599 bytes, sticky mode, negative timestamps, character device | `test_metadata_boundaries` |
| Human/JSON/help output errors and inherited SIGPIPE policy | `test_output_failure`, `test_broken_pipe_inherits_signal_policy` |
| Reuse after argument/lookup/help/data-output failure; unchanged argv; borrowed descriptors; untouched stdin position; specification help | `tests/test_stat_module.c` |

Ownership review: parser definitions and argv pointer copy are invocation-local,
freed through the shared cleanup path, including parser/help/lookup/output errors.
Argv strings and inherited streams/descriptors are borrowed. Metadata lookup uses
`lstat`/`stat`, not input opens/reads; output uses the existing short-write/EINTR
helper. UTF-8 validation precedes any JSON output. Caller-provided module arguments
must have valid non-NULL entries for `argc >= 1`; malformed caller vectors are
outside the API contract. No registry implementation exists under `src`.

New review tests passed on their first run: they characterize existing behavior,
not a new RED→GREEN implementation cycle. Historical runnable baseline RED is
reported in HERMES_HANDOFF.md; it was not rerun here. The negative control below
was rerun against the expanded suite and rejected.

### Exact execution evidence

Run from the repository root with
`export TMPDIR=/home/name/.hermes/cache/scratch`.
Logs are in `/home/name/.hermes/cache/scratch/stat-review-20261008/`
(scratch files are not committed and may be pruned).

| Command | Observed result | Log |
| --- | --- | --- |
| `make test-stat-impl` before review edits | Exit 0; six CLI tests and module assertions passed | `baseline.log` |
| `make -B build/stat build/test_stat_module` | Exit 0; fresh compilation with project warning flags | `forced-build.log` |
| `make test-stat-impl` after review | Exit 0; 10 CLI methods, no skips; module assertions passed | `stat-impl.log` |
| `make test-stat` | Exit 0; four compatible methods passed, six project-only methods skipped; GNU coreutils 9.4 | `reference.log` |
| `python3 tests/test_stat.py /usr/bin/true` | Exit 1, expected rejection; 12 assertion failures and 31 JSON decode errors | `negative-control.log` |
| `make test` | Exit 0; accumulated traversal/cat/head/tail/pwd/stat regression passed | `regression.log` |
| `make lint` | Exit 0; project clang-tidy analysis passed | `lint.log` |
| `make all` after regression | Exit 0; all delivery binaries up to date | `build.log` |
| `clang-format --dry-run --Werror src/cmd_stat.c src/cmd_stat.h src/stat_main.c tests/test_stat_module.c` | Exit 0 | `scoped-format.log` |
| `make format-check` | Exit 2; failures only outside stat scope | `global-format.log` |

Repository formatting failures remain in `src/main.c`, `src/traverse.c`,
`src/traverse.h`, `src/cmd_spec.h`, `tests/test_help.c`, `tests/test_recursive.c`,
and `tests/test_output.h`. The review initially introduced include alignment
violations in the expanded stat module test; scoped `clang-format -i
tests/test_stat_module.c` fixed those, and the final scoped check passed.
No unrelated files were reformatted.

Sanitizer compilation and execution both exited 0; module output was
`stat module: reuse, argv, stdin position, error recovery passed`, with no
ASan/UBSan/leak diagnostics. Exact invocation:

```sh
EVIDENCE=/home/name/.hermes/cache/scratch/stat-review-20261008
cc -Ibuild/argtable3/include -std=c17 -g -O1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -o "$EVIDENCE/test_stat_module_asan" \
  tests/test_stat_module.c src/cmd_stat.c src/cmd_lines.c \
  -Lbuild/argtable3/lib -largtable3 -lm
ASAN_OPTIONS=detect_leaks=1 "$EVIDENCE/test_stat_module_asan"
```

Logs: `sanitizer-build.log`, `sanitizer.log`. `command -v valgrind` exited 1:
Valgrind is unavailable, not a passing check.

### Limits and delivery scope

- No registry discovery/dispatch test is possible; only standalone and direct
  `cmd_spec_t` execution were exercised.
- No allocation-failure injection, block-device fixture, permission-denial
  injection, or stat-specific short-write/EINTR injection was performed.
  Cleanup was reviewed and ordinary/error-path module reuse was sanitized;
  that does not establish every allocation failure path. The linked argtable3
  library itself was not rebuilt with sanitizers.
- Deterministic generated cases are not an arbitrary-input fuzz campaign.
  Multiple operands, GNU formatting/filesystem mode, and other deferred
  inventory features remain unsupported by design.
- This review changed only `tests/test_stat.py`, `tests/test_stat_module.c`, and
  `docs/commands/stat.md`; no commits, staging, AI_LOG access, or publication.
  Stat artifacts also include `src/cmd_stat.c`, `src/cmd_stat.h`,
  `src/stat_main.c`, and the existing stat integration in Makefile.
- Behavior is ready for a scoped commit with these explicit limitations.
  The isolated stat staging includes `src/cmd_spec.h` and `src/cmd_lines.c/.h`
  because these shared dependencies were not yet in HEAD. `cmd_spec.h` defines
  the module interface; stat uses all three existing `cmd_lines` helpers:
  `lines_write`, `lines_utf8`, and `lines_quote`. The shared files are included
  unchanged, without refactoring or including earlier command implementations.
  The staged Makefile is constructed from HEAD with only argtable3 flags and
  stat build/reference/module/regression/clean/lint integration; the working
  Makefile retains its earlier command integrations unchanged.

### Isolated staged-tree verification

On 2026-10-08 the staging worker extracted the index with
`git checkout-index --all --prefix=.../tree/` into
`/home/name/.hermes/cache/scratch/stat-stage-20261008/tree/`.
Only `build/argtable3` and `build/cunit` were symlinked to the existing installed
build dependencies; no earlier command sources, other untracked files, or
credentials were copied. All commands below ran from the isolated tree with
`TMPDIR=/home/name/.hermes/cache/scratch` and `LC_ALL=C`.
Evidence logs are in `/home/name/.hermes/cache/scratch/stat-stage-20261008/`.

| Command | Observed result | Log |
| --- | --- | --- |
| `make test-stat-impl` | Exit 0; 10 CLI methods, no skips; module assertions passed | `stat-impl.log` |
| `make test-stat` | Exit 0; four compatible methods; six intentional skips | `reference.log` |
| `make test` | Exit 0; HEAD traversal/help regression plus stat CLI/module tests | `regression.log` |
| `make all` | Exit 0; delivery build | `build.log` |
| `make clean` then `make all` | Both exit 0; fresh traversal/stat build without earlier command sources | `clean.log`, `clean-build.log` |
| `make lint` | Exit 0; all eight C translation units in the staged tree | `lint.log` |
| `clang-format --dry-run --Werror src/cmd_stat.c src/cmd_stat.h src/stat_main.c tests/test_stat_module.c src/cmd_lines.c src/cmd_lines.h` | Exit 0 | `scoped-format.log` |
| `make format-check` | Exit 2; same seven pre-existing files listed above | `global-format.log` |

The unchanged shared `cmd_spec.h` also retains existing trailing whitespace and
an extra final blank line, so `git diff --cached --check` reports those two
findings. These and its formatting failures are preserved rather than silently
refactoring the shared dependency. They do not prevent compilation or tests.
The review worker's no-staging/no-commit record above is historical; this worker
staged the isolated command and dependencies but made no commit or push.

Examples: `./build/stat --json -- FILE`, `./build/stat -L FILE`,
`./build/stat --help`.
