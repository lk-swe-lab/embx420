# clear implementation and verification report

## Delivery state and ownership

Ready for independent review of the clear-owned module, wrapper, tests and
command guide. **Not delivered as a working-tree shell builtin yet:** the
coordinator must integrate the registry/build changes and rerun the final shared
checks. No staging, commit, AI log, wiki, README/FILES edit, shared-source edit, or
user-terminal clearing was performed by this resumed worker.

Owned files:

- `src/cmd_clear.c`, `src/cmd_clear.h`, `src/clear_main.c`
- `tests/test_clear.py`, `tests/test_clear_module.c`
- `docs/commands/clear.md`, this report

The wrapper already existed as `src/clear_main.c`; there was no root-level
`clear_main.c`. The implementation and Python tests existed; the module test and
both clear documents were missing. The prior worker's saved transcript is
`/home/name/.hermes/cache/delegation/live/deleg_5f878eef/task-0.log`.

Current assignment session was verified as `20261008_203032_03710e`, title
“Subagent: Resume and finish clear-owned…”. `docs/sessions.txt` was intentionally
not edited under this worker's clear-only ownership restriction; registration
remains for the coordinator if required.

## Selected contract and reference inventory

The delegated contract, not ncurses compatibility, controls this implementation.
The upstream [ncurses clear(1) manual](https://invisible-island.net/ncurses/man/clear.1.html)
was retrieved through the browser after the extraction backend rejected URL
extraction. Its footer identifies ncurses 6.6, manual revision 2025-04-05.
No reference executable was substituted for the implementation.

| Reference feature | Selection / reason |
| --- | --- |
| Clear screen; normally also clear supported scrollback | Keep visible screen only; preserve scrollback. |
| Read TERM and consult terminfo | Read TERM only on terminal stdout; assume ANSI compatibility rather than add a capability dependency. |
| `-T type` overrides terminal selection; also ignores LINES/COLUMNS | Defer: terminal override/capability lookup outside selected minimum. |
| `-V` reports ncurses version and succeeds | Defer: there is no ncurses dependency to report. |
| `-x` suppresses attempted scrollback clearing | Defer option: retaining scrollback is unconditional here. |
| No operands in documented syntax | Keep; reject operands instead of silently ignoring them. |
| General stderr/exit-status policy | Manual does not enumerate it; project defines 0/1/2 explicitly. |
| `-h`/`--help`, malformed module argv guards | Project additions using shared argtable3 anatomy. |

Essential behavior is exactly seven bytes `\x1b[H\x1b[2J`, with no newline or
scrollback sequence. Valid nonterminal stdout produces nothing and succeeds,
regardless of TERM. Closed stdout fails. Terminal stdout requires TERM to be
present, nonempty and not exactly `dumb`; all other names, including an unknown
name, are assumed ANSI-compatible. No terminal-capability validation, ncurses,
terminfo, subprocess or JSON functionality is implemented.

`-h`/`--help` and standalone `--` are accepted. Repeated help, unknown options,
option values and all operands fail with status 2 before escape output. Parser
errors take precedence over help. Help does not depend on TERM or terminal
classification; help output failure returns 1. Success/help is 0, operational
failure is 1, argument error is 2; diagnostics use stderr.

The user delegation approves this minimum; no additional course-required flags
are asserted. Project procedures: [anatomy](../COMMAND_ANATOMY.md),
[API and primary POSIX/argtable references](../COMMAND_API.md),
[coding](../CODING.md), [TDD](../TDD.md), [verification](../VERIFY.md).

## Implementation and ownership model

One public `cmd_clear_spec` uses the existing shared `cmd_spec_t`. `clear_run`
and the usage callback construct fresh argtable help/end entries and free every
constructed entry. Help and parsing share those definitions. The standalone
wrapper only calls the specification's run callback. Registry integration follows
the repository's existing specification-array pattern, not a new registry API.

Before parsing, validate argc, argv and every argv element. Copy the pointer
vector with an explicit trailing NULL so argtable reordering cannot modify the
caller's array; strings remain borrowed. Save/restore exported getopt variables
`optarg`, `optind`, `opterr`, `optopt` around parsing. This preserves the tested
public values, **not** an interrupted/nested getopt scan's private internal state;
thread-safe/concurrent parsing is not claimed.

Borrow all inherited descriptors; never close/read stdin or alter descriptor
flags, cwd, TERM, or signal dispositions. Check stdout validity with `fcntl`
(retry EINTR), then `isatty`, then TERM if needed. Use unchanged shared
`lines_write` for short-write/EINTR recovery and zero-write failure. An output
error may leave a partial escape prefix; it is diagnosed, not rolled back.
Help flush failure is diagnosed and clears stdout's error indicator so a later
invocation on a repaired borrowed stdout can succeed. Errno is not preserved.

## Recovered TDD evidence, then resumed work

Raw evidence is under `/home/name/.hermes/cache/scratch/clear-worker/` (abbreviated
`$E` below). These files are transient local evidence, not durable repository
logs; retain them for review before cache cleanup.

| Slice | Actual RED observation | Actual GREEN evidence |
| --- | --- | --- |
| Prior 1: help | `slice1-red.log`: help expected Usage, received empty stdout. | `slice1-green.log`: 1 test passed. |
| Prior 2: terminal bytes | `slice2-red.log`: terminal expected seven bytes, received nothing. | `slice2-green.log`: 4 tests passed. Nonterminal/invalid cases already passed; not separate fabricated REDs. |
| Prior 3: TERM and closed stdout | `slice3-red.log`: closed stdout succeeded; unusable TERM cleared/succeeded. | `slice3-green.log`: all 6 then-existing tests passed. |
| Resume 4: borrowed getopt state | Newly added module test aborted (134); `slice4-red.log` records optind changed to 1, opterr to 0 and optarg not preserved. | Save/restore around real argtable parse; `slice4-green.log`, final `module.stdout`, and sanitizer module run pass. |
| Scratch integration | `integration-red.stderr`: real baseline main built successfully, then returned 127/unknown command instead of clear statuses/output; discovery omitted clear. Test exit 1. | Add clear include/spec only to scratch registry and link module; `cli-integration-green.stderr`: 8 tests passed. |

The resumed module fault tests characterized already-working short-write/EINTR,
zero/error/partial-error, constructor-NULL and argv-copy allocation paths. They
passed without production changes; no new implementation RED is claimed for
those paths. Help output/recovery and public usage parity likewise characterize
existing behavior. Only the observed getopt-state defect needed a new module fix.

A separate **negative control**, not an implementation TDD slice, compiled a
scratch mutant changing erase-screen parameter 2 to 3. `byte-negative.stderr`
shows the intended exact-byte assertion failure (status 1); production remains
unchanged and green. `byte-negative.json` records its compile/test commands.

## Acceptance matrix

| Requirement | Executed check |
| --- | --- |
| Exact bytes, no newline/scrollback, standalone `--` | `test_terminal_bytes`, child-owned PTYs. |
| Pipe/regular-file silence independent of TERM | `test_nonterminal`; module tmpfile capture. |
| Terminal TERM unset/empty/dumb failure; unknown-name assumption | `test_term_guard`, exact stdout/status, stderr present. |
| Closed stdout failure | `test_closed_stdout`; main parity includes closed stdout. |
| No effects on malformed options/operands/help combinations | `test_invalid_before_effects`, terminal stdout remains empty, diagnostic has no ESC. |
| Help before terminal effects; both aliases, all TERM states | `test_help`; PTY help differs only by expected CRLF translation. |
| Help output fault | `test_help_output_failure`: `/dev/full` and closed stdout. Module repairs stdout and repeats help successfully. |
| Invalid API inputs | Module argc 0/-1, NULL argv, NULL argv[0]/later element. |
| Borrowed argv and host state | Module checks pointers/strings including parser-reordered mixed input; stdin position; all three descriptor flags/status flags; cwd, TERM, SIGPIPE handler/flags; public getopt values. |
| Invocation reuse, error cleanup, same help grammar | Module success/error/help sequences, constructor/copy failure then recovery, callback help byte-equal to run help. |
| Robust output | Wrapped real shared write loop: two-byte transfers, initial EINTR, zero write, ENOSPC, error after two bytes, recovery. |
| Registry discovery/help/dispatch and repeat loop | `test_main_parity`: eight direct parity cases, listing, `help clear`, pipe loop, exact-byte PTY loop with success/error/help/success. |

No user terminal received ANSI output: every clear invocation either used
nonterminal capture or a subprocess-owned PTY. PTY waits/processes are bounded.

## Executed final checks and reproduction

From `/home/name/Documents/420-EMBDX/A`:

```sh
python3 /home/name/.hermes/cache/scratch/clear-worker/verify.py
python3 /home/name/.hermes/cache/scratch/clear-worker/handoff.py
python3 /home/name/.hermes/cache/scratch/clear-worker/check_make.py
```

- C17 builds with all project diagnostic flags plus `-Werror`: clear, module test,
  baseline main and scratch-integrated main all status 0.
- CLI plus scratch-main suite: **8 tests passed**, no skips; module test status 0.
- Scoped `clang-format --dry-run --Werror` on all clear C/header/test files:
  status 0. Formatting was applied only to those owned C/header files.
- Scoped clang-tidy using the project's analyzer check selection and
  `--warnings-as-errors=*`: three C translation units, status 0, no diagnostics.
- ASan + UBSan + leak detection: standalone, module and scratch main compiled
  with `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`.
  The same 8 CLI/main tests and module checks passed with halt-on-error enabled;
  no sanitizer diagnostics. The existing static argtable library itself was not
  rebuilt with instrumentation.
- The scratch registry also passed a dry-run formatting check with the explicit
  repository style. Initial scratch-only attempts lacked that configuration,
  then exposed the new array layout; these were corrected without touching the
  working registry.
- A separate scratch checkout using the proposed Makefile and registry ran
  `make -B test-clear`: rebuilt clear/module/main, 8 tests passed, module passed,
  status 0 (`scratch-make.stdout`, `.stderr`, `.json`).

`verification.json` contains exact argv, actual status and expected status for
every verification gate; each label has raw `.stdout`/`.stderr`. Initial-slice
logs retain the prior worker's original RED/GREEN evidence. `clear-code-sha256.json`
identifies verified clear code/tests; `snapshot-sha256.json` identifies the
copied pre-integration source snapshot. The scratch snapshot, not the concurrently
changing working tree, establishes main parity.

## Integration handoff and explicit limits

`$E/registry-clear.patch` adds only the clear header/spec to the registry array.
`$E/Makefile-clear.patch` adds the shell source/header, standalone/module build
rules, all/test/clean wiring, and a `test-clear` target that requires and exercises
main parity. Both changes are **scratch-only**, and the proposed Makefile was
actually exercised in `$E/delivery-tree`. They are review artifacts, not permission
to overwrite current shared files. Rebase them onto the coordinator's final
registry/Makefile state and maintain C line/format conventions.

Remaining gates: independent review; working registry/Makefile integration;
README/FILES work by their owner; final full shared regression/lint/format after
integration. No full repository check is claimed by this scoped worker.

Allocation injection covers NULL parser constructors and the module-owned argv
copy, not arbitrary argtable internal allocation/panic paths. Exhaustive malloc
failure, allocation accounting, coverage, property fuzzing, nested getopt and
concurrent parsing were not performed. The fixed-size output has deterministic
byte/fault tests rather than a claimed fuzz campaign. TERM handling is an ANSI
assumption, not a claim of compatibility with every terminal emulator. Diagnostic
text is checked for presence/absence of escapes except where main/standalone
parity compares exact bytes.
