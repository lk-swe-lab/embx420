# edit-insert-line independent review

**Verdict: PASS within the selected contract.** No demonstrated command-scope
defect was found; production sources and tests were not changed. Negative controls
are scratch characterization sensitivity checks, not a fabricated fix RED/GREEN.
No staging, commit, push, wiki, AI_LOG access or next-command work occurred.

Base: `5ef0df6b8de13cc708352de1133e0fd75181475a`. Review was independent of the
implementation worker. [Selected contract](edit-insert-line-implementation.md),
[complete raw streams](../../logs/commands/edit-insert-line-review-execution.log)
and [command/status/hash ledger](../../logs/commands/edit-insert-line-review-evidence.json)
retain the actual results and reproduction scripts.

## Findings and coverage

Read-only SQLite lookup confirms CLI_Tools.docx paragraphs 89 and 97–101:
complete-file deterministic editing, FILE N TEXT, help and JSON. Append, LF-only
new lines and metadata policies are selected project semantics, not extra course
requirements. No GNU command was used as a custom-command oracle.

Source review checked argtable/shared help/JSON validation, bounded positive
ASCII decimal N, empty TEXT, CR/LF rejection, invocation-local ownership,
malformed module argv, caller argv and exported getopt globals. Reviewed
same-directory exclusive staging, short/EINTR/zero writes, sync and owned close,
source/path consistency before rename, errno/cleanup and postcommit output.
Final symlink/hardlink/type/foreign-owner/special-mode rejection, ordinary
mode/uid/gid retention and borrowed process resources match the documented policy.
No secret, shell-execution/injection or unexpected execution path was found.
Prior editor/shared runtime sources remain byte-identical to HEAD.

| Fresh executed gate | Result |
| --- | --- |
| HEAD + narrow integration `make all test lint` | PASS; full regression, 14 CLI groups/150 properties, 355 module calls, 27 shell groups |
| Same fresh source, default O0 ASan/UBSan `make all test` | PASS; non-PIE, leak/halt enabled; dependencies uninstrumented |
| `python3 tests/test_edit_insert_line.py --program build/main --prefix edit-insert-line` | PASS in both normal and sanitized checkouts |
| Scoped `clang-format --dry-run --Werror` | PASS for insertion C/header/wrapper/module test and registry/shell-module integration |
| Baseline and candidate `make format-check` | Both fail on exactly six unchanged inherited paths, not insertion regressions |
| New independent original-offset byte oracle | PASS: 895 attempts across 263 exhaustive/random/buffer fixtures, alternating standalone/main |
| Seven negative controls | Intended exact-byte/argv/getopt/commit/recheck failures observed; all expected statuses and markers checked |
| Independent `strace -yy` | PASS: source never written/truncated; exclusive stage write, fsync, close precede rename |

The new oracle independently enumerates existing line starts, excludes EOF after
trailing LF, inserts TEXT+LF at original byte offsets, and adds an LF separator
only for unterminated append. It checks JSON/status, original bytes, old open
inode contents, ordinary metadata, unchanged out-of-range inode/mtime and no
stage leftovers. Exhaustive alphabet includes NUL/LF/CR/ASCII/invalid UTF-8;
seed 919420 supplements LF/CRLF/bare-CR/empty and 64 KiB boundary fixtures.
Negative controls are a no-op executable, omitted append separator, wrong line
position, borrowed argv parsing, omitted getopt restoration, omitted rename and
omitted final recheck. The last four use the real parser/module fault harness and
abort with defect-specific markers; the first three fail exact-byte assertions.

Existing fresh tests additionally cover real uid-1000 permission failures without
skips, UTF-8 JSON path escaping/rejection before effects, literal '-' and '--',
NAME_MAX, bounded 32 MiB input, parser/module allocation failures, every staging
fault/cleanup, observed external changes, inherited SIGPIPE and postcommit output
failure. Main verification includes exact 14-builtin/13-wrapper discovery and
help/parity, 32-entry registry capacity, repeated success/error/help dispatch and
a child-owned stdin/stdout PTY with exact prompts and bytes.

## Limits and delivery boundary

- Complete-file rename replaces the inode; ACL/xattr/timestamps/sparse layout are
  not preserved. Original read can update atime; parent metadata can change.
- Trusted stable namespace only: no locking, snapshot isolation, hostile-path
  safety or directory fsync; final check-to-rename race remains.
- JSON/output failure after commit cannot roll back. Inherited SIGPIPE applies.
- Installed internal argtable parser OOM terminates the host; constructor NULL
  and module-owned allocation failures are separately exercised/recoverable.
- O0 sanitizers passed; known inherited O1 shell fault-harness behavior was not
  retested. External dependency libraries are not sanitizer-instrumented.
- Six inherited formatting failures: src/cmd_spec.h, src/traverse.c,
  src/traverse.h, tests/test_help.c, tests/test_output.h, tests/test_recursive.c.
  Full raw logs preserve output whitespace; any raw-log whitespace exception is
  separate from source/documentation whitespace gates.
- Subagent HERMES_SESSION_ID was absent; no guessed registration was performed.
  The parent's existing mixed registration file was left untouched.

Reviewed delivery adds this report and its two evidence files to the original
15-path allowlist (18 paths total). README/FILES are narrow HEAD-based scratch
candidates, not mixed working files. The external delivery manifest identifies
all exact candidate bytes, anchors, reviewed patch and its hash. Fresh checkouts
prove candidate dependency closure; they do not claim an exact staged-index gate.
The parent must verify its exact final index before any separately authorized
commit. No remaining implementation blocker was found.
