# clear/cd combined independent review

## Verdict

**Runtime/source review passes; final delivery remains conditional.** No new
blocking implementation, integration or security defect found. Actual main has
10 builtins, 9 standalone wrappers and deliberately no standalone cd. The
remaining gates are parent acknowledgement of documented dependency/usage limits,
historical-link disposition, and verification of the exact approved index tree.
Nothing was staged or committed; no cp work started.

Review-only scope overrides automatic fix/stage/commit suggestions in loaded
command-development and requesting-code-review skills. Project session,
add-command, anatomy/API, coding, design, TDD, verification and logging procedures
were read. Session `20261008_212719_5953ea`, title “Subagent: Independently review
combined actual main…”, was verified against this conversation. Registration is
deferred to the parent because shared-file writes are excluded.

## Source and acceptance assessment

- Registry stores 10 unique module specifications, repeated initialization is
  idempotent, and clear/cd identity is asserted. Capacity is 32: the extra probe
  leaves 21 successful registrations; the next 3 fail and final count stays 32.
  Python separates builtin inventory from 9-name standalone parity, retaining
  original coverage. Cd help is checked through direct main/callback aliases.
- Makefile links clear/cd into main and the shell module harness, includes their
  targets in aggregate tests/clean, and builds only clear as a standalone. Neither
  main nor shell needs a new command-specific branch.
- Clear parses before effects; valid nonterminal stdout is silent, closed stdout
  fails, and terminal output is exactly `\x1b[H\x1b[2J` (7 bytes). Unset/empty/dumb
  TERM fails only on terminals; other values assume ANSI support. Help does not
  clear and ignores TERM. Invalid syntax wins over help. Output-fault tests cover
  short writes, EINTR, zero/error/partial output and recovery; no user terminal
  was cleared.
- Cd performs one kernel chdir, persists across the real main loop, physically
  resolves symlink/`..`, and defaults to HOME. Missing/empty HOME, invalid syntax,
  empty operands and failed chdir retain cwd. Literal dash HOME works; explicit
  `cd -` is unsupported. PWD/OLDPWD/HOME/CDPATH remain unchanged; CDPATH is ignored.
  Module tests cover borrowed argv, exported getopt sentinels, descriptors,
  unconsumed stdin, allocation cleanup and 400 repeated calls.
- Parser tables and copied argv pointer vectors are invocation-owned and freed
  on returning paths. Source inspection found no subprocess execution, injected
  shell strings, embedded credentials, exfiltration or unrelated source edits.
  Prior clear and fixed-cd reviewed code hashes are compared in owned audit
  evidence; current source/test/build and mixed README/FILES bytes are pinned
  before/after fresh executions, not inferred from old binaries.

## Fresh execution

Run from the repository root:

```sh
python3 logs/commands/clear-cd-review/run.py
```

Exact argv/cwd/selected environment/status and source hashes are in
`logs/commands/clear-cd-review/manifest.json`; complete raw output is in
`checks.stdout` and `checks.stderr`, separated by named gate headers.

| Gate | Actual result |
| --- | --- |
| Actual `make -B test` | 0; complete regression recipes, including 21 shell, 8 clear and 7 cd Python tests and all three module harnesses. |
| Actual `make -B all` | 0; rebuilt main and all 9 standalone tools. |
| Actual `make lint` | 0; all 38 C translation units, one suppressed non-user-code warning. |
| Scoped clang-format dry-run | 0; registry, shell harness and all clear/cd C/header/wrapper/module files. |
| Global `make format-check` | 2; only the six inherited files below. |
| Fresh strict `-Werror -O2` focused build/tests | 0; shell/clear/cd suites and module harnesses. |
| Fresh ASan/UBSan `-Werror -O0` focused build/tests | 0; same suites/harnesses, leak detection and halt-on-error enabled; no sanitizer finding. |
| Actual, strict O2 and sanitized O0 PTY/cwd edge gates | All 0; concurrently drained child-owned stdin/stdout PTY combines pwd/clear/cd with exact clear bytes, physical symlink/parent output and eight prompts. Regular-file stdout silent for five TERM states; physical cwd beyond 4096 bytes and absolute recovery pass. |
| Actual internal argtable OOM probe | 1 / `Out of memory!`; dependency termination observed, not recoverability. |
| Real index read-only check | Empty, status 0. |
| HEAD plus exact reviewed allowlist runtime tree | Fresh `make -B test`, 0; README/FILES patch separately passes `git apply --check` in scratch. No real index write. |

