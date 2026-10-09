# ls: inventory, contract, and report

Status: independently reviewed 2026-10-08; isolated ls index prepared.
No commit or publication; staged-tree gates recorded below.

Manual: installed `man 1 ls`, GNU coreutils 9.4, August 2026 footer,
rendered on 2026-10-07. Course source: `data/02/CLI_Tools.docx`,
queried through `docs/data.sqlite`. Selection uses the user's autonomous
continuation authorization. Author/copyright/support sections are provenance.
Use argtable3 and cmd_spec_t, standalone wrapper, status 0 success,
1 operational failure, 2 invalid arguments. Parse fully before side effects.
Keep human -h/--help and course --json separate from GNU reference tests.
No shell registry exists. No commits or pushes.

Course paragraphs 18–23: listing, -a, -l, JSON entries.
Purpose: discover files and inspect metadata for human and AI workflows.

| Manual feature | Decision and reason |
| --- | --- |
| FILE operands, current directory default | Keep one optional operand; defer multi-operand headers |
| Alphabetic name ordering | Keep deterministic byte ordering; GNU oracle uses LC_ALL=C |
| -a/--all, including . and .. | Keep course-required hidden listing |
| -l | Keep permissions, owner, size, timestamps; stable project layout |
| --help | Keep; course -h means help, not GNU human-readable sizes |
| -1 | Keep explicit one-entry-per-line default |
| -A/--almost-all, -B/--ignore-backups, --hide=PATTERN, -I/--ignore | Defer alternate filters |
| --author, -g, -G/--no-group, -o, -n/--numeric-uid-gid | Defer alternate long columns; project always uses numeric IDs |
| -b/--escape, -N/--literal, -q/--hide-control-chars, --show-control-chars, -Q/--quote-name, --quoting-style, QUOTING_STYLE | Defer switches; raw names in text, escaped UTF-8 JSON |
| --block-size, -h/--human-readable, --si, -k/--kibibytes, -s/--size, SIZE suffixes | Defer scaled sizes/blocks; bytes in long/JSON output |
| -c, -u, -t, -S, -U, -v, -X, --sort, -r/--reverse, --group-directories-first | Defer alternate sorting |
| -C, -m, -x, --format, -T/--tabsize, -w/--width | Defer terminal layout; one line per entry |
| --color[=WHEN], LS_COLORS, --hyperlink[=WHEN], WHEN auto/always/never | Defer terminal decoration |
| -F/--classify[=WHEN], --file-type, -p/--indicator-style=slash, --indicator-style | Defer suffix decoration |
| -f (all entries in directory order) | Defer alternate order/filter combination |
| -d/--directory, -R/--recursive | Defer directory-self and recursive traversal |
| -H/--dereference-command-line, --dereference-command-line-symlink-to-dir, -L/--dereference | Defer selectable dereference policy; inspect links themselves |
| --full-time, --time, --time-style, TIME_STYLE and custom date formats | Defer date-language support; epoch seconds in project metadata |
| -D/--dired, -i/--inode, -Z/--context, --zero, --version | Defer editor/security/alternate fields and delimiters |
| GNU status 1 minor / 2 serious failures | Deliberate difference: project operational1 / argument2 |
| --json (course addition) | Keep stable entry array |

Contract: `ls [-a] [-l] [-1] [--json] [FILE]`; directory contents or a single
non-directory entry. Symlink operands/entries describe the link without
following the final link. Intermediate links and trailing slashes retain native
pathname resolution (`link/` lists its directory target). Byte-sorted names;
-a includes dot entries. A single file's name preserves the operand spelling;
directory names are basenames. `-` is literal; `--` ends option parsing.
Each flag is accepted at most once. Text default emits
names plus LF. Long rows use permissions, numeric uid, numeric gid, byte size,
epoch mtime, name, separated by tabs; no totals or arrow suffix. JSON array
contains name,type,size,mtime (integer timestamps). Validate all JSON names
before output. --json determines format if combined with -l.
Use lstat/fstatat, directory iteration, dynamic storage; no fixed PATH_MAX.
Errors must be reported without fabricated metadata. Collect all selected names
and metadata before output: lookup, iteration/close, allocation, and encoding
failures return 1 with stderr and no listing. Output errors return 1 and may
leave partial bytes/JSON. Invalid syntax (including extra operands) takes
precedence over help and returns 2 without lookup. Valid help skips lookup,
prints to stdout, and checks flush errors. Module argv must have argc >= 1 and
non-NULL argv/elements; malformed arguments return 2 before parsing.

