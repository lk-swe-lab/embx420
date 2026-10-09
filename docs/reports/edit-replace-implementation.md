# edit-replace implementation

## Selected contract — before TDD

Autonomous authority covers this command, narrow integration/docs/tests/evidence;
no stage, commit, publication, wiki, AI_LOG or later command. Course SQLite queried
first: CLI_Tools.docx indexed paragraphs 109–115 require global regex replacement,
FILE PATTERN REPLACEMENT, -h/--help, -i, --fixed-strings and JSON counts.
This custom CLI has no GNU executable oracle. Independent byte fixtures rule.

Syntax: edit-replace [-h|--help] [-i] [--fixed-strings] [--json] [--]
FILE PATTERN REPLACEMENT. Exactly three operands; FILE nonempty, '-' literal.
Help permits missing operands but not unknown options/extra operands. Single-use
flags. Invalid CLI/regex/pattern or replacement limits/path encoding: status 2,
stderr diagnostic, no output/mutation. Operational or output failure: 1. Success,
including no matches and help: 0. Non-JSON success silent. Successful JSON exactly
{path,matches,replacements} plus LF, counts equal and include unchanged matches.
Operational file-attempt failure JSON additionally has status:error,message,
counts zero; parser/locale/regex setup allocation failures are status 1 without
JSON. A reported regex compiler/executor REG_ESPACE is operational, not syntax;
counts on errors are not partial progress reports. No JSON for argument errors.

Whole-file POSIX ERE, C thread-local locale, leftmost-longest nonoverlapping
matches. No REG_NEWLINE: dot/classes can consume LF; ^/$ mean file bounds.
At nonzero search offsets use REG_NOTBOL. Linux/BSD REG_STARTEND bounds
execution on the original buffer and avoids repeated suffix strlen scans;
this is an explicit non-POSIX API dependency, not a regex-language change.
NUL input remains rejected even though the extension could accept it. Replacement bytes are always literal:
&, backslashes, dollar and LF/CR receive no expansion. Pattern NUL and replacement
NUL cannot be argv bytes. Regex input containing NUL fails unchanged with 1;
fixed mode accepts all input bytes. Invalid UTF-8 content/pattern/replacement
are bytes, not decoded Unicode; -i uses ASCII case only, in both modes. JSON
requires UTF-8 FILE only. Regex backreference escapes (backslash + digit) are
rejected as outside selected ERE, even inside brackets; literal mode accepts them.
Other implementation-defined libc regex extensions are not portability promises.

Empty pattern matches all byte boundaries, including EOF/empty input. After a
zero-width match before EOF, emit replacement, retain one original byte and
advance by one byte. At EOF emit once and stop. After a nonempty match resume
at its end, allowing one subsequent empty match there. Never match replacement
text recursively. No-match preserves inode/mode/mtime/ctime and creates no stage;
reading may update atime. Matched identical replacements still commit new inode.

Explicit limits: input 8 MiB inclusive, pattern 1024 bytes, replacement 4096 bytes,
output 32 MiB inclusive. Oversized input/output fails with 1 unchanged; operand
limits fail with 2. Module storage bounded by input plus 64 KiB stage buffer;
match iterations bounded by input size+1. Libc regex has its own allocation/CPU
complexity: no hard timeout or hostile-pattern resource guarantee. Caller must
supply trusted bounded expressions or isolate the shell externally. No private
regex engine, subprocess or speculative shared edit framework.

Reuse reviewed single-line editor staging/parser mechanics and cmd_lines helpers.
Require regular singly-linked effective-user-owned file, ordinary permissions;
reject final symlinks/special modes/nonregular/foreign owners, follow parent links.
Require readable/writable source and writable parent for edits. Read/count/size
validate before stage creation. Exclusive same-directory stage, complete write,
group/mode restoration, successful fsync and close, recheck source/path identity,
size/mode/uid/gid/nlink/mtime/ctime before source close and rename. Preserve uid/gid
and ordinary mode, replace inode; existing open descriptors see old content.
Do not preserve timestamps, ACLs/xattrs or sparse layout. No source writes/truncate.
Precommit failures preserve source and remove stage unless diagnosed cleanup fails.
Observed external race changes cannot be rolled back. Trusted stable namespace:
final check-to-rename race, no directory fsync/locks/snapshots remain explicit.
Postcommit output failure does not undo committed edit. Inherit SIGPIPE. Preserve
borrowed argv bytes/pointers, descriptors/stdin offset, exported getopt globals,
cwd/environment/umask/signals and caller thread locale; not private getopt scan
state or concurrent parser safety. Internal argtable OOM may exit host; module
OOM and constructor NULL are separate recoverable seams.

