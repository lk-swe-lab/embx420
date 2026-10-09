# cat: manual inventory and scope decisions

Status: approved core implemented as `build/cat` and a reusable command module.
CLI and module tests pass; full-project formatting has pre-existing failures.
Task examples below are proposed uses, not verified user or agent requirements.

## Sources

- **M:** installed `man 1 cat`, GNU coreutils 9.4, footer August 2026;
  rendered with man-db 2.12.0 on 2026-10-06.
- **I:** installed `info coreutils 'cat invocation'`, section 3.1.
- **C:** installed `info coreutils 'Common options'`, section 2.
  The Info excerpts do not state their version; do not assume it matches M.
- **S:** `data/02/CLI_Tools.docx`, queried through `docs/data.sqlite`.
  Paragraph 2 names both human users and agent backends; paragraphs 15–16
  identify agent-facing core tools and prefer built-ins where feasible.
  Paragraphs 30–33 specify cat, help, and optional JSON.

Inventory covers M in full, with relevant I/C supplements. Common-option rules
are general GNU documentation; cat-specific edge cases need verification if kept.
Author, copyright, bug-reporting, and related-tool links are provenance, not
functional features. Other utilities linked by the manual are outside this scope.

## How to decide

1. Agree on concrete human and AI tool tasks, separately from output format.
2. Keep course requirements; distinguish them from optional course suggestions.
3. For each other feature, ask whether removing it blocks an agreed task and
   whether composition with another tool already supplies the capability.
4. Record keep/defer and a reason for every row. Check dependencies, complete
   workflows, boundaries, and failure handling before approving the contract.

Unapproved inventory rows remain undecided. Approval of the core behavior below
does not approve optional transformations, JSON, or additional integration.

## Operands, defaults, and streams

| Feature / reference | Proposed human or AI task | Course requirement | Decision | Reason / question |
| --- | --- | --- | --- | --- |
| Print file contents to stdout (M DESCRIPTION; S30) | Human reads a file; AI retrieves source or configuration | Required | Keep | Implemented and tested core purpose |
| Multiple files, in operand order (M SYNOPSIS/EXAMPLES; I) | Combine documents or supply ordered context to another tool | Not explicit in S30–33 | Keep | Approved: concatenate in argument order, without added separators or labels |
| No file operands reads stdin (M DESCRIPTION) | Consume a pipe or redirected input | Not explicit | Keep | Approved: read stdin when no file operands are supplied |
| `-` operand reads stdin, including between files (M EXAMPLES) | Combine stream data and named files | Not explicit | Keep | Approved: read stdin at this position in operand order |
| Default copying without display transformations (I opening) | Preserve content for review and downstream processing | Print contents; byte contract unspecified | Keep | Tested byte preservation, including empty/binary data and missing final newlines |
| Text/binary mode on platforms such as MS-DOS (I) | Portability to systems with distinct file modes | Not explicit | Undecided | I describes binary default, text exceptions for terminals and `-bensAE`; is this platform in scope? |

## Every option in the installed man page

| Feature / reference | Proposed human or AI task | Course requirement | Decision | Reason / question |
| --- | --- | --- | --- | --- |
| `-A`, `--show-all`: combines `-vET` (M/I) | Inspect invisible bytes and line boundaries | Not listed | Undecided | Depends on all three transformations |
| `-b`, `--number-nonblank`: number nonempty lines from 1; overrides `-n` (M/I) | Refer to nonempty output lines | Not listed | Undecided | Are these useful references for an agreed task? |
| `-e`: combines `-vE` (M/I) | Inspect nonprinting bytes and line endings | Not listed | Undecided | Alias for two other features |
| `-E`, `--show-ends`: mark line ends with `$`; CRLF appears as `^M$` (M/I) | Diagnose line-ending issues | Not listed | Undecided | Alters output content |
| `-n`, `--number`: number all lines from 1; ignored with `-b` (M/I) | Human or AI cites line locations | Not listed | Keep | Approved: number every output line, including empty lines, continuously across files; numbering is opt-in. `-b` remains undecided |
| `-s`, `--squeeze-blank`: collapse adjacent empty lines (M/I) | Reduce blank-line noise | Not listed | Undecided | Loses original spacing |
| `-t`: combines `-vT` (M/I) | Inspect tabs and nonprinting bytes | Not listed | Undecided | Alias for two other features |
| `-T`, `--show-tabs`: display tabs as `^I` (M/I) | Diagnose indentation or delimiters | Not listed | Undecided | Alters output content |
| `-u`: accepted but ignored (M/I) | Accept a compatibility invocation | Not listed | Undecided | No added data-processing capability |
| `-v`, `--show-nonprinting`: caret/M- notation, excluding LF and tab (M/I) | Inspect control and high-bit bytes | Not listed | Undecided | Escaped display is not original content |
| `--help`: show usage/options, then succeed (M/C; S32) | Human or AI discovers invocation | Listed for cat | Keep | Approved: print help without reading input; argument errors take precedence |
| `--version`: show version, then succeed (M/C) | Identify tool behavior when diagnosing a run | Not listed | Undecided | Is version discovery needed now? |

