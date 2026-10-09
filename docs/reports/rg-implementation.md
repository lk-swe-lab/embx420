# rg implementation

## Selected contract (before tests)

Course: read-only docs/data.sqlite lecture_content/lecture_sources query,
CLI_Tools.docx paragraphs 77–87: regex, help, -n, -i, -w, -C N,
--fixed-strings, --json objects per match (file, line, column, text).
Reference: installed ripgrep 15.2.0 e89fff89ac, `rg --help` (no installed
man entry). Primary upstream GUIDE and POSIX regcomp/regexec; installed
`man -P cat 3 regcomp` was also inspected.
Installed verbose help is preserved with the evidence, including the complete
option inventory. Keep only the course flags and explicit paths; all other
reference flags, aliases, modes, encodings, filters, glob/ignore handling,
multiline, PCRE2, configuration, parallelism and replacement are deferred.

Syntax: `rg [-h|--help] [-n] [-i] [-w] [-C N] [--fixed-strings]
[--json] [--] PATTERN PATH...`. One pattern, at least one explicit path;
no implicit cwd, stdin, glob expansion or external process execution. '-' is
literal. Unknown/repeated options or malformed arguments fail with status 2;
valid help needs no operands. Parse/compile before opening input.

Regex is line-local POSIX extended, leftmost-longest (not Rust leftmost-first).
Use a temporary thread-local C locale, restoring caller locale. ASCII case
folding and word boundaries (A–Z/a–z/0–9/underscore), byte matching including
UTF-8 bytes. No Unicode case folding, lookaround, lazy quantifiers or promised
non-POSIX escapes/extensions. Fixed strings match literal bytes. Reject LF in
pattern; CR is data. Empty patterns supported. Matches are non-overlapping,
ordered left-to-right. A rejected whole-word candidate advances one byte from
its start. After an accepted empty match advance one byte; include end-of-line
empty match once. No phantom line after terminal LF. Empty file has no lines.

Text output always prefixes literal path; -n adds 1-based line number. Matching
lines printed once (colon separators); context lines use hyphens. Context is
union of N-line windows, adjacent/overlapping windows merge; separated groups
within a file use `--\n`, no separator across files. Output lines end LF even
when input's final line does not. JSON is NDJSON, one object per individual
match: file literal path, line 1-based, column 1-based byte offset in line,
index 0-based absolute byte offset in file, text full line excluding LF.
JSON ignores -n and context (no context events). Require UTF-8 path and entire
nonbinary file in JSON; invalid encoding fails before that file emits output.

Explicit regular files and directories only. Reject explicit symlinks and
special files; skip discovered links/special files. Do not follow final links;
ancestor symlink resolution and concurrent replacement are not hardened sandbox
boundaries. Recursion includes hidden names and ignores ignore files: the caller
must select safe roots. Depth limit 64 directories; path-length/kernel errors
are reported. Operands processed in order, duplicates retained; children sorted
by bytewise strcmp, depth-first. Files containing NUL anywhere are silently
skipped, even if an earlier line matched. Whole-file buffering enables that
policy/context; 64 MiB/file cap (larger files fail, no output from that file).
Memory proportional to one file plus its line table and current directory
names; no streaming/linear-time regex claim. Read-only: bytes/mode/mtime/link
identity unchanged; reading/enumeration may update atime. No concurrent-write
snapshot promise.

Status 0 matches (or help), 1 no matches, 2 any argument/regex/I/O/allocation/
encoding/traversal/output error. Continue independent paths after input errors;
stop on output error. Earlier successful output may remain on any later error.
Diagnostics stderr, never JSON error records. Borrow stdin/stdout; preserve argv,
exported getopt values, cwd/environment/signals/thread locale. Argtable internal
fatal OOM is a dependency limitation, not recoverable module allocation.

## Ordered TDD / acceptance

Vertical slices: explicit-file ERE/-n and statuses; fixed/-i/-w; JSON individual
matches and empty-match advancement; context union; recursive/binary/type/error
policy; module reuse/fault cleanup; main discovery/parity/count/capacity.
Independent byte expectations, installed reference only on agreed common subset,
negative control executable, malformed regex/options, UTF-8 and escaping,
whole-word alternatives, anchors, long lines, empty/binary files, paths, output
failure, deterministic injected read/write/allocation/directory failures,
repeated invocation/descriptor/resource/no-modifications checks.

