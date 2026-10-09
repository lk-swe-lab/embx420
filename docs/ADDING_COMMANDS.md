# Adding a command

Reusable procedure for a requested command, not an implemented automation runner.
Inspect the shared [command specification](../src/cmd_spec.h) and
[registry API](../src/registry.h). Use
[design](DESIGN.md), [coding](CODING.md), [TDD](TDD.md), and
[verification](VERIFY.md). Consult [course lookup](DATA_LOOKUP.md) first.

## Working modes and approval

- **Guided (default):** discuss one decision/behavior at a time. The student
  writes tests and code; the AI explains, reviews, and suggests. Code examples
  require a request. The student executes commands unless execution is authorized.
- **Autonomous (explicit opt-in):** approve the contract, editable files,
  permitted execution, and acceptance gates before the AI implements.
- File-edit and execution permissions are separate. Neither mode grants blanket
  authority. Stop for scope changes, destructive actions, or unresolved behavior.
- Creating a command wiki entry requires a separate user request and approved
  scope; implementation does not promote a draft or imply human verification.

## 1. Research → discuss → keep

1. Query the course catalog/slides; read the GNU command's manual and relevant
   POSIX API references. Record source, manual section, and version when known.
2. Identify purpose, operands, options, defaults, streams, errors, exit statuses,
   and relevant examples. When authorized, explore safe installed-command examples
   in disposable fixtures. Inspect existing project implementations and tests.
3. For troff sources, use the installed manual renderer (man-db/groff), not a
   homemade directive parser. Preserve source references; rendered headings are
   aids, not a guaranteed schema. Do not execute untrusted formatting inputs.
4. Discuss trade-offs and course requirements. **Keep** an agreed core subset;
   list deferred features and deliberate differences from GNU behavior.

### Catalogue the manual before selecting features

- Keep inventories, contracts and execution evidence in `docs/reports/<name>.md`
  (for example, `docs/reports/cat.md`), created with approved file scope.
  Keep `docs/commands/<name>.md` brief: usage, options, limits and a report link.
  Use Markdown, not a new SQLite database; `docs/data.sqlite` stores course
  sources.
- Record the manual source, section, and version (or mark it unknown).
  Catalogue every documented option, operand rule, default behavior, stream,
  error behavior, and exit status before selecting the supported subset.
- Use the table below. Preserve option aliases and interactions; distinguish
  course requirements and project additions from manual-defined behavior.
  Mark inferred tasks as proposals until agreed.

| Feature / reference | Human or AI task served | Course requirement | Keep / defer / undecided | Reason |
| --- | --- | --- | --- | --- |

- Keep course requirements and features necessary for agreed human or AI tool
  workflows. AI-facing means useful task capabilities, not merely an output
  format. Defer other features with explicit reasons; omit nothing silently.
- Check that retained features form complete, testable workflows, including
  their dependencies and failure handling. Leave unresolved choices undecided.
- Approve the inventory's selection before implementation; resolve undecided
  items before approving the behavior contract.

### Select the functional minimum

- Start with course requirements and the command's essential purpose.
- Classify each behavior as required by the course, essential to that purpose,
  or optional. Cite evidence for requirements; mark proposals as proposals.
- Keep required and essential behavior as the smallest useful set. Explicitly
  defer optional features and record why each included behavior is needed.
- Preserve correctness and error handling for the selected scope, including
  boundary inputs, I/O failures, and cleanup.
- Approve supported behavior and exclusions before planning tests.

For `cat`, the original keep/defer decisions are preserved in its
[historical inventory](reports/cat.md); current usage is in
[the command guide](commands/cat.md). Do not infer scope from a generic example.

Gate: the human approves the behavior contract; unresolved decisions stay open.
For refactoring, characterize current behavior before changing it and agree on
any intentional differences. Course examples are illustrative, not proven code.

## 2. Plan the module and tests

- Reuse `src/cmd_spec.h`; do not duplicate `cmd_spec_t`.
- Identify `cmd_<name>.c`, its specification, run/usage functions, registration
  helper, declarations, and optional thin standalone wrapper. Inspect actual
  registry/build interfaces before naming integration points.
- Define argtable3 options/operands once for parser and help. Keep parser ownership
  local to each invocation; specify allocation failures and cleanup.
- Agree on API choices, descriptor ownership, error continuation/stopping policy,
  signal policy when relevant, and exact exit statuses.
- Map every requirement to named tests. Order small end-to-end behavior slices;
  plan test categories now, but write/implement one slice at a time.

Gate: approve interfaces, file scope, execution scope, and the test matrix.

## 3. Implement with TDD

### Reference-first tests: provisional procedure

Use this procedure when requested, before implementing a selected compatible
subset. Refine it after two or three commands; do not build a generic framework
until those commands demonstrate what should be shared.

1. Record the reference executable/version and the selected behaviors.
2. Write independent expected results from the reviewed behavior: fixture bytes,
   arguments, stdout, stderr, exit status, and forbidden side effects. Do not
   generate expected output by invoking the reference during the test.
3. Run those expectations against the reference. Display expected versus got,
   escaping invisible bytes; compare full output even when display is bounded.
   Use isolated fixtures, a controlled locale, cleanup, and a deadline.
4. Check the harness rejects a deliberately incorrect executable or output.
   Record that failure separately from reference passes and implementation TDD.
5. Select the executable through one test-runner argument or Make variable.
   Switch only that path to the project implementation; keep fixtures and
   expectations unchanged unless an explicit contract decision requires an edit.
