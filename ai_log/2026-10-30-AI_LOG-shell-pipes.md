# Module shell and pipes — 2026-10-30

Summary of this task. Not a prompt transcript.

## Decisions

- Keep command code in loaded `.so` modules, including built-ins. The host has
  no static command fallback or PATH lookup. Standard modules ship with the
  release; `pkg` installs extra packages. Do not call these the same operation.
- Keep the pipe experiment separate in `sidequests/lean-ai-shell/`.
  Reuse the module runtime. Record final integration steps in `DESIGN.md`.
- Build pipes recursively in the parent. Start all stages before waiting.
  Run `cd`, `exit`, and `pkg` alone in the parent.
- Defer exec, redirection, background jobs, and full job control.
- Use short ASD-STE100 text. Keep the README to commands and guide links.

## Correction and checks

- Class 5 slide 18 uses a loop, not recursion. The early wait on slide 17 can
  block a writer before the next reader starts. Do not copy that order.
- Review found a construction-time interrupt race. Children forked later do not
  inherit pending signals. Keep INT/QUIT blocked, then forward pending signals
  to every child before restoring the parent mask.
- Both interrupt tests failed before the fix. All 23 tests then passed in normal
  and ASan/UBSan builds. Static analysis, changed-C formatting, and independent
  review passed. Main-shell and package regression targets also passed.
- The package demo recorded a passing real-output capture, including live
  install/remove/reselect and release relocation. See `demo/PACKAGING.md`.
- The new pipe-demo test failed against a no-op script, then passed against the
  real script. It ran from another directory with all five-second pauses.
  Output checks, process tracing, cleanup, and unchanged-binary checks passed.
- Follow-up check: `make -C sidequests/lean-ai-shell sanitize test-demo` passed
  23 normal tests, 23 sanitizer tests, and the complete demo test.
- The package runner also passed again: 11 processes, 22 loop inputs, 27 checks.
  Captures went to scratch; existing tracked evidence was not replaced.

## Artifacts and publication

- Module shell: commit `49a8855`. Pipe sidequest: commit `fe62cff`.
  Both were pushed to `lk-swe-lab/embx420`, branch `feat/aishell-so`.
- Package demo: `demo/packaging-demo.py`. Pipe demo: `sidequests/lean-ai-shell/demo.sh`.
- Private wiki drafts explain pipes and foreground execution. Read-back and
  validation passed. Drafts and course data stay out of Git publication.
- This follow-up adds the demo, log, and short README. Its commit is separate.
  No change to shell execution is intended.