## Execution and acceptance evidence

Selected raw commands, complete execution stdout/stderr and exit codes:
[rg-evidence.json](../../logs/commands/rg-evidence.json). Research document
bodies are kept in scratch rather than republishing third-party documents;
successful duplicate/intermediate traces are summarized rather than repeated.
The durable ledger retains fetch command/status, byte count and SHA-256.
Installed verbose help is retained (MIT-licensed ripgrep). Three reference
case groups pass; seven project-only groups are intentionally skipped there.

| Behavior | Actual RED observation | Accumulated GREEN |
| --- | --- | --- |
| explicit-file ERE/-n/status | Runnable baseline returned 1/empty instead of matches | slice1-green |
| literal/-i/-w | unknown -i/-w returned 2 | slice2-green |
| individual JSON/empty-match advancement | unknown --json | slice3-green |
| merged/separated context windows | unknown -C | slice4-green |
| recursive/order/binary/link policy | tree rejected with EINVAL | slice5-green |
| JSON UTF-8 preflight | invalid later UTF-8 still emitted earlier match | slice6-green |
| strict decimal context | argtable integer accepted 0x1 | decimal-context-green |
| actual main integration | rg unknown (127), discovery/parity assertions failed | main-integration-green |
| help output diagnostics | /dev/full returned 2 without stderr | help-output-regression-green |

Expanded existing-behavior checks are characterization, not claimed new-feature
RED: exact byte/JSON boundaries, malformed expressions, 64 MiB cap edge,
65-level recursion refusal, 120 seeded independent fixed-string properties,
500 module calls, borrowed descriptors/cwd/argv/exported getopt preservation,
nondefault C.utf8 caller locale restoration, and real RLIMIT_NOFILE exhaustion.
Constructor NULL and argument-copy allocation failures are exercised separately.
Post-parse module-owned allocation sweeps hit all five file and 103 tree
allocation positions. Narrow linker seams inject mid-read EIO, short/EINTR
reads/writes, zero/error output, open/stat/close, fdopendir, mid-readdir,
closedir and regex-execution errors. Exact captured bytes prove short/EINTR
recovery; FD counts and default leak sanitizers check cleanup. Read-only open
assertions plus byte/mode/mtime/inode and link/directory checks protect fixtures.
No rg test searches the repository/home implicitly or outside disposable roots.

Test strength: a wrong executable (true) fails expected stdout; scratch borrowed
argv and missing-getopt-restore mutants both compile and abort at the intended
caller-invariant assertions (status 134). The parser seam actually permutes
its received pointer array, avoiding a false negative from an otherwise
non-mutating installed parser. No production source was mutated for controls.

The initial /usr/bin/rg reference location was a setup error, not RED; corrected
using command -v rg. Initial lint found an open-varargs harness diagnostic;
fixture creation now uses the real-open seam and wrapped opens explicitly
forbid O_CREAT. No analyzer suppression was added. Subsequent full lint passes.

### Final gates

- `make test-rg-impl test-shell`: PASS; 10 rg CLI groups, 120 seeded cases,
  module ownership/fault sweeps; 23 shell integration groups and capacity test.
- `make test`: PASS in working tree and a fresh HEAD-plus-allowlist checkout.
- Fresh `make all`: PASS; all twelve builtins / eleven standalone wrappers.
- Fresh `make lint`: PASS (existing non-user/NOLINT notices remain).
- Scoped `clang-format --dry-run --Werror` on six touched C/header files: PASS.
- Global `make format-check`: FAIL only on inherited src/traverse.c/.h,
  src/cmd_spec.h, tests/test_help.c, tests/test_recursive.c, tests/test_output.h.
  Those six files were not changed.
- Fresh default -O0 `make all test` with AddressSanitizer + UndefinedBehaviorSanitizer,
  leak checking and halt-on-error: PASS; non-PIE sanitizer executables, installed
  dependency libraries uninstrumented. Final test update reran the full sanitized
  regression successfully. No -O1 sanitizer claim (inherited shell harness limit).