JSON is an array plus LF (`[]` when empty), with exactly name (UTF-8 string),
type (regular/directory/symlink/fifo/socket/character/block/unknown), size
(integer bytes), and mtime (integer epoch seconds). Shared cmd_lines validates
UTF-8 and escapes JSON punctuation/controls. Only selected names are validated.
JSON wins over -l regardless of order. Long permissions include special bits;
uid/gid are numeric. Raw text filenames may contain LF/tab: intentionally not
an unambiguous text protocol.

Do not read file contents, write filesystem objects, change cwd, close caller
FDs, or alter signal dispositions. Directory enumeration may update directory
atime (an unavoidable filesystem effect). Parser tables, copied argv pointers,
directory streams, and listing storage are invocation-owned. No real registry
exists: expose cmd_ls_spec without a fabricated registration shim.

Tests first: literal names, hidden/dot entries, byte order, default cwd,
single file, symlink/dangling link, empty directory, long metadata, JSON Unicode
and invalid UTF-8, missing input, extra operands, help, output failure,
repeated module calls and unchanged cwd. GNU reference for default/-a with
-1 and explicit literal quoting; project long layout uses metadata expectations,
not GNU's human default. Record reference adapter precisely.

## Results

### Research checked

Read-only docs/data.sqlite lecture_sources/lecture_content query for
CLI_Tools.docx paragraphs 16–25: 20–23 require help, hidden, long, JSON.
The indexed sentence ends `with ..`; interpreted as dot-prefix listing,
consistent with GNU -a (the punctuation interpretation is inferred). The installed man 1 ls inventory was
re-rendered via `MANWIDTH=100 man 1 ls | col -b`: coreutils 9.4, August 2026
footer. Added its previously omitted -f row. Installed readdir(3)/fstatat(2)
manuals: Linux man-pages 6.7, 2023-10-31; checked errno-vs-EOF, dirent ownership,
dirfd-relative lookup, and AT_SYMLINK_NOFOLLOW.

The configured web extractor cannot extract URLs (search-only backend).
Direct HTTPS fetches succeeded for these primary references:
- https://www.gnu.org/software/coreutils/manual/html_node/ls-invocation.html
  (live manual says Coreutils 9.12; not the installed reference binary).
- https://pubs.opengroup.org/onlinepubs/9799919799/functions/readdir.html
- https://pubs.opengroup.org/onlinepubs/9799919799/functions/fstatat.html
- https://www.argtable.org/tutorial/
HTML copies: `/home/name/.hermes/cache/scratch/ls-reference-{0,1,2,3}.html`.

### Observed TDD

| Slice | Actual RED | Accumulated GREEN |
| --- | --- | --- |
| Raw names | Runnable compiled baseline returned empty bytes instead of A/file10/file2/z/é listing | default cwd, directory, empty, single-file names |
| Hidden | -a rejected with status 2 instead of 0 | -a/--all, -1, dangling links, bare links, link/ |
| Long | -l rejected with status 2 | independent lstat/stat.filemode rows and link/FIFO metadata |
| JSON | --json rejected with status 2 rather than success/encoding1 | JSON schema, escaping, precedence, encoding rejection |
| Module argv | SIGSEGV on argv[1]==NULL; GDB traced getopt_internal -> arg_parse -> ls_run -> test assertion | reject NULL elements before parsing; complete reuse suite passes |

Long's first build failed because S_ISVTX was hidden; `_XOPEN_SOURCE=700`
resolved it. Extended checks of already implemented behavior passed immediately;
they are not claimed as new RED cycles. GNU 9.4 passed independent raw-name
fixtures before implementation; `/usr/bin/true` failed the same byte assertion.

