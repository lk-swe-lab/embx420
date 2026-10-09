# touch implementation record

## Selection and contract (before tests)
Autonomous touch-only implementation; no commit/publication/registry work.
Course read-only SQLite lookup: CLI_Tools.docx paragraphs 72–75 (touch,
-h/--help, --json); FileIO slides 2, 4, 7 (module and mutation validation).
Installed GNU coreutils 9.4 `man 1 touch` (August 2026 footer), and Linux
man-pages 6.7 `man 2 utimensat` / `man 2 open`, reviewed in this session.

Syntax: `touch [-h|--help] [--json] [--] FILE...`.
0 success; 1 operational/output/allocation failure; 2 argument error.
Unknown/repeated options, missing operands, empty operands, malformed module
argv, and non-UTF-8 JSON paths fail before mutation. Help after valid parsing
has precedence over operations (including missing operands); invalid options
still fail. Text mode accepts arbitrary non-NUL pathname bytes.

Default sets both atime and mtime to kernel current time; filesystem precision
applies; ctime changes normally. Existing content, inode, ownership and mode
are unchanged. Creation requests 0666 subject to inherited umask/default ACL.
Directories, hard links, existing symlink referents, FIFOs, sockets, and device
nodes receive timestamp-only updates without opening their content. Final
symlink's own timestamps are not intentionally updated; kernel traversal can
update its atime (observed with both GNU and project). Dangling final links
fail (deliberate GNU difference), missing parents fail, symlink loops fail;
no parents are created. Kernel slash/dot/path resolution is retained.

Use utimensat first; on ENOENT exclusively create O_WRONLY|O_CREAT|O_EXCL
with O_CLOEXEC, then futimens and one close. EEXIST retries utimensat once
for a competing creator; no open of an existing object. Retry EINTR on
utimensat/open/futimens; never retry close on Linux. Close failure is reported.
No rollback: new files can remain after later futimens/close failures.
Path races are not transactional, and prefix symlinks can redirect operations.

Text success is silent. Each failed operand emits stderr diagnostic and does
not stop independent later operands. JSON emits one ordered array after all
operations: {"path":string,"status":"ok"|"error","message":null|string}.
JSON names must be UTF-8, messages are errno diagnostics (ASCII generic
fallback when locale diagnostics are not UTF-8), controls are escaped.
Output failure returns 1; output can be incomplete and completed effects remain.
Borrow stdin/stdout/stderr, argv; preserve input position, cwd, umask and signal
handlers. SIGPIPE follows inherited disposition (may terminate process).
Invocation-local argtable definitions and copied argv, preallocated JSON error
array; free all owned entries on every return. Installed argtable internal OOM
can terminate the process; only module-owned/constructor-NULL recovery promised.
Usage derives from parser with stable logical operand cardinality.

## Complete manual inventory / exclusions
Keep FILE... current atime+mtime and missing regular creation, --help; add course
-h and --json. Literal '-' names a file (GNU uses stdout-associated file).
Defer -a/-m/--time selection; -c/--no-create; -d/--date flexible grammar;
-r/--reference; -t explicit dates; ignored -f; -h/--no-dereference (conflicts
with course help); --version. At assignment start no registry existed; direct
cmd_spec dispatch is tested. A concurrent sibling added shell/registry sources during verification;
touch does not modify or register into that in-progress integration.

## Ordered TDD / acceptance matrix
1. Creation, update, preservation, multi-target/partial effects: CLI independent
   fixtures, GNU common subset and incorrect-executable negative control.
2. Parsing/help/preflight/literal dash: exact statuses/output and forbidden files.
3. JSON: independent ordered objects, escapes/errors and before-effects UTF-8.
4. Module: reuse, stable help, borrowed state/resources; deterministic allocation,
   syscall, race/EINTR and output faults; real full/broken-pipe output failures.
5. Targeted/reference/full/fresh-build/lint/format/sanitizer gates.
Evidence: logs/commands/touch-*.log and scratch touch artifacts.

## Observed TDD evidence
- Slice 1: old existing-file timestamps failed in the runnable no-effects module,
  GNU passed, /usr/bin/true failed the same oracle; minimal timestamp update green.
- Slice 2: two missing operands failed status/creation expectations; exclusive
  creation and independent-operand iteration made accumulated fixtures pass.
- Slice 3: no operands wrongly succeeded; argtable/help/preflight made it fail 2
  before effects. Metadata target characterization exposed a faulty link-atime
  oracle in both GNU and project; corrected to unchanged link mtime and explicit
  kernel traversal-atime policy, not a claimed production RED/GREEN.
- Slice 4: absent JSON bytes and accepting invalid UTF-8 failed independent
  expectations; JSON generation/preflight green. Initial json.loads error was
  replaced with exact byte assertion before implementation.