## Shared parsing and status rules

| Feature / reference | Proposed human or AI task | Course requirement | Decision | Reason / question |
| --- | --- | --- | --- | --- |
| `--` ends option parsing (C) | Read filenames beginning with `-` | Not explicit | Keep | Approved: following arguments are operands, including dash-prefixed filenames; lone `-` still denotes stdin |
| Options may occur among operands (C) | Accept flexible invocation order | Not explicit | Undecided | Verify cat/parser behavior if retained |
| `POSIXLY_CORRECT` changes option ordering (C) | Use GNU/POSIX compatibility conventions | Not explicit | Undecided | Environment-dependent parsing adds scope |
| Unambiguous long-option abbreviations (C, qualified as most programs) | Shorten interactive invocations | Not explicit | Undecided | Verify cat behavior; full names may suffice |
| Zero status for success; nonzero for failure (I) | Human or AI detects failure | S30–33 do not specify statuses | Keep | Approved project statuses: 0 success, 1 I/O failure, 2 argument error |

## Course additions and contract gaps

| Feature / reference | Proposed human or AI task | Course requirement | Decision | Reason / question |
| --- | --- | --- | --- | --- |
| `-h` help (S8–10, S32); absent from GNU cat M | Discover command usage | Required | Keep | Approved synonym for --help; deliberate addition to GNU cat |
| `--json` with path/content fields (S33) | AI associates returned content with its source path | Explicitly optional for cat | Undecided | S11–12 generally recommends JSON for agents; define multi-file, binary, and error representation if kept |
| Shell expands globs; command does not reinterpret them as regex (S6–7) | Read a shell-selected group of files | Shared course rule | Undecided | Separate shell responsibility from cat behavior |
| Built-in where feasible (S16) | Agent calls internal command APIs | Course preference | Undecided | Confirm integration separately from CLI features |
| Diagnostics on stderr; input/output failure policy (project proposal) | Distinguish file data from failures; recognize partial results | Not detailed in S30–33 | Keep | Approved: report input failures and continue, ultimately returning 1; stop immediately on output failure |
| Resource cleanup, short I/O, interruptions, and signals (project proposal) | Reliably transfer content | Implementation obligations once copying is kept | Undecided | Agree ownership, recovery, close-error, and broken-pipe policies |
| Help/invalid-option precedence (project contract) | Predict invocation behavior | Not settled by the reviewed excerpts | Keep | Approved: argument errors take precedence over help |
| Combined short flags (contract gap) | Predict invocation behavior | Not settled by the reviewed excerpts | Undecided | Verify argtable3 behavior before extending the contract |

## Approval and next step

Approved: multiple-file concatenation in argument order and optional `-n` /
`--number`, starting at 1 and continuing across files, including empty lines.
Default output has no added numbering, separators, or labels.

Additional decisions explicitly approved on 2026-10-06:

| Condition | Approved behavior |
| --- | --- |
| No file operands | Read stdin |
| `-` operand | Read stdin at that position in the operand sequence |
| `-h` / `--help` | Print help and exit without reading files or stdin |
| Help plus an invalid option | Report the argument error; return 2 |
| Exit statuses | 0 success; 1 I/O failure; 2 argument error |
| Input file fails | Report the failure, continue with remaining files, and ultimately return 1 |
| Output fails | Stop immediately; return 1 for a detected I/O failure |
| `--` | End option parsing; subsequent arguments are filenames, including those beginning with `-` |