GNU adapter: `-1 --quoting-style=literal --color=never`, LC_ALL=C. Expectations
are fixed bytes or independent Python metadata, never generated from GNU output.
Displays are bounded; comparisons use complete bytes. GNU-compatible suite:
four executed methods and eight intentional project-only skips. Project CLI:
12 executed methods, no skips on this non-root host.

### Acceptance evidence

| Requirement | Passing check |
| --- | --- |
| cwd/name/order/empty/file | test_names; test_many_and_unusual_names |
| hidden/symlinks | test_hidden_and_symlinks |
| parser/help/lookup/status | test_arguments_and_lookup; test_permission_failure |
| long/special bits/large size/negative mtime | test_long_metadata; test_json_all_and_special_modes |
| JSON/schema/precedence/encoding | test_json; test_binary_names |
| types/generated names/no mutation | test_json_types_and_no_mutations |
| dirfd lookup without joined PATH_MAX buffer | test_directory_relative_lookup (32 levels, 240-byte name) |
| real output failures | test_output_failure (/dev/full and default/ignored SIGPIPE) |
| repeated module/resource/argv invariants | test_ls_module: 100 mixed calls plus error/recovery calls |
| failures/cleanup/no partial listing | injected arg_lit0/realloc/strdup/readdir/fstatat/closedir failures |
| interrupted/short/zero writes | injected write faults through unchanged cmd_lines |

Module checks borrowed stdin position and open stdio FDs, unchanged cwd/argv,
SIGPIPE handler, caller-selected usage stream, no stale parser state, and
help/raw/long/JSON output-error recovery. Close injection releases the real
stream before reporting EIO; not a kernel close-failure simulation. JSON
invariant fixtures retain inode/mode/size/mtime/ctime/atime and symlink targets.

### Executions and logs

Log prefix: `/home/name/.hermes/cache/scratch/`.

- `make test-ls-impl && make test-ls`: exit 0, `ls-final-targeted.log`.
- `make test && make -B all`: exit 0, `ls-final-regression.log`.
- `make lint && clang-format --dry-run --Werror src/cmd_ls.c src/cmd_ls.h
  src/ls_main.c tests/test_ls_module.c`: exit 0, `ls-final-quality.log`.
- `python3 tests/test_ls.py /usr/bin/true --reference LsTests.test_names`:
  expected exit 1 / empty-output assertion, `ls-negative-control.log`.
- `make format-check`: exit 2, `ls-full-format.log`, inherited failures only:
  src/main.c, src/traverse.c, src/traverse.h, src/cmd_spec.h, tests/test_help.c,
  tests/test_recursive.c, tests/test_output.h. Those were not changed here.
