# tail: inventory, contract, and report

Status: implemented and command checks passed; final batch regression pending.
Scope selected under the
user's explicit autonomous batch authorization in NEXT_BATCH.md.

## Sources and purpose

Inspect the end of a file or finite stream. Course: CLI_Tools.docx paragraphs 35–39, retrieved from
docs/data.sqlite. Manual: installed man 1 tail, GNU coreutils 9.4,
footer August 2026, rendered with man-db on 2026-10-07. Every option and
functional behavior in that manual is catalogued below. Author/copyright/
bug-reporting/related-manual sections are provenance, not executable features.

## Manual inventory

| Feature | Decision | Human/AI task or reason |
| --- | --- | --- |
| Last 10 LF-delimited lines; preserve final unterminated line | Keep | Course core; inspect recent messages/content |
| -n, --lines=N with decimal N >= 0 | Keep | Course count selection; default 10; last count wins |
| -n +NUM (start at line); multiplier suffixes b,kB,K,MB,M,GB,G,T,P,E,Z,Y,R,Q and KiB-style prefixes | Defer | Finite last-N selection is sufficient |
| -c, --bytes=[+]NUM | Defer | Byte slicing is outside this subset |
| -f, --follow[=name|descriptor]; default descriptor following | Defer | Long-lived monitoring needs a separate lifecycle contract |
| -F (= --follow=name --retry) | Defer | Depends on following and reopening |
| --max-unchanged-stats=N (default 5); --pid=PID; --retry; -s, --sleep-interval=N (default 1.0) | Defer | Follow-mode polling, rotation, and process-lifetime controls |
| Multiple FILE operands and automatic filename headers | Defer | One source per invocation |
| -q, --quiet, --silent; -v, --verbose | Defer | No multi-file headers |
| -z, --zero-terminated | Defer | LF is the supported delimiter |
| --help | Keep | Discovery; add course -h |
| --version | Defer | No immediate file-inspection task requires it |
| No FILE or FILE='-' reads stdin | Keep | Finite piped content |
| -- ends options | Keep | Dash-prefixed filenames |
| --json (course addition) | Keep | One object with path and selected lines |

## Selected contract

Syntax: `tail [-h|--help] [-n N|--lines=N] [--json] [--] [FILE]`.

- Status 0 succeeds, 1 reports I/O/resource/encoding failure, 2 reports invalid
  arguments. Diagnostics go to stderr; stdout contains data or requested help.
- Invalid arguments take precedence over help. Help performs no input reads.
- Options/help share argtable3 definitions. Preserve caller argv and inherited
  streams. Linux target; inherit signal dispositions; do not change shell state.
- Unsupported options are argument errors; inherited unambiguous long-option
  abbreviations are accepted by argtable3, not a new feature promise.
- One input file or stdin. N is unsigned decimal, including zero; signs,
  whitespace, suffixes, overflow, and missing values are errors. N=0 returns
  empty output (or an empty JSON lines array) after validating/opening input.
- Raw output preserves all selected bytes, including NUL and missing final LF.
  JSON removes one terminating LF from each selected line; preserves CR/empty
  lines; requires valid UTF-8 in selected content and path, with controls escaped.
  Invalid UTF-8 returns 1 before JSON output. Stdin path is "-".
- JSON is one object: {"path": "...", "lines": ["...", "..."]}, plus newline.
  Input read failures produce no selected output; output errors stop immediately.
- Head stops reading after N lines. Tail reads finite input to EOF, retaining
  only the last N lines. Memory grows with selected content plus one input line.

## Implementation and verification plan

- Files: src/cmd_tail.c/.h, src/tail_main.c; shared helpers only where
  actual duplication warrants them. Integrate build/test targets in Makefile.
- Test driver: tests/test_tail.py, standard-library CLI fixtures with an
  explicit executable and reference/project modes; isolated cwd and locale.
- Sequence: fixed expected results against reference; negative control;
  runnable baseline RED; minimal implementation GREEN; regression and lint.
- Cover normal, empty, binary/Unicode where relevant, option errors, unusual
  paths, output failure, and repeated module calls. Compare stdout, stderr,
  exit status, and unchanged input files. Full GNU formatting is not our oracle
  for deliberately different human/JSON output.
- No registry currently exists; test cmd_spec_t and standalone parity without
  claiming shell registry integration. No commits or pushes in this batch.

## Results

Supplemental check: `python3 tests/test_lines_random.py` passed 120 seeded
head/tail scenarios comparing raw GNU output and project JSON, including
8192-byte boundaries and malformed selected UTF-8. This supplements, rather
than replaces, the independent fixed expectations.

The installed GNU reference passed 17 independent fixed-output checks:
`python3 tests/test_tail.py --program /usr/bin/tail --reference`.
The harness rejected `/usr/bin/true` as a negative control. A compiled runnable
baseline returning success failed the default-output assertion: expected last ten
lines, got empty bytes. The executable was then connected to the implementation.

| Verification | Actual result |
| --- | --- |
| `make test-tail-impl` | 46 CLI checks and module reuse/ownership executable passed |
| clang-tidy on `src/cmd_tail.c`, `src/tail_main.c`, `tests/test_tail_module.c` with project flags | All three passed |
| `clang-format --dry-run --Werror src/cmd_tail.c src/cmd_tail.h src/tail_main.c tests/test_tail_module.c` | Passed |

The orchestrator will run the combined regression suite after remaining commands;
this report does not claim that later gate has passed.

Coordinator verification, 2026-10-08: `make test` passed all existing regressions,
including 46 tail CLI checks, module checks, and 120 head/tail differential
scenarios. `make lint` passed all 18 existing C source/test files.

The command exports `cmd_tail_spec` through `src/cmd_tail.c/.h` and a thin
`tail_main.c` wrapper. Its finite-input algorithm reads 8192-byte buffers, assembles
one current line, and maintains a linked queue of at most N finished lines. Each
new line evicts the oldest when full. No allocation is proportional to the numeric
count alone; maximum uintmax_t with tiny input stays small. Memory grows with the
retained selected content plus one current line, allocation slack, and queue
metadata. N=0 opens/validates the operand and does not read it.

`cmd_lines.c/.h` is reused unchanged from head for complete writes, UTF-8
validation, and JSON quoting. Selection algorithms stay separate because head
must stop without reading ahead while tail must reach EOF. This is the observed
shared-helper boundary; no general command framework or shell registry was added.
The parser table and queue belong to each invocation; argv and inherited streams
remain borrowed. Owned input closes before output. Read/close/encoding failure
emits no selected output; write failure can leave partial output and returns 1.
Only selected lines are validated for JSON; invalid UTF-8 in discarded lines is
irrelevant. All selected lines and the path are validated before any JSON bytes.

CLI tests cover defaults, zero/empty/unterminated/binary/long final lines, counts,
stdin/files/dash paths, strict errors, output failure, JSON encoding and paths.
`tests/test_tail_module.c` covers repeated success/error/help invocations, caller
argv stability, borrowed descriptors, finite EOF consumption, and zero-count
nonconsumption. Exact diagnostics are not promised, but status and diagnostic
presence are asserted. Inputs remain unchanged. Expected/got previews are capped
at 90 bytes; comparisons cover the full output.

Limitations: no injected allocation/close/EINTR/short-write tests, sanitizer run,
fuzzing, or performance benchmark is claimed. Argtable3 allocation failure can
abort and its global parser state precludes a concurrent-call guarantee. Signal
dispositions are inherited. No commits or pushes were made.
