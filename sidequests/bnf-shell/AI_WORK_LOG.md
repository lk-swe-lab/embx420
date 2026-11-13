# AI work log — autonomous sidequest completion

## Authority and scope

The delegated instruction and the direct user request authorized autonomous
implementation, real tests, fixes, documentation, and a runnable demo. The
allowed implementation write scope was this sidequest directory only. The parent
agent prohibited the implementation subagent from making commits, pushes,
branch changes, parent/main-shell edits, or accessing secrets. Git publication
remained with the parent agent under the user's separate authorization.
A later steering message requested a Mermaid learning guide for the
final actual grammar. This log summarizes decisions and observed results; it
is not an export or reconstruction of complete prompts.

## Key user prompts and authorized changes

These are short excerpts from this session, not reconstructed prompts.
Earlier increments were reviewed through user-reported test results. The final
autonomous increment is verified by tools; it has not yet had human review.

1. "can you create three tests?" — Added command-argument, empty-argument, and
   invalid-pipe parser checks before extending the grammar.
2. "let's follow 06" and "rewrite" — Selected internal command threads and
   external program processes; added an internal echo dispatch check.
3. "why execute_echo if we want a generic executor?" followed by "implement"
   — Separated command behavior, thread execution, external execution, and
   dispatch. The user subsequently reported eight passing tests.
4. "we want to keep working on grammar" — Prioritized recursive pipelines,
   per-command redirection, jobs, variable nodes, and backquote parsing.
5. "can you create a branch from current work and work fully autonomously?"
   — Preserved the 32-test guided checkpoint and selected a separate completion
   branch. The user clarified that they wanted to inspect the full result
   before returning to step-by-step work.
6. "push current branch to repository" — The parent pushed the saved checkpoint
   on the completion branch; the implementation child did not publish files.
7. "once done, please create a visualization mermaid of the grammar so I can
   learn?" — Added GRAMMAR.md with category/constructor diagrams and an AST.

## Actual test/implementation cycles

1. Added runtime tests for mixed pipelines, redirection/sequences, last-stage
   status, and external stdin ownership. The initial six-test run had four
   failures for missing execution behavior. Implemented owned stage plans,
   explicit worker descriptors, concurrent starts, child-only spawn file
   actions, prelaunch file opens, exact PID waits, and byte-wise line input.
   The original 32 tests and the six runtime tests passed.
2. Added tests for wider tokens, large pipe output, variables/export,
   conditionals, cd/exit, and registered hello dispatch. The 13-test runtime
   run had six failures. Extended Grammar.cf and added main-thread shell state
   plus a tiny internal registry. `make test` passed 45 tests.
3. Added backquote capture and background lifetime/nonblocking tests. The
   20-test runtime run had five failures. Implemented the inner-AST evaluator,
   bounded read-before-wait capture, owned background plans, null stdin,
   completion polling, and shutdown joins/reaping. `make test` passed 52 tests.
4. Added malformed grammar, file/pipe override, SIGPIPE, exact capture limits,
   resource limits, EOF, and `/proc` child-reaping tests. All 66 tests passed.
5. A real ASan probe of `echo partial |` found a generated-parser leak:
   101 bytes in six allocations. Added `prepare_parser.py` to install typed
   Bison destructors and a private untyped ownership-transfer start rule.
   Public parsing still uses Input only. A new assignment-before-exit test
   also failed (status 1 instead of 0); corrected the inline assignment's
   preceding-command status. The accumulated suite passed 68 tests, and the
   ASan/LeakSanitizer/UBSan driver/runtime suite passed 39 tests.
6. Added README.md, executable demo.py, GRAMMAR.md with three Mermaid diagrams
   and an actual generated AST, and build-contract/diagram-name checks.
   The final clean build and accumulated suite passed 72 tests. The demo and
   the 39-test sanitizer run passed. Direct `./demo.py` also passed.

7. Independent review found that FIFO redirection could block before the
   matching pipeline/background reader started. The parent added one regression
   with four cases; all four first failed with bounded timeouts. Redirection now
   opens in nonblocking mode, rejects FIFOs using the opened descriptor's type,
   and restores blocking mode for supported targets. The suite passed 73 tests;
   the sanitizer suite passed 40 tests. The demo passed again.

## Final observed commands

Run from this directory:

- `make clean && make test`: 73 tests, OK; no compiler warnings.
- `make demo`: printed checked output and the generated AST; completed with
  `Demo passed. Temporary fixtures were removed.`
- `make sanitize`: 40 driver/runtime tests, OK with ASan, LeakSanitizer, and
  UBSan enabled, including syntax-error cleanup paths.
- `./demo.py`: passed as an executable.
- `git diff --check -- .`: exit status 0.

## Decisions and limits

Runtime status now follows the last command; parse/input errors remain sticky
for checkpoint compatibility. Background completion does not alter shell
status. EOF/exit wait for background work. Reaping occurs at input/command
boundaries, not while idle. Expansions produce one argument and are not reparsed.
The grammar has no general quoting, escaping, nesting, or POSIX-shell claim.
Preparation is not transactional for file truncation or substitution effects.
No main-shell registry integration, full job control, timeout/cancellation,
allocator failure injection, ThreadSanitizer, or interactive terminal suite was
added. Mermaid source consistency is checked; visual browser rendering was not
performed. Full supported syntax and numerical limits are in README.md.