6. Start each implementation slice with a runnable baseline and observe the
   intended behavioral failure. A missing executable is a setup failure, not RED.
   Implement, rerun, and refactor. Add project-specific tests for deliberate
   differences rather than forcing them to match GNU.
7. Label reference results clearly and keep them separate from project delivery
   results. Once the implementation exists, integrate its tests into `make test`
   with an explicit build dependency; never fall back to the reference silently.

Initial example: `make test-cat` tests GNU cat; later,
`make test-cat CAT_UNDER_TEST=./build/cat` tests our executable after it is built.
Record cases, commands, evidence, and gaps in `docs/reports/<name>.md`.

Lessons from the implemented `cat` and `head`:

- Keep GNU-compatible checks separate from project help, argument-status, and
  JSON checks. JSON has its own contract, including encoding and escaping;
  GNU raw output cannot verify it.
- A standard-library Python CLI driver can express byte fixtures compactly;
  retain C module tests for repeated calls, unchanged argv, descriptor ownership,
  and stream position. CLI success alone does not prove safe module reuse.
- Test stream consumption as well as printed bytes: `head -n 1` must leave
  subsequent bytes available on borrowed stdin. Record the performance cost of
  the selected no-read-ahead implementation.
- Share helpers only after concrete need appears. Keep command-specific
  selection/parsing separate from reusable byte output and JSON encoding.
- In delegated work, distinguish saved artifacts and executed checks from a
  worker's running status. Escalation waits are blockers, not implementation
  progress; report them immediately and route execution to the coordinator.

### Implementation cycle

For each slice: write a test → observe its intended failure → implement minimally
→ rerun it and accumulated relevant tests → refactor while green. A broken test
harness is not evidence of the intended behavioral failure. Add regression tests
for discovered bugs. Preserve unrelated human changes; do not commit or push
without separate authorization.

## 4. Testing methodology

Every acceptance test specifies inputs/setup, expected stdout bytes, stderr
expectations, exit status, and side effects. Use isolated fixtures, controlled
locale/environment, bounded waits, and cleanup; never rely on arbitrary sleeps.

| Layer | Required evidence when applicable |
| --- | --- |
| CLI contract | Help, options, operands, defaults, `--`, invalid/missing arguments, exact output and status. |
| Boundaries | Empty/binary inputs, buffer boundaries, large inputs, unusual filenames, multiple operands. |
| Failure paths | Missing/unreadable inputs, allocation/I/O failures, output failure, partial progress and cleanup. |
| Focused units | Core algorithms and deterministic injected failures through a small test seam. |
| Integration | Registry discovery/dispatch/help, standalone parity, repeated success/failure/help calls without stale state. |
| Regression | Existing suite plus a reproducer for each discovered bug. |

- Prefer real file/pipe CLI tests. Use controlled fault injection for hard-to-force
  cases such as short writes and EINTR; verify real recovery logic, not the mock.
- Test invariants and forbidden effects: unchanged input files, borrowed descriptors
  left open, owned resources released, no unexpected output or global-state changes.
- Compare selected compatible cases with the GNU command; our approved contract is
  authoritative for deliberate differences. Do not demand identical localized
  diagnostics unless their wording is part of the contract.
- Add property tests where useful (for cat: output equals concatenated input bytes).
  Agree on bounded fuzzing, sanitizers, and resource checks where applicable; record
  tool availability and approved omissions. Coverage supports review, not correctness.
- Make tests prove they detect the intended defect (the observed TDD failure or a
  controlled negative check). Never weaken an assertion merely to obtain green.

## 5. Verification and delivery gates

- Compile when needed for tests; run the complete applicable suite against final
  sources, then verify the delivery build. Check help against the accepted parser.
- Current project checks are `make test`, `make lint`, and `make format-check`;
  inspect the actual Makefile and document the command's real build invocation.
  Formatting edits require permission; do not substitute `make format` for a check.
- Exercise happy/error paths and both agreed execution forms. If a registry or
  tool is unavailable, report the gap; do not claim that gate passed.
- Review ownership, cleanup, error propagation, and unintended changes separately
  from test execution. Report baseline failures distinctly from new regressions.
- Keep exact commands/results, failures, skips, and limitations. Documentation,
  demo, logging, and publication changes require their own approved scope.

Done means every agreed requirement has passing evidence and every applicable
acceptance gate is satisfied. A skipped check or blocker is not a pass. Report
changed paths, actual results, and remaining work briefly. No automatic wiki write.

## Reusable per-command record

Fill during discussion; save a separate command plan only with approved file scope.

- Command / mode / owner:
- Course and manual references / reference version:
- Core behavior and rationale:
- Syntax / options / operands / defaults:
- Stdout / stderr / exit statuses / failure policy:
- Deferred features / deliberate GNU differences:
- APIs / ownership / cleanup / signal policy:
- Module interfaces / integration points / editable files:
- Authorized commands / prohibited side effects:
- Requirement → named test → expected result → actual evidence:
- Ordered TDD slices:
- Verification gates / approved omissions / unresolved decisions:
- Delivery status / wiki requested separately:

Source basis: Command Anatomy slides 12–13 (AI contract and evidence) and
File I/O slides 3, 18, 21 (milestone, shared parsing/help, acceptance), indexed in
`docs/data.sqlite` from `data/02/`. This procedure does not approve the proposed
cat contract or imply any command has been implemented.
