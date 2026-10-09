# Echo implementation contract

Autonomous scope: echo only, from committed HEAD 71c6e49a4be299db071f0a2a1fc6ef411e343a5e. Skipped rmdir and mixed working files excluded. No staging, commits, AI_LOG, wiki or publication.

## Research and selection (before tests)

Read-only SQLite `lecture_content`/`lecture_sources`, `CLI_Tools.docx` paragraphs 221–223: print text, `-h, --help`. Optional human JSON is not selected. Installed `/usr/bin/echo` is GNU coreutils 9.4; rendered `man echo`, section 1, plus the downloaded GNU Coreutils 9.12 primary manual https://www.gnu.org/software/coreutils/manual/html_node/echo-invocation.html. Primary APIs: https://www.argtable.org/tutorial/ and https://pubs.opengroup.org/onlinepubs/9799919799/functions/write.html. Direct primary HTML downloads succeeded; web retrieval of GNU/POSIX separately failed.

| Manual feature | Decision | Reason |
| --- | --- | --- |
| Zero or more STRING operands, spaces between, final newline, stdout | Keep | Essential print-text workflow |
| `-n` suppress newline | Defer | Not course-required; always terminated output |
| `-e` enable / `-E` disable escapes | Defer | Literal bytes avoid escape language |
| Escapes: backslash, alert, backspace, stop-output, escape, form feed, newline, CR, tab, vertical tab, octal, hex | Defer all | No interpretation |
| `--help` | Keep, add course `-h` | Project parser/help convention |
| `--version` | Defer | No version feature selected |
| GNU long help/version recognized only alone; other option-looking strings often literal | Deliberately differ | Project parser rejects unsupported options before output |
| GNU `--` is literal text | Deliberately differ | Project `--` ends options and is not printed |
| GNU POSIXLY_CORRECT option/escape behavior | Defer | Deterministic literal contract, no environment-driven modes |
| Shell builtin warning / printf alternative | Document | Standalone tests explicitly use `/usr/bin/echo` |
| Success 0, failure nonzero | Keep, specify 1/2 | Predictable project statuses |

## Exact contract / design

`echo [-h|--help] [--] [TEXT...]`. Parser flags may occur among operands; a single help request suppresses operand output only after successful argument parsing. Unsupported options, duplicate help, and help plus unsupported options return 2, no stdout, diagnostic stderr. Abbreviated long options follow Argtable's existing convention. After `--`, all strings including help/option tokens are literal; `-` is literal. Empty strings count as operands. Output preserves non-NUL bytes, including invalid UTF-8, LF, CR, backslashes, tabs and control bytes; no JSON. Embedded NUL cannot exist in a C argv string or OS argument; the shell rejects NUL input before dispatch.

Status 0 success; 1 operational/output/allocation error; 2 invalid module vector or parser errors. Validate argc >= 1, argv non-NULL and all argc elements non-NULL before parsing; valid NUL-terminated C strings are the caller's responsibility. No aggregate output allocation. Size operand table by argc, but render logical `[TEXT]...` grammar independent of argc. Save/restore exported getopt variables; copy argv pointers before parsing; parser table and pointer copy invocation-owned and freed on every path. Preserve caller pointers/string bytes, environment, cwd, signals and borrowed descriptors. Stdin never read. Short writes and EINTR use existing `lines_write`; zero write becomes EIO. Output failure stops immediately, may leave a prefix, reports stderr and returns 1; no rollback. SIGPIPE remains inherited, as for existing modules. Stdio help flush is checked and its error indicator cleared for reuse. Usage callback has a void API; reports constructor failure on supplied stream but cannot return an operational status.

Module/header/wrapper, Python literal/reference/project/main/PTY checks, C reuse/fault tests. Narrow integration in Makefile, registry and shell inventories; README/FILES/shell guide candidates only. Planned verticals: literal output; parsing/help; owned faults/state/reuse; real shell integration. Independent expected fixtures precede reference/project runs, then negative controls. Full fresh exact-patch normal and default O0 non-PIE ASan/UBSan regression, lint and scoped formatting required. Six pre-existing global format failures, optimized shell injection baseline and internal Argtable OOM are separate inherited limits, not passes for echo.

## Execution