Permission failures ran under nonroot euid 1000, not skipped. Every cwd-changing
execution was child-owned; parent-agent cwd remains the repository root. Fresh
strict/sanitized builds used new source copies with installed argtable3/CUnit
symlinks, not pre-existing executables; dependencies were not sanitizer-rebuilt.

Inherited format failures: `src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`,
`tests/test_help.c`, `tests/test_output.h`, `tests/test_recursive.c`; all equal
HEAD bytes. They are not new regressions and were not fixed in this review.
No fresh O1 sanitizer-harness pass is claimed; the previously reported optimized
fault-injection limitation remains. Full regression was rerun at actual default
O0; sanitizer rerun is proportionately focused on integrated shell/clear/cd.
No exhaustive fuzzing, coverage, 32-bit, concurrent parsing, private interrupted
getopt-state, arbitrary terminal compatibility or all-library-OOM guarantee.

## Exact delivery audit and scratch corrections

Original proposal: `clear-cd-index-proposal-9r5nfy4z` under the configured scratch
root. Its 25 core and 88 evidence entries are unique, present, text-only and
narrowly related to clear/cd integration. No data/wiki/demo/session/AI-log move,
unrelated touch evidence, source snapshots, compiled binaries, caches or private
credential material was identified. Evidence totals and byte hashes are recorded
in `logs/commands/clear-cd-review/audit.json`; automated scans are not a guarantee
against every possible private datum. Existing machine paths/session IDs are
provenance, not secret values; parent should accept their publication explicitly.

The 88 evidence files are bounded raw execution triplets plus reproduction and
manifest scripts, not a generated build dump. Empty streams intentionally retain
stdout/stderr/status provenance. Historical reviewer raw archives remain excluded;
only the explicitly linked original cd evidence aggregate is included.

`logs/commands/clear-cd-review/audit.py` generates the final explicit newline path
allowlist and scratch-only HEAD-based README/FILES versions/patch. It preserves
HEAD's original eight standalone-help examples rather than rewriting them,
changes the count and nine-wrapper note, adds clear/cd rows and combined review
links, and adds only relevant FILES entries. Unrelated working README demo
relocation and broad FILES data/wiki/log reorganization remain untouched.
No working README/FILES/shared document was edited. Live clear/cd guides still
say combined review is pending; scratch `live-status.patch` narrowly corrects
those two status lines and links this review. The rehearsal uses those proposed
versions; the parent must approve/apply them only to the intended index versions.
The older integration report remains an execution-time snapshot, not a rewritten
claim of final delivery.

**Historical-link disposition:** the only excluded Markdown dependencies are
clear's historical report links to `docs/COMMAND_ANATOMY.md` and
`docs/COMMAND_API.md`. Preserve the report bytes as a snapshot under the existing
HEAD `docs/reports/README.md` notice (which expressly permits obsolete links),
not as live setup instructions. Do not import their unrelated data/course
transitive closure. A scratch-only optional patch converts just those two links
to explicit historical path labels if the parent instead requires no unresolved
Markdown links; applying it requires parent approval and refresh of snapshot
claims. Current command guides, integration/review links and evidence links must
resolve in the proposed checkout. Historical pending/scratch-only verdicts do
not describe current availability; this report and current guides do.

Final explicit allowlist: `logs/commands/clear-cd-review/final-allowlist.txt`
contains **121 unique paths** (25 original core, 88 original evidence, this
report and 7 owned evidence files). Scratch proposal versions and patches are
located by the `proposal` field in `audit.json`; they are not repository edits.
The report and owned evidence are included explicitly. Scratch
proposal verification is a pre-index rehearsal, not a claim about a staged tree.
After parent corrections/approval, extract the exact index, run its applicable
build/tests/link checks and verify its tree hash before committing.

## Required parent dispositions

1. Accept real argtable internal-OOM termination and void usage callback limits;
   ordinary constructor-NULL/module-copy seams do not prove all-OOM recovery.
   Exported getopt preservation is not private-state or concurrency isolation.
2. Accept the historical snapshot-link disposition or approve the supplied narrow
   link patch; do not absorb unapproved course/data files.
3. Use the explicit final allowlist and HEAD-based mixed-document patch, never
   blanket add or working README/FILES copies. Verify the final index after any
   change. Global formatting and O1 harness limitations remain disclosed.

Created only this report and `logs/commands/clear-cd-review/`; scratch corrections
are outside the repository. No source/shared-doc/session/AI_LOG/wiki/index edit,
commit, push or cp action occurred.