- Slice 5: module assertion failed at interrupted utimensat, verified by gdb;
  retry loops made syscall/EINTR/race/resource tests pass. Existing cleanup,
  allocation and non-interruption fault behavior was characterization, not
  falsely reported as a distinct new RED.
- Slice 6: simulated non-UTF-8 strerror produced invalid JSON; exact fallback
  assertion failed (direct status 134 and gdb). Validated-message fallback green.
- Scoped formatting refactor was followed by accumulated green tests.

## Final verification and requirements mapping
| Requirement | Actual evidence |
| --- | --- |
| Defaults/create/multi/error continuation/no truncation | test_existing, test_creation_multi_partial; exact binary bytes/inode/owner/mode/time bounds |
| Symlinks/hardlinks/dirs/FIFOs/sockets | test_metadata_targets, test_dangling_loop_and_trailing_slash; no FIFO readers, bounded subprocess deadlines |
| Parse/help/invalid-before-effects/literal '-' | test_arguments_help_dash; malformed argv module calls; identical help across argc and usage callback |
| JSON/order/escapes/messages/UTF-8 | test_json_order_escaping_errors, test_json_utf8_before_effects; exact bytes and independent objects; injected non-UTF-8 locale message |
| Permissions/umask/boundaries | test_permissions_umask, test_permission_denied_continue (uid 1000, no root skip); 128 independent seed-420 fixtures plus duplicate operands |
| Syscall failure/partial effects/race/EINTR | module utimensat EACCES, open ENOSPC, futimens/close EIO; later operands succeed, new files remain after post-create failure; competing creator retains race-data |
| Allocations/reuse/resources/forbidden state changes | module 90 repeated calls; four constructor-NULL positions, owned malloc/calloc faults; allocation/fd tracking, borrowed argv/descriptors/stdin position/cwd/umask/SIGPIPE checks |
| Output faults | module EINTR/short/zero/EIO writes, real /dev/full help+JSON, ignored-SIGPIPE closed pipe; CLI verifies output failure retains newly created files |

`make test-touch test-touch-impl`: GNU 6 compatible tests pass (5 deliberate
project-only skips); project all 11 tests and module assertions pass.
`clang-format --dry-run --Werror` on the four touch C/header files: pass;
C/header lines limited to 80. Scoped clang-tidy: pass, one suppressed non-user
warning. Strict C17 builds emit no compiler diagnostics.
ASan+UBSan (leak detection and halt-on-error, -O1 -no-pie): all 11 CLI tests and
fault-injected module pass; installed argtable dependency is not instrumented.

Fresh scratch tree builds only against installed argtable/CUnit directories;
selected touch sources/tests/Makefile are byte-identical to working sources
(final SHA256 ledger in touch-final-fresh-sanitizers.log). Fresh
`make test-touch test-touch-impl`, `make test`, `make all`: pass. For this full regression only,
scratch src/main.c is pinned to HEAD b0db03c99bfd2e1a4add175448717cdf6e8e2718,
because concurrent unrelated shell work changed main before its Makefile links
were added. No working source was reverted. This is baseline-compatible full
regression evidence, not a claim that concurrent shell integration passes.
Reproduction scripts: $TMPDIR/touch-work/verify.py and sanitize.py; complete
raw outputs: touch-fresh-{0,1,2}.log; final sanitizer execution and commands
are in touch-final-fresh-sanitizers.log (earlier run in touch-sanitizers.log).

Working-tree `make test` was attempted and blocked by main's unresolved shell/
registry symbols; `make lint` was blocked by concurrent shell analyzer errors
and tests/test_shell_module.c missing shell.h. Logs preserve those failures;
none are in touch. Global `make format-check` still fails outside touch: six
files observed (src/main.c was reformatted by sibling, reducing the inherited
seven): cmd_spec.h, traverse.c/h, test_help.c, test_output.h, test_recursive.c.
No unrelated fixes, staged files, commits, pushes, wiki writes, cp work, README/
FILES edits, or AI_LOG reads were performed. Session registration appended only
verified session 20261008_185718_50ccff (title: Subagent: Implement touch only in
/home/name/Documents/420…). Shared cmd_lines/cmd_spec remain untouched.

## Limits / review readiness
Ready for independent touch review, not an isolated commit claim. The parent
must reconcile/finalize its concurrent shell integration and rerun global gates.
No actual shell-registration/discovery test for touch; direct specification
execution and standalone delegation are covered. No privileged live device,
read-only filesystem, default-ACL, or adversarial prefix-race experiment;
FIFO/socket/directory behavior and deterministic syscall failures cover ordinary
nonregular/error logic. No thread-concurrent parser invocation guarantee or
unbounded fuzzing. print_usage's void ABI cannot report constructor failure;
the tested constructor-NULL path emits no usage bytes and releases resources.
Internal argtable allocation failure may panic/exit; module recovery tests do
not claim library-wide all-OOM handling. Changes are not independently reviewed
or staged. Detailed logs include real failing setup/oracle attempts, separately
identified from intended behavior failures.