A lone `-` remains the stdin operand after `--`. These decisions unblock the
previously requested parser and copying implementation; they do not approve
other optional inventory rows. Signal policy remains a separate open detail.

## Development plan

The user authorized AI implementation after the reference-first tests. Work
proceeds in small TDD slices. Undecided behavior stays undecided.

1. Finalize scope: retain multiple files and `-n`; settle help, stdin, `--`,
   deferred options, parsing rules, and error behavior. Approve the contract,
   module interfaces, editable files, execution scope, and test matrix.
2. Parser and help: integrate the installed `build/argtable3` dependency;
   reuse `src/cmd_spec.h` and one argument definition for parsing and help.
   Test help and invalid arguments before implementing them.
3. File copying: test one file, empty and binary files, buffer boundaries,
   and preservation of every byte, including missing final newlines.
4. Multiple files: test operand order, empty files between inputs, and file
   boundaries without added separators or newlines.
5. Numbering: test all lines, including empty lines, numbering across files,
   and a line spanning files when an input lacks a final newline. Agree on
   exact number formatting before writing these assertions.
6. Failures: test missing inputs, output failures, partial transfers,
   interruptions, and cleanup against the agreed continuation/status policy.
7. Integration and verification: check command-specification and standalone
   behavior, repeated calls, regression tests, lint, and formatting. No registry
   currently exists in project sources; decide its scope separately and do not
   claim shell integration until it is implemented and tested.

For every implementation step: observe the intended failing test, implement
minimally, rerun the relevant accumulated tests, then refactor while green.
A missing executable or broken harness is not the intended behavioral failure.

### Verification evidence

- Map each approved requirement to a named test before its implementation.
  Assert stdout bytes, stderr expectations, exit status, and side effects.
- Use isolated real-file/pipe fixtures for CLI cases and controlled fault
  injection for short I/O and interruptions; agree on the test seam first.
- Check input files remain unchanged, borrowed descriptors stay open, owned
  resources are released, and repeated invocations do not retain state.
- Compare selected compatible outputs with installed GNU cat 9.4. The approved
  contract governs deliberate differences such as `-h` and chosen error codes.
- Before delivery, run `make test`, `make lint`, `make format-check`, and `make`.
  These targets now include cat. Record exact commands, results, failures, and
  skipped checks; reference tests remain a separate `make test-cat` target.
- Agree on property tests, bounded fuzzing, and sanitizer/resource checks or
  their omissions before completing the test matrix. A skipped check is not
  a pass; separate baseline failures from failures introduced by cat.

## Reference-first test stage

