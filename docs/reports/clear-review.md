# clear independent review

## Verdict

**PASS for the selected module/standalone contract; ready for integration.**
No blocking implementation defect or security concern was found in the reviewed
clear source, header, wrapper, tests, or documentation. **Not a delivered shell
builtin:** the reviewed working registry/build snapshot omits clear. Its freshly
built main returned 127 / `main: clear: unknown command`. Registration, build
wiring, current documentation status, and final shared checks remain owner gates.

Only this report and `logs/commands/clear-review/` were created by this reviewer.
No implementation/shared-source edits, staging, commit, wiki, AI_LOG, README, or
FILES changes were performed. All terminal effects were confined to child-owned
PTYs; other command output was captured in files/pipes.

Session lookup verified `20261008_205019_aeabd6`, title
“Subagent: Independently review clear…”. Session registration was deliberately
not written under this review-only file scope; the coordinator owns that decision.
Loaded command-development and requesting-code-review skills; read project
session, anatomy, API, add-command, coding, design, TDD, verification and logging
procedures. Review-only authority overrides those skills' implementation,
auto-fix, staging and commit suggestions.

## Actual independent execution

From `/home/name/Documents/420-EMBDX/A`:

```sh
python3 logs/commands/clear-review/run.py
python3 logs/commands/clear-review/extended.py
```

Both final script executions returned 0. Builds/tests used the isolated copy in
`/home/name/.hermes/cache/scratch/clear-review/tree`, not a changing staged tree.
Installed argtable3/CUnit directories were borrowed through scratch symlinks.

| Gate | Observed result |
| --- | --- |
| C17 strict build, project diagnostic flags plus `-Werror` | Standalone, module test and scratch main compile at `-O0` and `-O2`, all status 0. |
| CLI and actual scratch-main parity | 8 tests, no skips, pass separately at both optimization levels. |
| Module ownership/reuse/fault checks | Status 0 at both optimization levels. |
| ASan + UBSan + leak detection | Standalone/module/main builds and the same 8 CLI/main tests plus module checks pass at `-O0` and `-O2`; no sanitizer findings. |
| Scoped clang-format | Clear source/header/wrapper/module test, dry-run `--Werror`, status 0. |
| Scoped clang-tidy | Three clear C translation units, project analyzer selection, warnings-as-errors, status 0; only processing notices. |
| Proposed Makefile wiring in scratch | `make -B test-clear` with strict flags rebuilds standalone/module/main, passes all 8 tests and module checks, status 0. |
| Independent regular-file stdout probes | Six TERM states, including unset/empty/dumb/unknown, all silent status 0 with stdin closed. |
| Extended sanitized reuse | 1,000 success/error/help cycles followed by the normal ownership/fault suite, status 0, including parser mutation seam. |
| Sensitivity controls | Bypassing argv copy and corrupting restored optind each abort with SIGABRT (-6); diagnostic markers identify the intended invariant failure. Erase parameter 3 instead of 2 fails the exact-byte CLI assertions (status 1). Unchanged code passes. |

The executed CLI cases include seven-byte ANSI output, standalone `--`, help
aliases/TERM independence, PTY CRLF behavior, missing/empty/dumb TERM rejection,
unknown-terminal ANSI assumption, invalid arguments before output, closed stdout,
help on `/dev/full`, discovery, `help clear`, direct dispatch/status parity, and
repeated success/error/help/success shell loops. Fault injection exercises real
`lines_write` short-write/EINTR recovery, zero-write failure, ENOSPC, partial-output
failure, and later recovery. Constructor NULL and argv-copy allocation faults
also recover. All malformed module argv guards were executed.

`results.json` records 27 core command executions and their exact expected/actual
statuses. `extended-results.json` records 10 extended subprocess executions plus
six regular-file observations. Raw `.stdout`/`.stderr` are retained for each
command; negative controls are expected failures, not production/TDD failures.

## Code and ownership assessment

- `cmd_clear_spec` is the single public specification; the wrapper delegates
  directly. Definitions for parsing and help are invocation-owned, shared, and
  freed on success, argument errors, help, and checked allocation failures.
  The existing registry uses a specification array, so no new registration API
  or per-module registration helper is needed for this integration.
- argc/argv and each borrowed argument pointer are validated. A terminated
  pointer-vector copy protects caller argv without copying or modifying strings.
  A reviewer wrapper calls the real parser, then swaps its received pointers.
  Production survives; parsing the original caller array instead fails the
  explicit argv marker. This closes the installed parser's natural-permutation
  sensitivity gap without changing the production tests.