- Narrow `git diff --check`: PASS; index remained empty and HEAD unchanged.

Snapshots use installed argtable3/CUnit dependency symlinks only, not working
binaries or untracked source dependencies. Raw ledger records exact build flags
and snapshot paths. These are proposed-delivery checks, not verification of a
staged/committed tree; parent must review and verify the exact index before commit.

### Limits / integration ownership

64 MiB/file and 64 directory levels are deliberate functional limits, not full
ripgrep scalability. Temporary file memory and line tables can exhaust memory;
module-owned failures return 2. Fatal argtable-internal OOM remains an inherited
library limit, not covered by the recoverable module-allocation guarantee.
No thread-safety or interrupted-private-getopt-state restoration is promised.
Final parser characterization: argtable accepts unambiguous prefixes of the
supported long names (for example --he); this is inherited parser behavior,
not additional advertised ripgrep aliases.
C-locale leftmost-longest candidates are checked for ASCII word boundaries;
rejected candidates resume at start+1, without backtracking alternate endings
at the same start. UTF-8 empty-match byte columns may lie inside a code point.
The caller's signal dispositions remain unchanged; default SIGPIPE may terminate
instead of returning an output error (ignored-SIGPIPE EPIPE is tested).
No regex performance bound, hard-link deduplication or concurrent filesystem
snapshot/sandbox guarantee. Earlier output may remain after later errors.

Scope: rg module/header/wrapper, Makefile, registry, rg CLI/module tests, shell
inventory/count/parity tests, concise rg usage/report/evidence, narrow README/FILES
updates. Working README/FILES retain unrelated edits. HEAD-only alternatives and
their patch are in .tmp/rg-{README,FILES}.index.md / rg-mixed-proposal.patch.
The exact delivery allowlist is in .tmp/rg-delivery-allowlist.txt; sessions.txt
registration is incidental startup state and excluded from that proposal.
No staging/commit/push/wiki/AI_LOG access or next-command work. Parent owns
independent review, final exact-index checks, commit and restart handoff.

## Reference inventory

Primary references inspected: upstream ripgrep GUIDE.md at
https://raw.githubusercontent.com/BurntSushi/ripgrep/master/GUIDE.md,
POSIX regexec at
https://pubs.opengroup.org/onlinepubs/9799919799/functions/regexec.html,
and installed regcomp(3). Installed rg --help provides all 104 option headings
below; negating spellings and secondary aliases/interactions are preserved in
the full help evidence, not promised by this subset. All DEFER rows are outside
the course-required explicit-path read-only search workflow. Even retained
heading aliases are unsupported unless named in the contract.

Reference operands: optional path defaults cwd or redirected stdin; patterns
can also come from -e/-f. Deferred to avoid implicit reads and extra pattern
sources. Reference defaults use Unicode regex, conditional TTY numbers/headings,
ignore/hidden filtering, parallel unsorted traversal and heuristic binary output.
Selected replacements are byte ERE, stable path-prefix output, sorted traversal,
explicit roots, no ignore/config/TTY modes and entire-file NUL skipping.
Reference JSON begin/match/context/end/summary events and base64 byte fields
are replaced by course-oriented per-match full-line records. Reference ordinary
statuses 0/1/2 are retained; quiet-mode exceptions are deferred. Stderr input
errors may accompany prior stdout; no successful search modifies input bytes.