The user requested explicit expected-versus-got tests against GNU cat first,
then the same expectations against our implementation. This characterizes the
selected GNU-compatible behavior; it does not implement cat or settle the
remaining option inventory. The reusable procedure is in
[Adding a command](../ADDING_COMMANDS.md#reference-first-tests-provisional-procedure).
Refine the blueprint after implementing two or three commands.

Runner: `tests/test_cat.c`; reference: `/usr/bin/cat`, GNU coreutils 9.4.
The original file cases assert exact stdout bytes, status 0, empty stderr, and
unchanged input contents. Later cases cover stdin and expected failures with
explicit status/diagnostic expectations. Temporary fixtures include filenames
with spaces. CLI runs use `LC_ALL=C` and a five-second child deadline. Display
shows the first 100 escaped bytes; comparison covers complete output.

| Named test | Expected behavior |
| --- | --- |
| single file preserves bytes | `alpha` newline `beta`, no final newline |
| multiple files preserve operand order | `second` then `first`, with an empty input between |
| file boundaries add no separator | `left` and `right` become one line |
| sixteen files preserve operand order | No artificial two/three-file limit |
| empty files produce no output | Zero output bytes, both plain and with `-n` |
| binary bytes survive concatenation | Preserve NUL, high-bit bytes, CR, and LF |
| -n numbers every line across files | Number text and empty lines continuously, skipping empty files |
| --number preserves missing final newline | Long option numbers the last unterminated line without adding a newline |
| -n keeps a line spanning files intact | One number for a line split across two operands |
| large file is copied completely | Preserve all 65,537 bytes, beyond common buffer sizes |

The numbering expectations characterize GNU's decimal field of at least six
columns followed by a tab. No filename labels or resets occur between files.

Run the reference:

```sh
make test-cat
```

Test our implementation, including project-specific CLI and module cases:

```sh
make test-cat-impl
```

`CAT_UNDER_TEST` takes one executable path, not a shell command or extra flags.
The runner prints that path. To use the CLI runner directly, run
`make test-cat CAT_UNDER_TEST=./build/cat CAT_TEST_ARGS=--project` after building
cat. `make test` builds and runs project cat tests and existing regression tests;
it never substitutes GNU cat. A reference pass is not implementation GREEN.

### Historical reference-stage verification (2026-10-06)

| Command | Observed result |
| --- | --- |
| `make test-cat` | All 9 cases passed against GNU cat 9.4 (10 invocations) |
| `./build/test_cat /usr/bin/true` | Expected exit 1 from the runner: 8 cases failed on missing output; empty-file case passed |
| `make test` | All 6 existing project tests passed; these do not test our future cat |
| `make lint` | Passed, including the new test source |
| `clang-format --dry-run --Werror tests/test_cat.c` | Passed |
| `make format-check` | Failed on existing formatting in unchanged source/header/test files |
| `make` | Passed; existing main target already up to date |

At this historical stage, fixtures covered at most three operands and there was
no project cat executable. The later sixteen-file case and implementation below
supersede those limits.

## Implementation report

Implemented the approved core in `src/cmd_cat.c`/`.h`, with a thin standalone
`src/cat_main.c`. Reuses the existing `src/cmd_spec.h`. Argtable3 definitions
drive parsing and help; file capacity follows argc instead of a fixed limit.
The caller's argv pointer order is preserved. Numbering state is invocation-local
and continues through buffers and file boundaries.

Reads/writes retry EINTR and complete partial transfers. Only owned input
descriptors are closed. Close errors are reported without retry on Linux.
Same regular input/output files (including aliases) are rejected to prevent
corruption or endless appending. Signal dispositions are inherited, not changed;
default SIGPIPE can terminate the standalone command on a broken pipe.

TDD evidence: runnable baseline failed all nine original cases; plain copying
then passed while numbering assertions failed; numbering made those pass.
The new same-file guard first failed its regression test, then passed after
the guard was implemented. Existing expected file output was not weakened.

| Check | Result |
| --- | --- |
| `make test-cat-impl` | 15 CLI cases plus 4 module cases; see latest run |
| `make test` | 25 cases passed: 19 cat plus 6 existing regressions |
| `make test-cat` | 13 GNU-compatible cases passed against GNU cat 9.4 |
| `make lint` | Passed |
| `clang-format --dry-run --Werror src/cmd_cat.c src/cmd_cat.h src/cat_main.c tests/test_cat.c tests/test_cat_module.c` | Passed |
| `make format-check` | Existing violations in unchanged project files; new cat files pass |
| `make` | Passed; builds main and cat |

Module tests cover repeated success/error/help calls, preserved argv order,
borrowed descriptors, EINTR/short transfers, zero-progress writes, and same-file
rejection. CLI tests also cover stdin, help/error precedence, input-error
continuation, `--`, and stopping after a `/dev/full` output failure.

Known limits: no shell registry exists yet, so registration/dispatch is not
claimed. Argtable3 uses global parser state; concurrent invocations are not
supported or tested. Its allocator aborts on exhaustion; this dependency behavior
prevents a guarantee of recoverable allocation failure. No allocation/close-error
fault injection, sanitizer, or fuzzing run has been claimed. Other inventory
options are outside this implemented slice; JSON and display transformations
are not implemented. No unrelated formatting fixes were made.

Manual checks from the project root:

```sh
make build/cat
./build/cat --help
./build/cat README.md Makefile
./build/cat -n README.md
printf 'one\n\ntwo\n' | ./build/cat -n
./build/cat /does-not-exist README.md
echo "$?"
```

The last invocation prints a diagnostic, still prints README.md, and returns 1.
Dependencies are local at `build/argtable3` and `build/cunit`, both Git-ignored;
override Makefile dependency flags for another installation.
