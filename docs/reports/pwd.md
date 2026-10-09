# pwd: inventory, contract, and report

Status: implemented; CLI and module verification passed. Scope selected under the
user's explicit autonomous batch authorization in NEXT_BATCH.md.

## Sources and purpose

Identify the actual current working directory. Course: CLI_Tools.docx paragraphs 142–145, retrieved from
docs/data.sqlite. Manual: installed man 1 pwd, GNU coreutils 9.4,
footer August 2026, rendered with man-db on 2026-10-07. Every option and
functional behavior in that manual is catalogued below. Author/copyright/
bug-reporting/related-manual sections are provenance, not executable features.

## Manual inventory

| Feature | Decision | Human/AI task or reason |
| --- | --- | --- |
| Absolute current directory; default -P | Keep | Course core; reliable path for human and agent subsequent actions |
| -P, --physical | Keep | Explicit form of the default |
| -L, --logical; trust/validate environment PWD | Defer | Physical paths avoid ambiguous symlink/environment semantics |
| --help | Keep | Discovery; add course -h |
| --version | Defer | Not necessary for path discovery |
| Shell builtin may supersede executable | Document | Tests invoke explicit executable paths |
| --json (course addition) | Keep | Course object {"cwd": "..."} |
| -- and extra operands | Keep delimiter; reject operands | pwd takes no file operands |

## Selected contract

Syntax: `pwd [-h|--help] [-P|--physical] [--json]`.

- Status 0 succeeds, 1 reports I/O/resource/encoding failure, 2 reports invalid
  arguments. Diagnostics go to stderr; stdout contains data or requested help.
- Invalid arguments take precedence over help. Help performs no input reads.
- Options/help share argtable3 definitions. Preserve caller argv and inherited
  streams. Linux target; inherit signal dispositions; do not change shell state.
- Unsupported options are argument errors; inherited unambiguous long-option
  abbreviations are accepted by argtable3, not a new feature promise.
- No operands. Physical path ignores PWD and resolves symbolic links through
  getcwd. Raw output is the path plus newline; JSON is {"cwd": "..."}.
  Grow the path buffer as needed; report a removed/inaccessible cwd as failure.
  JSON requires a valid UTF-8 path and escapes controls.

## Implementation and verification plan

- Files: src/cmd_pwd.c/.h, src/pwd_main.c; shared helpers only where
  actual duplication warrants them. Integrate build/test targets in Makefile.
- Test driver: tests/test_pwd.py, standard-library CLI fixtures with an
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

- Reference-first evidence: `/usr/bin/pwd` passed five compatible test methods
  (four project-only methods skipped). `/usr/bin/true` negative control failed
  eight subchecks, detecting missing output/status rather than setup errors.
- Runnable baseline compiled successfully, then failed all nine project test
  methods (12 failing subchecks): missing path/help/JSON and wrong error status.
  Tests were saved and executed before the implementation replaced the baseline.
- Implementation uses a growing `getcwd` buffer, argtable3 parser-derived help,
  and existing `cmd_lines` UTF-8/JSON/write helpers. No registry exists yet.
- CLI cases cover physical/default/delimiter, symlinks and false environment
  PWD, paths longer than 1,000 bytes, arbitrary filename bytes, removed cwd,
  help/invalid arguments, Unicode/control escaping, invalid UTF-8 without partial
  JSON, and `/dev/full` for raw/JSON/help output.
- `tests/test_pwd_module.c` exercises five repeated success/error/help calls,
  unchanged argv, expected output, and borrowed descriptors remaining open.
- `make test-pwd-impl`: GREEN, all nine CLI methods passed, plus all five
  repeated module invocations. New C/header files passed formatting checks.
- Initial targeted lint identified `strcpy` in the module fixture despite its
  preceding size assertion; replaced with a bounded `memcpy`. Lint rerun pending.
- Review added module tests for zero/negative argc and NULL argv. Their runnable
  RED aborted with status 134 because `run(0, NULL)` printed cwd instead of
  returning 2. An entry guard now rejects those invalid calls before allocation.
  Help also checks the stream error flag and preserves errno while clearing it.
  Final regression rerun pending.

Reproduce: `make test-pwd` for the GNU-compatible reference checks;
`make test-pwd-impl` for the implementation and module checks.

Coordinator verification, 2026-10-08: `make test` passed, including all nine
pwd CLI methods and its module checks; `make lint` passed all 18 existing C
source/test files. This closes the pending regression and lint reruns above.
`make format-check` still fails on pre-existing formatting in unrelated files.

Limits: GNU logical `-L` and version output remain deferred. Linux target;
no registry integration is claimed. No fault-injected allocation/getcwd failures
or concurrency checks; real removed-directory and output-failure cases cover
operational error paths. Argtable3's internal allocator may abort on exhaustion;
sequential module reuse is tested, thread safety is not promised.