Primary API basis: author-maintained Linux man-pages regex(3) and Open Group
POSIX.1-2024 regcomp (regcomp/regexec/free, flags, locale and string input).
Both primary pages were actually fetched; web_extract backend was unavailable.
Reviewed rg's C-locale and regex iteration plus prior editors' staging implementation.
No reference-executable compatibility claim. Deferred: backreference replacement,
multifile edits, backups, Unicode folding, line-local regex, embedded-NUL regex,
unbounded files and hostile-pattern execution guarantees.

## Acceptance plan

Vertical slices: literal nonoverlapping global replacement; regex wholefile
leftmost-longest; zero-width/empty advancement; case-insensitive interactions.
Independent exact-byte/property tests, buffer/size bounds, special operands,
CLI/help/JSON/error semantics, no-match forbidden effects, permissions/types/links,
metadata, fault/concurrency/cleanup/output checks, repeated direct module borrowed
state and locale. Real main inventory/discovery/help/dispatch/parity/capacity,
repeated loop and child-owned stdin/stdout PTY. Full make all test lint, scoped
format, known six inherited global format failures separately; fresh HEAD-based
narrow candidate patch normal and default O0 ASan/UBSan, intentional negative
controls with exact defect markers. Known O1 inherited shell harness not a gate.
ASan's regex interceptor performs internal_strlen on the entire string regardless
of REG_STARTEND (llvm-project sanitizer_common_interceptors.inc). Actual 1 MiB
empty-match instrumentation exceeded both 4s and a separate 180s probe, while
normal runtime was 0.11s. The instrumented repeated-regexec stress therefore uses
8 KiB; normal retains the real 1 MiB/4s acceptance guard. All byte/count/inode and
deadline assertions remain; other input/output size-limit fixtures are unchanged.
This is an explicit sanitizer-workload difference, not a production speed claim.

Evidence: [raw execution log](../../logs/commands/edit-replace-execution.log)
and [structured ledger](../../logs/commands/edit-replace-evidence.json).

## Executed results

| Gate | Observed result |
| --- | --- |
| Functional TDD | Four real RED/GREEN slices: literal global, wholefile ERE, zero-width advancement, ASCII folding. |
| Regex allocation TDD | Injected REG_ESPACE was wrongly status 2; reproducer failed; corrected status 1 passed. |
| Repeated regex resource TDD | Normal 1 MiB empty-match timed out at 4s; REG_STARTEND correction passed in 0.21s (two-pattern group). |
| Standalone and main CLI | 20 groups passed; 780 exhaustive literal byte cases and 180 seeded independently decidable ERE cases. |
| Direct module | 464 invocations passed: mixed regex/fixed/help/error calls, all owned allocations/descriptors balanced, nondefault C.UTF-8 host locale restored. |
| Main integration | 31 shell groups passed: 16 builtin inventory, 15 standalone parity, full 32-entry registry capacity, dispatch/help/status, repeated loop and child-owned stdin/stdout PTY exact prompts/bytes. |
| `make all test lint` | Full worktree regression passed; 56 C files analyzed with inherited suppressions unchanged. |
| Scoped clang-format | Owned C/header/wrapper/harness, registry and shell-module integration pass; 80-column C rule passes. |
| Global format-check | Exactly six inherited failing files, byte-identical to HEAD. |
| Six negative controls | Compiled argv/getopt/no-rename/no-recheck controls fail with intended markers; no-op executable and lost-zero-width-byte controls fail exact independent assertions. |
| Actual syscall trace | Same-directory O_EXCL stage fully written, synced and closed before rename; source never written/truncated; no-match creates no stage. |
| Preliminary fresh HEAD+narrow patch | `make all test lint`, full main CLI matrix and scoped format pass without dirty-working-tree dependencies. |
| Preliminary fresh default O0 ASan/UBSan | Full `make all test` and main CLI matrix pass with leak/halt-on-error. Repeated-regexec stress is explicitly 8 KiB under ASan, normal remains 1 MiB/4s. |

Exact frozen final candidate re-execution is stored externally in the handoff
manifest/proof directory below; this avoids a self-referential log/ledger hash.
These are implementation gates, not independent review or exact-index verification.