- GCC 13.3, ASan+UBSan, `-O1 -g3 -fno-omit-frame-pointer
  -fsanitize=address,undefined`, same module linker wrappers, then all 12 CLI
  tests, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`: exit 0, no sanitizer
  diagnostics. `ls-final-sanitizers.log` records exact compile/run commands.
  Installed static argtable library was not rebuilt with instrumentation.
- RED/GREEN logs: `ls-hidden-red.log`, `ls-hidden-green.log`, `ls-long-red.log`,
  `ls-long-green.log` (feature-macro build failure), `ls-long-green-final.log`,
  `ls-json-red.log`, `ls-json-green.log`, `ls-module-red.log`,
  `ls-module-green.log`. Initial baseline RED is recorded in session output.

### Limits and readiness

Implementation-worker handoff: no stage, commit, push, AI_LOG or wiki edits.
No mkdir implementation. Session lookup verified 20261008_154426_3977fb,
title `Subagent: Implement ls only, following…`; registration left to the
coordinator under the ls-only edit scope (docs/sessions.txt unchanged here).

Memory O(entries + name bytes), no atomic filesystem snapshot or adversarial
concurrent-rename guarantee (including lstat/opendir interval). Sequential
reuse is verified; concurrent calls sharing streams/getopt state are not.
Block-device/unknown-type fixtures were not constructed; privileged device
creation is out of scope. Parser/growth/name allocation faults were injected,
not every dependency allocation point. Bounded generated filename/metadata
checks and sanitizers were run, not exhaustive fuzzing or coverage measurement.
Inherited global format failure remains separate from scoped ls readiness.

## Independent review, 2026-10-08

No blocking implementation defect reproduced. Reviewed parser/help sharing,
copied argv ownership, partial allocation cleanup, collection-before-output,
errno-vs-EOF, close-error preservation, byte sorting, final-link metadata,
special permission bits, numeric width/sign, UTF-8 validation, JSON escaping,
stdout failures, and module reuse. No source/header/wrapper changes were needed.

Coverage additions in tests/test_ls.py passed immediately; they characterize
existing behavior and are not new RED/GREEN fixes:
- test_parser_repetition_help_and_placement: repeated flags/aliases reject
  with status 2; option placement and help aliases agree with the glossary.
- test_utf8_scalar_boundaries: nine valid boundary names, twelve malformed
  UTF-8 forms; reject before JSON output, preserve raw-byte operand output.
- test_no_mutations_all_modes: directory names and metadata remain unchanged
  for raw/all/long/JSON/help and lookup/parser failures; only enumerated
  directory atime is excluded. File bytes and link target also remain intact.

A scratch-only mutant bypassed JSON UTF-8 validation. The new scalar test
rejected it with the intended `0 != 1` status assertion (exit 1); no repository
source was modified. Evidence: ls-review-negative.py and ls-review-negative.log.

Review logs live under /home/name/.hermes/cache/scratch/:
- make test-ls-impl: exit 0, ls-review-targeted.log (15 CLI methods, no skips,
  plus module fault/reuse assertions).
- make test-ls: exit 0, ls-review-reference.log (four compatible methods,
  eleven deliberate project-only skips).
- make test && make -B all: exit 0, ls-review-regression.log.
- make lint && clang-format --dry-run --Werror src/cmd_ls.c src/cmd_ls.h
  src/ls_main.c tests/test_ls_module.c: exit 0, ls-review-quality.log.
- ASan+UBSan compile and complete 15-method CLI/module runs: exit 0, no
  diagnostics; ls-review-sanitizers.log contains exact commands. Static
  argtable remains uninstrumented, as in the original worker verification.
- Primary POSIX readdir/fstatat references independently fetched successfully;
  copies are ls-review-readdir.html and ls-review-fstatat.html.

Index scope: FILES.md, Makefile, docs/commands/ls.md, src/cmd_ls.c,
src/cmd_ls.h, src/ls_main.c, tests/test_ls.py, tests/test_ls_module.c.
Mixed Makefile and FILES.md working bytes are preserved. Staged Makefile is
HEAD plus only ls build/test/clean integration; staged FILES.md is HEAD plus
five ls-specific registry entries. Untracked cat/head/tail/pwd implementations
are not required or included. No AI_LOG/wiki edits, commit, or publication.
Session lookup verified 20261008_155953_c153f8, title
`Subagent: Independently review ls implementation, tests…`; registered in
working docs/sessions.txt per AGENTS.md, deliberately excluded from the index.

Isolated staged-tree execution used git checkout-index in scratch, with only
installed build/argtable3 and build/cunit dependency directories symlinked.
All extracted index blobs were checked; older untracked command sources were
absent. Exact paths and tree hash are in ls-review-staged-results.json.
- make test && make test-ls: exit 0, ls-review-staged-tests.log.
- make -B all: exit 0, ls-review-staged-build.log.
- make lint: exit 0, ls-review-staged-lint.log.
- clang-format --dry-run --Werror src/cmd_ls.c src/cmd_ls.h src/ls_main.c
  tests/test_ls_module.c: exit 0, ls-review-staged-format.log.
- git diff --cached --check: passes; added-line secret/injection/unsafe-eval
  scan has no matches. Final staged diff is ls-review-index.diff.

The first index check caught the report's extra final blank line; it was
removed before isolated-tree execution. Rechecked make format-check: exit 2,
ls-review-global-format.log, only the same seven inherited paths listed above.
Existing limits still apply: no atomic concurrent-rename snapshot, no threaded
getopt/stream reuse guarantee, no privileged block-device/unknown-type fixture,
no exhaustive fuzzing/coverage measurement or every-allocation injection.
Global formatting remains an inherited exception; ls-scoped formatting passes.
