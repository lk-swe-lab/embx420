# mv — selected contract and implementation evidence

Authorized autonomous implementation; no staging/commit/publication. Course query:
`docs/data.sqlite`, `lecture_content`/`lecture_sources`, CLI_Tools.docx paragraphs
48–52: move/rename files/directories, -h/--help, -f, --json. Recorded before tests.

## Full installed manual inventory / selection

Installed `man mv` (section 1), GNU coreutils 9.4, August 2026. Primary APIs:
POSIX rename/renameat, Open Group Issue 7 2018 edition and Issue 8 2024
(retrieved using curl after web_extract backend refused extraction), installed
rename(2), Linux man-pages 6.7; primary GNU full mv invocation also retrieved.

| Manual feature | Keep/defer and reason |
| --- | --- |
| SOURCE DEST; SOURCE... DIRECTORY; -t DIRECTORY SOURCE... | Keep exactly one SOURCE DEST; multiple sources/-t deferred to bound mutations. |
| Existing directory receives basename | Keep; useful file organization. Directory symlink destinations follow. |
| -f/--force | Keep course flag, authorizes replacement without prompts. |
| -i/--interactive, -n/--no-clobber; last -i/-f/-n wins | Defer prompts/skips/ordering; default refuses existing effective targets with error rather than GNU noninteractive overwrite. |
| --backup[=CONTROL], -b | Defer backup workflow. |
| -S/--suffix, SIMPLE_BACKUP_SUFFIX; VERSION_CONTROL | Defer with backups. Manual controls none/off, numbered/t, existing/nil, simple/never. |
| --debug (implies -v), -v/--verbose | Defer verbose/debug output; JSON serves bounded agent summary. |
| --no-copy | Always rename-only; cross-filesystem copy/delete deferred. |
| --strip-trailing-slashes | Defer option; trailing SOURCE slash requires a real directory, never a final link. |
| -T/--no-target-directory | Defer; directory destinations always use basename. |
| --update[=all,none,older], -u | Defer time/skip comparisons. Manual default all without option, older with option; none skips without failure. |
| -Z/--context | Defer SELinux relabeling; rename preserves object. |
| --help, --version | Keep help, add course -h; version deferred. |
| Default streams/status | Keep silent successful text stdout, stderr diagnostics, 0 success/1 operational failure; project argument errors 2. GNU manual refers full info for details. |
| Cross-device copying, metadata copying/rollback | Defer; exactly one rename, no copy/unlink/chmod. |
| --json | Course addition; stable one-object summary below. |

## Binding operation contract

`mv [-h|--help] [-f|--force] [--json] [--] SOURCE DEST`.
Fully parse/validate before filesystem mutation; no help precedence over malformed
options. Help permits missing operands and is read-only. Empty paths and invalid
UTF-8 paths in JSON mode return 2 without JSON/mutation. Text paths are raw bytes.

Default refuses **any existing effective target**, including dangling symlinks.
`-f` authorizes a single POSIX rename over a distinct target: non-directories over
non-directories, directories over empty directories. Type mismatch and nonempty
directories fail without deletion; never unlink target to work around failure.
No prompting, no stdin reads. Final source symlinks (including dangling links) move
as links. Existing directory DEST (including symlink-to-directory) receives source
basename. Final effective target symlinks are replaced, not followed. Source
trailing slashes are trimmed only after verifying the final entry is a directory;
symlink/regular trailing slash fails. DEST trailing slash requires existing
resolved directory. Source basename empty, dot or dot-dot is rejected. Source
self-target or same inode through a hard link is error even with force; final
symlinks pointing at source are distinct entries. Directory into itself/descendant
is rejected by rename/kernel (or existing-target preflight), never recursively
copied. Missing parents are not created. FIFO/socket/device entries can be renamed
without opening/reading them; destination-dir interpretation is the only follow.

JSON valid operands get exactly one newline-terminated object:
`{"source":SOURCE,"destination":DEST,"status":"ok"|"error"}`. Paths are original
operands, not resolved targets; stderr retains human errors. JSON escapes controls
and quotes and validates UTF-8 before mutation. Module allocation/preflight errors
may produce no JSON. Detect obviously invalid/read-only stdout before JSON rename;
actual write errors can occur **after rename**, return 1, and do not roll back.
Text success does not use stdout. Help output failure returns 1.