Filesystem checks cover LF/CRLF/bare CR/unterminated/empty/binary bytes, invalid
UTF-8 patterns/replacement/paths, literal replacement metacharacters, newline
patterns, nonoverlap/leftmost-longest, start/end anchors, empty patterns and EOF,
ASCII folding versus non-ASCII bytes, fixed/regex interactions, identical
replacements and no-match status/inode/mtime/ctime. Input 8 MiB and output 32 MiB
inclusive boundaries, +1 rejection, all operand byte limits, 64 KiB stage edges,
NAME_MAX basename and an 8 MiB edit under 24 MiB address-space limit pass.
ASan alone disables that address-space cap for shadow-memory reservation.

Mode/uid/gid and new inode are checked; old open descriptors retain old bytes,
inode and xattr, while replacement drops xattr/old timestamps as documented.
Final symlinks, hardlinks, directories, FIFO, Unix socket, special mode and
permission errors fail unchanged; parent symlinks are followed. Permission cases
run as uid 1000, not root-skipped. Locked-parent no-match succeeds with no stage.
Both committed and no-match result-output failure behavior are independently
checked; JSON path escaping and argument/setup/file-attempt failure shapes agree.

Module faults exercise all eight parser constructor NULLs, all three module
malloc sites, open/lstat/fstat/temp/fcntl/read/write/fchown/fchmod/fsync/close/
rename/unlink and regex/locale allocation errors; interrupted/short/zero transfers,
late stat/close failures, observed content/path/mode/link/owner/size/mtime/ctime
changes, diagnosed cleanup leftovers and postcommit stdout/ignored-SIGPIPE errors.
No-match arms a temp-creation failure that must remain unconsumed. Preserved
borrowed argv pointers/string bytes are tested with a real parser plus intentional
pointer permutation; getopt sentinels, inherited stdin offset/fds, cwd/HOME/umask/
SIGPIPE and nondefault caller thread locale are checked. Internal argtable OOM
was actually probed in a child and exits host status 1; it is not recoverable
module-owned OOM. Third-party libraries themselves remain uninstrumented.

Inherited formatting failures: src/cmd_spec.h, src/traverse.c, src/traverse.h,
tests/test_help.c, tests/test_output.h, tests/test_recursive.c. Known inherited
O1 shell fault-harness failure was not rerun; no optimized-sanitizer claim.
Raw log retains real compiler/terminal trailing whitespace; only this exact raw
log path is exempted from candidate whitespace checks, not source/docs.

Observed setup/iteration failures are retained, not represented as passes:
one overbroad Makefile replacement briefly altered the preceding command recipe
and was narrowly corrected before regression; a stale overwrite was refused;
one scratch-script syntax error had no file effects; help expectation initially
missed argtable's combined [-hi] grammar; parity initially reused a mutated fixture
and was fixed by restoring independent input before each run; trace parsing first
excluded relative rename records; a preliminary fingerprint compared a live
appending evidence log to its earlier snapshot and was corrected. None required
weakening production assertions. Initial 1 MiB ASan stress timeout and 180s probe
are retained alongside its primary-source diagnosis and explicit workload change.

## Review handoff

Base HEAD: 705eee07b7b45d3f97f26f9c4e90b6cec2dc21e3. No staging, commit,
publication, wiki/AI_LOG access or later command. Session startup registration
20261009_010901_1860b4 (verified current title) is separate and excluded from the
command candidate; all existing registrations remain intact. Unrelated tracked
dirty/deleted paths are fingerprinted and preserved. Mixed README/FILES working
files are untouched: use only narrow HEAD-based candidates.

Exact 16-path allowlist, candidate hashes, unchanged source-anchor and installed
argtable3/CUnit dependency hashes, README/FILES candidates and integration.patch:
/home/name/.hermes/cache/scratch/edit-replace-integration/manifest.json.
Reproducible frozen patch verifier is candidate.py in that directory. Final proof
streams/statuses are final-proof.log/final-proof.json there, separate from the
frozen deliverable. Ignored .tmp/EDIT_REPLACE_HANDOFF.md gives concise actual
results and patch hash. Parent owns independent review, exact-index verification
and any separately authorized commit. No mv work started.

Remaining limits: literal replacement only; trusted paths/patterns; Linux/BSD
REG_STARTEND dependency; libc regex has no hard hostile-pattern CPU/memory cap;
final check-to-rename race; no directory fsync/locks/metadata preservation beyond
ordinary mode/uid/gid; internal argtable OOM can terminate host; postcommit output
failure cannot roll back. The sanitizer stress difference above is explicit.