| Installed heading | Selection |
| --- | --- |
| `-e PATTERN, --regexp=PATTERN` | DEFER |
| `-f PATTERNFILE, --file=PATTERNFILE` | DEFER |
| `--pre=COMMAND` | DEFER |
| `--pre-glob=GLOB` | DEFER |
| `-z, --search-zip` | DEFER |
| `-s, --case-sensitive` | DEFER |
| `--crlf` | DEFER |
| `--dfa-size-limit=NUM+SUFFIX?` | DEFER |
| `-E ENCODING, --encoding=ENCODING` | DEFER |
| `--engine=ENGINE` | DEFER |
| `-F, --fixed-strings` | KEEP --fixed-strings only |
| `-i, --ignore-case` | KEEP -i only |
| `-v, --invert-match` | DEFER |
| `-x, --line-regexp` | DEFER |
| `-m NUM, --max-count=NUM` | DEFER |
| `--mmap` | DEFER |
| `-U, --multiline` | DEFER |
| `--multiline-dotall` | DEFER |
| `--no-unicode` | DEFER |
| `--null-data` | DEFER |
| `-P, --pcre2` | DEFER |
| `--regex-size-limit=NUM+SUFFIX?` | DEFER |
| `-S, --smart-case` | DEFER |
| `--stop-on-nonmatch` | DEFER |
| `-a, --text` | DEFER |
| `-j NUM, --threads=NUM` | DEFER |
| `-w, --word-regexp` | KEEP -w only |
| `--auto-hybrid-regex` | DEFER |
| `--no-pcre2-unicode` | DEFER |
| `--binary` | DEFER |
| `-L, --follow` | DEFER |
| `-g GLOB, --glob=GLOB` | DEFER |
| `--glob-case-insensitive` | DEFER |
| `-., --hidden` | DEFER |
| `--iglob=GLOB` | DEFER |
| `--ignore-file=PATH` | DEFER |
| `--ignore-file-case-insensitive` | DEFER |
| `-d NUM, --max-depth=NUM` | DEFER |
| `--max-filesize=NUM+SUFFIX?` | DEFER |
| `--no-ignore` | DEFER |
| `--no-ignore-dot` | DEFER |
| `--no-ignore-exclude` | DEFER |
| `--no-ignore-files` | DEFER |
| `--no-ignore-global` | DEFER |
| `--no-ignore-parent` | DEFER |
| `--no-ignore-vcs` | DEFER |
| `--no-require-git` | DEFER |
| `--one-file-system` | DEFER |
| `-t TYPE, --type=TYPE` | DEFER |
| `-T TYPE, --type-not=TYPE` | DEFER |
| `--type-add=TYPESPEC` | DEFER |
| `--type-clear=TYPE` | DEFER |
| `-u, --unrestricted` | DEFER |
| `-A NUM, --after-context=NUM` | DEFER |
| `-B NUM, --before-context=NUM` | DEFER |
| `--block-buffered` | DEFER |
| `-b, --byte-offset` | DEFER |
| `--color=WHEN` | DEFER |
| `--colors=COLOR_SPEC` | DEFER |
| `--column` | DEFER |
| `-C NUM, --context=NUM` | KEEP -C only |
| `--context-separator=SEPARATOR` | DEFER |
| `--field-context-separator=SEPARATOR` | DEFER |
| `--field-match-separator=SEPARATOR` | DEFER |
| `--heading` | DEFER |
| `-h, --help` | KEEP both |
| `--hostname-bin=COMMAND` | DEFER |
| `--hyperlink-format=FORMAT` | DEFER |
| `--include-zero` | DEFER |
| `--line-buffered` | DEFER |
| `-n, --line-number` | KEEP -n only |
| `-N, --no-line-number` | DEFER |
| `-M NUM, --max-columns=NUM` | DEFER |
| `--max-columns-preview` | DEFER |
| `-0, --null` | DEFER |
| `-o, --only-matching` | DEFER |
| `--path-separator=SEPARATOR` | DEFER |
| `--passthru` | DEFER |
| `-p, --pretty` | DEFER |
| `-q, --quiet` | DEFER |
| `-r REPLACEMENT, --replace=REPLACEMENT` | DEFER |
| `--sort=SORTBY` | DEFER |
| `--sortr=SORTBY` | DEFER |
| `--trim` | DEFER |
| `--vimgrep` | DEFER |
| `-H, --with-filename` | DEFER |
| `-I, --no-filename` | DEFER |
| `--sort-files` | DEFER |
| `-c, --count` | DEFER |
| `--count-matches` | DEFER |
| `-l, --files-with-matches` | DEFER |
| `--files-without-match` | DEFER |
| `--json` | KEEP course schema, not reference events |
| `--debug` | DEFER |
| `--no-ignore-messages` | DEFER |
| `--no-messages` | DEFER |
| `--stats` | DEFER |
| `--trace` | DEFER |
| `--files` | DEFER |
| `--generate=KIND` | DEFER |
| `--no-config` | DEFER |
| `--pcre2-version` | DEFER |
| `--type-list` | DEFER |
| `-V, --version` | DEFER |