Preserve source inode/mode/owner/mtime/content/links on successful same-device
rename; object ctime and parent timestamps may change. No durable fsync promise.
Failed validation/preflight/local rename preserves namespace; POSIX EIO/NFS
ambiguous errors, concurrent namespace changes, and interruption after mutation
cannot guarantee unchanged state. Default no-overwrite preflight is not an atomic
no-replace guarantee under concurrency. Trusted stable local namespaces only.
Borrow argv strings/pointers and stdin/stdout/stderr; preserve exported getopt
state, not private interrupted scans or thread safety. Constructors checked;
internal argtable OOM may terminate host. No signal disposition changes.

## Planned vertical slices and verification

1. Runnable no-op baseline → exact inode-preserving file rename/reference.
2. Directory resolution and directory/link moves, trailing slash/type safety.
3. Existing-target refusal / force and same-inode checks.
4. Parser/help/JSON/encoding and output failure.
5. Module fault seams, borrowed state/reuse, shell registry/PTY integration.

Reference expectations are independent fixture bytes/inodes, not generated by GNU.
Separate reference/project groups; incorrect /usr/bin/true negative control.
Disposable fixtures only. Full build/regression/lint, scoped formatting, fresh
HEAD + exact allowlisted patch, fresh default O0 ASan/UBSan full regression.
Global format baseline and inherited O1 fault-harness limitation stay separate.

Execution evidence: [raw complete streams](../../logs/commands/mv-execution.log)
and [structured ledger](../../logs/commands/mv-evidence.json).

## Observed execution / acceptance

Four runnable incremental cycles: no-op module RED → inode-preserving rename
GREEN; direct rename RED (directory basename assertion) → directory resolution
GREEN; overwrite/force RED → force/refusal/same-inode GREEN; help/JSON/`--` RED →
argtable parser/JSON/output behavior GREEN. The initial slice-2 missing-path
exception was tightened into an explicit assertion before implementation. The
initial `/usr/bin/true` run is a harness negative, not the project baseline RED.
Registry RED demonstrated unknown-command/missing-discovery behavior; adding the
real builtin then exposed the separate parity inventory omission, fixed before
GREEN. Final audit additionally found that destination `stat` incorrectly rejected
a final cyclic link; GNU and direct rename succeeded. A new runnable regression
failed with ELOOP, then a narrow final-entry `lstat` check distinguished unresolved
final links from invalid directory prefixes, with no unlink/copy workaround.
All intermediate failures and corrections remain in the raw evidence.
Later property/fault tests characterize already-correct behavior; deliberate
no-rename and argv-reordering mutants independently demonstrated sensitivity.

| Requirement | Executed evidence |
| --- | --- |
| File bytes/inode/mode/owner/mtime and absent source | `test_file_rename` |
| Tree rename/directory destination/trailing real-directory slash/directory link | `test_directory_destination_and_links` |
| Live/dangling source links and FIFO without referent I/O | `test_links_and_special_source_without_follow` plus dangling fixture in directory test |
| Force replaces entry not referent; empty-directory target | `test_force_overwrites_entry_not_referent` |
| Force replaces a cyclic/unresolvable final target link | `test_force_overwrites_cyclic_final_link` |
| Default existing/dangling target refusal; same inode/hard link | `test_refuse_existing_and_same_inode` |
| Help/arity/unknown options/empty paths/UTF-8 before mutation | `test_help_and_argument_preflight` |
| Original paths, escaped JSON status, raw-byte text names | `test_json_status_original_paths_and_encoding` |
| Missing parents, types, nonempty targets, descendants, unchanged namespace | `test_errors_and_no_unexpected_namespace_effects` |
| Trailing link/dot/dot-dot/self directory refusal | `test_trailing_link_dot_self_and_missing_target` |
| `--`, leading-dash names, unaffected other hard links | `test_unusual_option_names_and_hardlinks` |
| JSON post-rename `/dev/full` error, help output failure | `test_postmutation_output_failure_and_help` |
| Real nonroot directory permission denial | `test_permission_failure` |
| Actual kernel EXDEV with existing target preserved | `test_real_exdev_preserves_source_and_existing_target`; disposable memfd proc-link → scratch local target; no mounts or /dev/shm fixtures |
| 100 seeded byte/name/inode/namespace properties | `test_seeded_raw_name_namespace_properties` |
| Constructor NULL6, module allocation3, preflight5, rename errno10 | `test_mv_module.c`; EXDEV/EACCES/EROFS/ENOSPC/EIO/EBUSY/ENOTEMPTY/EINVAL/ENOMEM/EINTR; exactly one attempt, failures preserve source/target |
| argv pointer/string bytes, exported getopt, cwd/stdin position/descriptors | module harness across 500 swap/back/help loops; real parser wrapper deliberately permutes its received pointer array |
| Short writes/EINTR/output error without rollback | module harness plus real `/dev/full` |
| Actual main discovery/run/help/parity, errors, repeated loops and PTY | `test_shell.py`: 32 groups, 17 builtins/16 wrappers; `test_mv_registry_loop_and_pty` attaches both stdin/stdout to child-owned PTY; shell module registry capacity/overflow retained |

