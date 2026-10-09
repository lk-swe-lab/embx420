# cp: inventory, contract, and report

Status: planned; not implemented. User requires mmap for file contents.

Manual: installed `man 1 cp`, GNU coreutils 9.4, August 2026 footer,
rendered on 2026-10-07. Course source: `data/02/CLI_Tools.docx`,
queried through `docs/data.sqlite`. Selection uses the user's autonomous
continuation authorization. Author/copyright/support sections are provenance.
Use argtable3 and cmd_spec_t, standalone wrapper, status 0 success,
1 operational failure, 2 invalid arguments. Parse fully before side effects.
Keep human -h/--help and course --json separate from GNU reference tests.
No shell registry exists. No commits or pushes.

Course paragraphs 41–46: copy files/directories, -r, -f, JSON.

| Manual feature | Decision and reason |
| --- | --- |
| SOURCE DEST; multiple SOURCE... DIRECTORY | Keep one source and destination, existing directory basename semantics; defer multiple sources |
| -R/-r/--recursive | Keep course-required directory copies |
| -f/--force | Keep retry after removal when destination cannot be opened; no prompt |
| --help | Keep plus course -h |
| -a/--archive, -d, -p, --preserve[=ATTR_LIST], --no-preserve, attributes mode/ownership/timestamps/links/context/xattr/all | Defer archival metadata and hard-link topology |
| --attributes-only, --copy-contents | Defer metadata-only/special-file content copying |
| --backup[=CONTROL], -b, -S/--suffix, SIMPLE_BACKUP_SUFFIX, VERSION_CONTROL, controls none/off/numbered/t/existing/nil/simple/never, same-file force+backup special case | Defer backups; always reject same-inode copying |
| --debug, -v/--verbose | Defer progress text; JSON summary instead |
| -i/--interactive, -n/--no-clobber, --update[=all/none/older], -u | Defer alternative overwrite policies |
| -H, -L/--dereference, -P/--no-dereference | Defer selectable policies; preserve symlinks during recursion, follow standalone source link |
| -l/--link, -s/--symbolic-link | Defer link creation modes (recursive symlink preservation remains necessary) |
| --parents, --strip-trailing-slashes, -t/--target-directory, -T/--no-target-directory | Defer operand layout switches; preserve normal path resolution |
| --remove-destination | Defer unconditional removal |
| --reflink[=always/auto/never] | Defer clone/CoW; mmap required |
| --sparse=auto/always/never and sparse heuristic | Defer sparse preservation; bytes still preserved |
| -x/--one-file-system | Defer filesystem boundary policy |
| -Z, --context[=CTX] | Defer security labels |
| --version | Defer packaging metadata |
| --json (course addition) | Keep structured operation summary |

Design constraints: mmap regular-file copying, correct destination size,
empty-file special case (no zero-length mapping), bounded mapping windows,
unmap and close on every path. Detect same inode before truncation; reject
destination inside source tree and traversal cycles. Reject unsupported special
file types without blocking. Preserve existing destination mode; new file modes
derive from source with inherited umask. Do not promise ownership, timestamps,
sparse allocation, hard-link topology or atomic replacement. Document partial
destinations on operational failure and concurrent source mutation limitations.
Detailed path/link race policy and JSON contract must be recorded before tests.

Tests first: binary/empty/large/multi-window, shorter overwrite/truncation,
same path/hardlink unchanged, directory destination, recursive nested and empty
directories, symlinks/dangling links, no recursion error, destination-inside-source,
-f semantics, permissions, mmap/ftruncate/msync/munmap failures via controlled
injection, output failure, JSON, module reuse/descriptor cleanup.
Use isolated temporary trees for all mutations; native cp verifies filesystem
results independently of project output schema.

## Results

Pending.

