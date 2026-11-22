# Completion plan

Implementation scope: this directory only. No main-shell ABI integration.
The parent agent handles scoped commits and publication under user approval.

- [x] Runtime: concurrent threaded/process pipelines, descriptors, redirection.
- [x] Grammar and state: wider words, variables/export, conditionals, cd/exit.
- [x] Background ownership/reaping and bounded backquote evaluation.
- [x] Error/limit tests, clean build, demo, documentation and work log.
- [x] Mermaid guide matched to Grammar.cf and generated AST.

## Evidence

Initial RED: `python3 -m unittest -v test_runtime`: six tests, four
failures. Missing pipelines, redirection/sequences, and stdin ownership
failed as expected. Missing-command and failed-open safety already passed.

Final verification, run from this directory:

- `make clean && make test`: 73 tests, OK; warning-free clean build.
- `make demo`: checked outputs and AST; temporary fixtures removed.
- `make sanitize`: 40 tests, OK under ASan/LeakSanitizer/UBSan.
- `./demo.py`: executable demo passed.
- `git diff --check -- .`: exit status 0.

The sanitizer probe found and the adapter fixed BNFC syntax-error AST leaks.
The regression suite also found and fixed assignment-before-exit status.
See AI_WORK_LOG.md for real cycle evidence and README.md for explicit limits.
Mermaid rendering was not visually checked; source/category/constructor
coverage is checked by test_build.py.

Independent review found a FIFO redirection preparation deadlock. The parent
reproduced it with four timed-out regression cases. Redirection opens now use
nonblocking mode during type inspection, reject FIFOs (including symlinks),
and restore blocking mode for supported targets. The new regression passes.
README.md documents this deliberate subset limit. System pipeline pipes are
unchanged. Parent verification reran the full suite, sanitizers, and demo.

Guided checkpoint: feat/bnf-shell-checkpoint, commit a79b657 (32 tests).
Completed-result branch: feat/bnf-shell-complete.

## Optional `@` prototype

- [x] Separate nl_input.c/.h helper capture and confirmation module.
- [x] Fixed-response mysh_llm_stub.py; no model or catalog integration.
- [x] Minimal pre-parser hook; unchanged Grammar.cf and GRAMMAR.md.
- [x] Exact-argv, fail-closed, byte/line/timeout, and lifecycle tests.
- [x] README and NL_INPUT.md protocol, trust boundary, and limits.

Observed final verification for this increment:

- `make clean && make test`: 95 tests, OK; no compiler warnings.
- `make sanitize`: 62 tests, OK under ASan/LeakSanitizer/UBSan.
- `make demo`: passed; temporary fixtures removed.
- `git diff --check -- .`: exit status 0.

No commit, push, branch switch, or write outside this sidequest was made.