- `make test-mv`: 9 GNU-compatible groups pass; 6 deliberate project groups skipped.
- `make test-mv-impl`: all 15 project groups and module assertions pass.
- `/usr/bin/true` and no-rename mutant: status 1, intended missing-source-removal
  assertion. Borrowed-argv mutant: status 134, `MV_ARGV_POINTER_CHANGED` marker.
- `make all test`: full normal regression passes in live checkout and fresh
  HEAD-plus-applied allowlisted candidate, not merely copied working sources.
- `make lint`: all 59 C translation units pass (only inherited suppressed
  dependency/NOLINT warnings). Scoped clang-format check passes; C lines <=80.
- Fresh candidate `make clean` then full `make all test` with **O0**,
  `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie`,
  link `-fsanitize=address,undefined -no-pie`,
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`,
  `UBSAN_OPTIONS=halt_on_error=1`: passes. Normal and sanitizer checks include
  actual main/PTY and all existing command suites; no baseline source substitution.
- Global `make format-check` still fails on exactly six unchanged inherited
  paths: src/cmd_spec.h, src/traverse.c/.h, tests/test_help.c, test_output.h,
  test_recursive.c. No new format failures. Known inherited optimized O1 shell
  fault-harness limitation is separate; O1 is not claimed verified here.
- Actual internal argtable malloc fault during parsing exits the process with
  status 1 and `Out of memory!`; the following `MV_HOST_SURVIVED` marker is absent.
  This is not recoverable module-owned OOM and is explicitly documented.

Primary retrievals (full raw data in research ledger):
`https://pubs.opengroup.org/onlinepubs/9799919799/functions/rename.html`,
`https://pubs.opengroup.org/onlinepubs/9699919799/functions/rename.html`,
`https://www.gnu.org/software/coreutils/manual/html_node/mv-invocation.html`.
The inventory follows the installed 9.4 manual, not newer online-only options.

## Review handoff and limits

HEAD base `ae3fe5ef739602aef2473d5486c4fa95070a8d82`. No staging, commit, wiki,
AI_LOG access, session-registration edits, publication or later command work.
Exact 16-path allowlist and complete HEAD-based patch/hash manifests live under
`/home/name/.hermes/cache/scratch/mv-delivery/`. README/FILES proposals are narrow
HEAD-based candidates there; the mixed live files are deliberately unchanged.
Installed argtable3/CUnit directories are the only fresh-checkout dependency
links. Final reapplication checks all delivered bytes/modes and untouched HEAD
anchors; final verification outputs are kept externally to avoid self-referential
patch/log hashes. The parent must independently review, then authorize staging
and exact-index verification; this report is implementation evidence, not review.

Raw execution streams contain genuine trailing whitespace/CR from tool/compiler
output. Whitespace checks exclude only `logs/commands/mv-execution.log`; preserve
its bytes/hash rather than sanitizing evidence. All source/documentation patch
whitespace must pass. Inherited mixed files/deletions/untracked data are excluded.

No cross-device copy/delete; no atomic default no-replace under concurrency;
no fsync durability; no EIO/NFS rollback guarantee; signals inherited, post-commit
output failure no rollback, internal dependency OOM can terminate host. Exhaustive
hostile namespaces, huge paths beyond kernel limits, threads and signal-driven
interruption after mutation are deliberately outside the stable local contract.