- All four exported getopt variables (`optarg`, `optind`, `opterr`, `optopt`) are
  saved/restored around parsing, including errors/help. Nondefault sentinel
  checks pass. This is public-value preservation only: interrupted/private getopt
  scan state, reentrant parsing and concurrent parsing are not guaranteed.
- stdout validity is checked with EINTR-retrying `fcntl(F_GETFD)` before terminal
  classification; nonterminal stdout remains a deliberate no-op. TERM is checked
  only for terminal effects. Exact output is `\x1b[H\x1b[2J`, with no newline or
  scrollback command. Partial output on operational failure cannot be undone.
- Help uses stdio, checks flush/error state, returns 1 on output failure, and
  clears stdout's error indicator for tested later reuse. Normal clear uses a
  descriptor writer. A module host must flush pending stdio output before direct
  descriptor output; reviewed `shell_dispatch` already does so before and after
  callbacks. The module does not preserve arbitrary inherited FILE-buffer/error
  state, errno, or every possible process-global value.
- stdin is not consumed; borrowed descriptors stay open with unchanged descriptor
  and status flags. Tests also preserve cwd, TERM and SIGPIPE handler/flags.
  There are no subprocess launches, executable path injection, dynamic terminal
  capability lookups, or secret-bearing data paths in the module.
- Diagnostics are best-effort stderr writes. Their own write/flush failures do
  not replace an already nonzero status. SIGPIPE disposition is inherited, not
  overridden; no universal status-1 promise is made for signal termination.
- The void usage callback cannot return an allocation failure: constructor failure
  prints a fallback line to its borrowed stream. Shell `help clear` can detect
  stream failure, not a successful write of that fallback. This is a nonblocking
  limitation of this callback/interface, not tested all-OOM recovery.

## Evidence and limits

Reviewed source hashes match the worker's saved five-code-file manifest, and
remain unchanged after this review. `reviewed-sha256.json` additionally identifies
the reviewed usage/implementation documents; `snapshot-sha256.json` pins the
isolated source tree. The worker's actual getopt RED/GREEN logs were inspected
and retained as `worker-slice4-*.log`; the broader worker evidence and handoff
patches were read. This review independently re-executed delivery behavior and
added sensitivity controls; it does not retroactively certify every historical
TDD slice.

The guide/report correctly distinguish pending integration from scratch parity,
ANSI assumptions from terminfo compatibility, and module-owned allocation faults
from dependency internals. Static argtable3 itself was not sanitizer-rebuilt.
No exhaustive internal-library OOM, allocation accounting, coverage measurement,
property fuzzing, interrupted getopt scan, concurrent parsing, or arbitrary
terminal-emulator compatibility was established. fcntl retry logic was inspected,
not separately fault-injected. Existing Python PTY tests wait for child completion
before draining stdout; this is adequate for the small fixed output/help fixtures,
not a scalable large-output harness. No full repository regression/lint/format or
final staged-index build is claimed by this scoped review.

## Exact integration handoff

The coordinator must rebase these changes onto its final shared tree, not replace
that tree with the scratch snapshot:

1. `src/registry.c`: include `cmd_clear.h` and append `&cmd_clear_spec` once to
   `register_all_builtin_commands`' builtin array; format the resulting array.
2. `Makefile`: add `src/cmd_clear.c` to `SHELL_SOURCES` and `src/cmd_clear.h` to
   `SHELL_HEADERS`; this covers main and the existing shell-module link rule.
3. Add `build/clear` linking `clear_main.c`, `cmd_clear.c`, `cmd_lines.c` and the
   existing argtable3 libraries; declare the corresponding headers/Makefile as
   prerequisites. Add `build/clear` to `all` and clear binaries to `clean`.
4. Add `build/test_clear_module` linking the module/shared helper and
   `-Wl,--wrap=write,--wrap=isatty,--wrap=malloc,--wrap=arg_lit0,--wrap=arg_end`.
5. Add phony `test-clear` depending on standalone, module test and `build/main`;
   run `python3 tests/test_clear.py --program ./build/clear --main ./build/main`
   followed by `./build/test_clear_module`. Make the accumulated `test` target
   depend on `test-clear`.
6. After real integration, the documentation owner updates pending-status text
   and relevant indexes. Run final shared regression/lint/format and validate the
   exact delivered/index tree. Scratch passes alone do not satisfy that gate.

Durable reviewed patch examples: `logs/commands/clear-review/registry.patch` and
`worker-Makefile-clear.patch`. The latter was actually applied and exercised in
scratch. No source fix is required by this review; integration and final shared
verification are the remaining blockers to delivery.
