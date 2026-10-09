# head: inventory, contract, and report

Status: implemented and verified; remaining commands are tracked separately.
Scope selected under the
user's explicit autonomous batch authorization in NEXT_BATCH.md.

## Sources and purpose

Inspect the beginning of a file or stream. Course: CLI_Tools.docx paragraphs 35–39, retrieved from
docs/data.sqlite. Manual: installed man 1 head, GNU coreutils 9.4,
footer August 2026, rendered with man-db on 2026-10-07. Every option and
functional behavior in that manual is catalogued below. Author/copyright/
bug-reporting/related-manual sections are provenance, not executable features.

## Manual inventory

| Feature | Decision | Human/AI task or reason |
| --- | --- | --- |
| First 10 LF-delimited lines; no extra newline | Keep | Course core; quick previews for people and agents |
| -n, --lines=N with decimal N >= 0 | Keep | Course count selection; default 10; last count wins |
| Negative counts (all but last N); multiplier suffixes b,kB,K,MB,M,GB,G,T,P,E,Z,Y,R,Q and KiB-style prefixes | Defer | More selection modes are unnecessary for the first preview contract |
| -c, --bytes=[-]NUM | Defer | Byte slicing is a separate task |
| Multiple FILE operands and automatic filename headers | Defer | One source per invocation keeps output and JSON attribution unambiguous |
| -q, --quiet, --silent; -v, --verbose | Defer | No multi-file headers in this subset |
| -z, --zero-terminated | Defer | This subset defines lines by LF |
| --help | Keep | Discovery without consuming input; add course -h alias |
| --version | Defer | No immediate content-inspection task requires it |
| No FILE or FILE='-' reads stdin | Keep | Pipeline and agent-supplied content |
| -- ends options | Keep | Dash-prefixed filenames |
| --json (course addition) | Keep | One object with path and selected lines |

## Selected contract

Syntax: `head [-h|--help] [-n N|--lines=N] [--json] [--] [FILE]`.

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

- Files: src/cmd_head.c/.h, src/head_main.c; shared helpers only where
  actual duplication warrants them. Integrate build/test targets in Makefile.
- Test driver: tests/test_head.py, standard-library CLI fixtures with an
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

The installed GNU reference passed 16 independent expected-output checks:
`python3 tests/test_head.py --program /usr/bin/head --reference`.
The same driver rejected `/usr/bin/true` as a negative control. A compiled,
runnable `head_main.c` baseline returning success failed the default-output
assertion (expected ten lines; got empty bytes). This is the observed RED.

The implementation compiled with the project's strict C flags and passed the
initial 36 project checks using `make test-head-impl`. Additional encoding/path
and module ownership checks were then added. Final verification:

| Command | Result |
| --- | --- |
| `make test-head-impl` (included in `make test`) | 45 CLI checks and the module ownership/reuse executable passed |
| `make test` | 25 previous cases plus all head checks passed |
| `make lint` | All 12 source/test files passed |
| `clang-format --dry-run --Werror src/cmd_head.c src/cmd_head.h src/head_main.c src/cmd_lines.c src/cmd_lines.h tests/test_head_module.c` | Passed |

Run only reference checks with
`python3 tests/test_head.py --program /usr/bin/head --reference`; run the project
with `make test-head-impl`. Full-repository formatting contains pre-existing
violations outside these edited files and is not claimed to pass here.

Implementation: `src/cmd_head.c/.h` exports `cmd_head_spec`; `head_main.c` calls
the same module. `cmd_lines.c/.h` provides byte output, UTF-8 validation, and JSON
quoting helpers. No shell registry exists yet. Parser storage and selected bytes
are owned per invocation; caller argv and inherited descriptors stay borrowed.
Owned inputs close before output; Linux close failures are not retried. Input
failures and invalid UTF-8 therefore produce no selected output. Output errors
can leave partial output and return 1. Signal dispositions are inherited.

The prefix reader uses one-byte POSIX reads so a built-in leaves every byte after
the selected prefix available on nonseekable stdin. This favors exact descriptor
semantics over throughput; performance tuning is deferred. Memory is proportional
to selected content, with geometric buffer growth, and has no fixed line limit.
Count parsing uses `uintmax_t`; the tested Linux target accepts through
18446744073709551615 without allocating storage proportional to that count.

Tests cover default/count/zero/empty/unterminated/binary/long lines, files/stdin,
dash paths, repeated counts, missing inputs/directories, `/dev/full`, argument
errors, JSON escapes/Unicode, malformed encodings, and selected-only validation.
`tests/test_head_module.c` adds repeated success/error/help calls, argv stability,
borrowed descriptor lifetime, zero-count nonconsumption, and no prefix read-ahead.
The driver compares complete bytes, status, diagnostic presence and unchanged
fixture files; expected/got displays are bounded to 90 bytes for readability.

Limitations: allocation/close failures and injected EINTR/short-write paths are
not yet fault-injected for this command; no sanitizer run, fuzzing, or performance
benchmark is claimed. Argtable3's internal allocator may abort on exhaustion. Concurrent module
calls are not promised because the dependency uses process-global parser state.
No commits or pushes were made for this command.