The first worker ended in a provider timeout, not a permission denial. Its saved
18 command records and complete raw streams were authenticated against their
stdout/stderr SHA-256 ledger before reuse. Original scratch artifacts remain
unchanged. The resumed checkout was rebuilt from the exact committed baseline,
then only the echo allowlist was imported. Earlier GREEN results establish only
their named slices, not unexecuted full-delivery gates.

| Gate / actual command or label | Observation |
| --- | --- |
| Saved literal RED → GREEN | Runnable no-output baseline failed newline expectation; literal implementation passed |
| Saved parser RED → GREEN | Literal-only executable failed help expectation; Argtable implementation passed |
| Saved invalid-vector RED → GREEN | Intended `ECHO_STATUS` assertion then passing guard checks |
| Initial resumed `make all test` | Failed missing echo case in exact standalone-parity inventory; case added; subsequent full suite passed |
| `host-parser-mode-RED` → `host-parser-mode-GREEN` | Synthetic POSIXLY_CORRECT caused `a --help` to print as data; private pointer normalization places flags before operands without altering host environment; standalone/main fixtures now pass both modes |
| `logical-help-RED` → `logical-help-GREEN` | Help advertised two operands; logical Argtable maximum 3 renders `[TEXT]...`; parsing capacity restored afterward |
| `make test-echo` | 467 independent GNU-compatible checks pass; includes every non-NUL single byte and 200 seeded operand vectors |
| `make test-echo-impl` | 528 checks pass: literals, parser modes, help, output failure, main parity/discovery, 100 mixed loop cycles and child-owned PTY with exact prompts |
| `build/test_echo_module` | 500 success/help/error reuse cycles; caller argv pointers/bytes, exported getopt state, cwd/environment/signal identity, descriptors and untouched stdin; constructor NULL, owned OOM, EINTR, short/zero/error/prefix writes and real failed-help reuse pass |
| Reference negative executable | Expected assertion failure: no-output executable violates final newline fixture |
| Four module behavioral mutants | Expected SIGABRT with defect-specific argv/getopt/help-reuse/output-length markers; no generic crash accepted |
| `make all test`, default CFLAGS | Full current suite passes after both source fixes |
| `make lint` | Status 0; only inherited suppressed non-user/NOLINT diagnostics |
| Scoped `clang-format --dry-run --Werror` | Changed C sources/headers formatted; exact frozen-patch repeat recorded externally |
| Pre-fix full O0 non-PIE ASan/UBSan run | Passed; superseded by required frozen-patch post-fix regression recorded externally, not counted as final-source verification |
| Baseline and candidate `make format-check` | Same six inherited paths fail: `src/cmd_spec.h`, `src/traverse.c`, `src/traverse.h`, `tests/test_help.c`, `tests/test_output.h`, `tests/test_recursive.c`; unrelated formatting untouched |
| Unchanged HEAD O1 sanitizer `make test-shell` | Python shell tests pass, inherited C injected-read assertion aborts; not an echo regression or passing optimized gate |
| Internal dependency malloc OOM probe | Argtable prints `Out of memory!`, exits 1 before returning to echo; module constructor NULL/owned allocation recovery is not an all-library-OOM guarantee |

Full command arrays, cwd, actual statuses, expected negative statuses, durations
and stream hashes are in [the ledger](../../logs/commands/echo-evidence.json);
complete original and resumed streams are in
[the raw log](../../logs/commands/echo-execution.log). Saved historical tests were
smaller; historical count 12 does not describe the final 467/528-check runners.

Delivery is one external HEAD-based patch and full blob/mode/SHA-256 manifest.
The report, log and ledger are frozen before fresh exact-patch normal and
sanitizer regression; those final execution streams remain outside the patch to
avoid self-referential changes. Independent review and authorized exact-index
verification remain parent-orchestrator gates. Shared fixed inventory, HEAD and
empty index are rechecked externally. Scratch-only session registration is
excluded from delivery to preserve shared bytes.

Limits: no -n/escape language/JSON; inherited SIGPIPE; valid C strings/argv storage
remain caller obligations; no thread-safety or preservation of getopt private
interrupted scans; dependency internals are not instrumented by the system
library build. Huge module vectors beyond real OS argv limits and exhaustive
concurrency/fuzzing are not claimed. Course API/anatomy files are available in the
shared workspace but not committed HEAD and are deliberately not imported.
No rmdir queries/tests were repeated. No stage, commit, AI_LOG, wiki, push or
next-command work.
