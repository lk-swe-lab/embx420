# Package contract and test matrix

## Status and authority

This is the selected implementation contract under the user’s autonomous-work
authority. Routine format and limit decisions below are settled, not repeated
user approval gates. Independent review and behavior evidence remain required.
This document alone proves no implementation or test pass. The
[plan](packaging-plan.md) controls scope, coordination, and release stages.

Baseline: current working files, including intentional edits, additions, and
deletions. Do not restore commands from HEAD. Source, generated package output,
and installed copies are separate. Install/remove must not write source or build
files. No remote fetch, archive extraction, dependency solver, automatic update,
signature system, or LLM input-schema adapter is included.

Reviewed: `src/cmd_spec.h`, `src/registry.[ch]`, `src/main.c`, `src/shell.c`,
`Makefile`, command builders in `cmd_clear.c`, `cmd_cd.c`, `cmd_cat.c`,
`cmd_head.c`, `cmd_touch.c`, and `cmd_rg.c`, and the installed header at
`build/argtable3/include/argtable3.h`. No source `pkg.json` was found.
Course basis remains the slides and notebook cited in the plan. The indexed
File I/O slide 13 also states that catalogs/docs derive from metadata, not source
parsing. Numeric limits and exact formats below are project decisions, not course
quotations.

## 1. Package layout and manifest

The first package is `hello`, version `0.1.0`, with command `hello`. Its
authoritative source manifest is `packages/hello/pkg.json`. Make generates a complete directory
at `build/packages/<name>-<version>/`. It copies the source manifest unchanged.
`pkg install DIRECTORY` reads that generated directory; it never runs Make,
scripts, or package binaries. The manifest is authoritative for payload ownership.
Unlisted input entries are errors, not copied silently.

Build output targets are immutable. Before running the exporter or writing any
output, reject overlap between the source directory and output root in either
ancestry direction. Reject binary/exporter inputs inside the output root too.
Publish a complete private stage only when the target is absent. If it exists,
accept only the exact expected record, file/directory inventory, bytes and modes,
with the same safe file types and link rules. This repeat build is a no-op.
Reject all other existing targets without moving or deleting them. Use a new
`--output-root`, or move the old target manually, to build changed inputs at the
same version. There is no automatic replacement, backup, or restore operation.
Stage cleanup must never own an existing target. No hostile concurrent writer
or concurrent build safety is claimed for this local build tool.

`pkg.json` is one JSON object. The agreed core fields are `name`, `version`,
`description`, `commands`, `files`, `docs`, and empty `dependencies`. These and
`schema_version` are required. Reject unknown keys. Use the validation rules and
numeric bounds below; they keep parsing and state bounded.
Duplicate object keys, trailing data, BOM, invalid UTF-8, embedded NUL, invalid
Unicode surrogate escapes, and unsupported `schema_version` are errors. JSON
numbers must be integers where an integer is required. Parse with bounded input
and nesting; do not silently truncate values or repair names/paths.

| Key | Type and rule |
| --- | --- |
| `schema_version` | Integer `1`. |
| `name` | Package name, as defined below. |
| `version` | Exactly three decimal components `X.Y.Z`; each is 0–65535, with no leading zero except `0`. No suffix or prefix. |
| `description` | Nonempty single-line public text. |
| `commands` | Nonempty array of distinct command names. Maps each name to `bin/<command>`. |
| `files` | Nonempty array of distinct regular-file paths. Exact payload inventory; excludes `pkg.json` itself. |
| `docs` | Documentation paths, each also in `files`; one generated `docs/<command>.md` per command. |
| `dependencies` | Empty array only. Any dependency is unsupported and rejected. |

Package names match `[a-z][a-z0-9-]{0,62}` (63 bytes maximum). Command names
match `[a-z][a-z0-9-]{0,59}` (60 bytes maximum). The three-byte `.md` suffix
must fit the 63-byte path component bound. Do not truncate names. Apply this
command bound in both builder and package backend. Names are case-sensitive.
Reject a package name already installed, even at another version. Reject any
command name shared by two installed packages, or reserved by a builtin or shell
control. Always reserve `help`, `exit`, and `pkg`, even before `pkg` registration.
Use the actual working builtin inventory; do not restore deleted commands.

Payload paths use `/` separators and ASCII components matching
`[A-Za-z0-9_][A-Za-z0-9_.-]{0,62}`. Reject absolute/empty paths, empty components,
`.` or `..` components, backslash, control bytes, trailing `/`, and duplicate paths.
Do not normalize rejected paths. Required executables use `bin/<command>`;
generated metadata uses `metadata/commands.json`. Allow other regular support
files when explicitly listed in `files` and subject
to the same safety checks. Do not impose a closed three-kind inventory.
The manifest, not a fixed layout list, defines ownership. Directories are the
necessary parents only. The manifest is mandatory separately.
Reject symlinks in the package directory path and all descendants; reject devices,
FIFOs, sockets, and regular files with link count other than one. All binaries
must have at least one execute bit. Docs and metadata must be readable files.

### Safety limits

Use these initial limits and the name/path bounds above. Check overflow and
reject excess input explicitly; do not truncate. No configurable limit system
is needed for this milestone.

| Resource | Inclusive limit |
| --- | --- |
| Manifest input | 65536 bytes |
| Generated metadata input | 262144 bytes |
| JSON nesting | 16 levels; root container is level 1 |
| Package commands | 16 |
| Payload files | 64 |
| Installed packages | 64 |
| Installed commands | 256 total |
| Relative payload path | 255 bytes; each component 63 bytes |
| One payload file | 16777216 bytes |
| Total payload bytes | 67108864 bytes; excludes manifest |
| Description or summary | 256 UTF-8 bytes |
| Detailed help or usage | 16384 UTF-8 bytes per field |
| Argument/option description | 1024 UTF-8 bytes |
| Argument/option entries | 64 per command, combined |

Public text excludes NUL, DEL, and C0 controls except LF and TAB in multiline
help/usage/argument descriptions. Single-line text also excludes LF/TAB/CR.
Decoded text limits count UTF-8 bytes. Check file lengths and accumulated sizes
without overflow before allocation/copy, then enforce limits while reading.
An over-limit installed root is invalid, not a partial successful catalog.

## 2. Generated command metadata and docs

`metadata/commands.json` contains exactly `schema_version` (integer `1`),
`package` (manifest name), `version` (manifest version), and `commands` (array).
Each command object contains exactly these fields:

| Field | Contract |
| --- | --- |
| `name` | Manifest command name. |
| `summary` | Nonempty single-line text from the command specification. |
| `long_help` | String from the specification; absent source text becomes `""`. |
| `usage` | Nonempty syntax string generated from the shared definitions, without a terminal newline. |
| `arguments` | Ordered array of positional entries. |
| `options` | Ordered array of named entries. |

Each argument entry contains `metavar` (nonempty display string), `description`
(string), `min_count` (nonnegative integer), and `max_count` (integer at least
`min_count`, or JSON null for unbounded logical repetition). Each option entry
contains the same fields plus `short` (array of one-character aliases), `long`
(array of aliases without `--`), and `value` (`none`, `required`, or `optional`).
At least one alias is required. For valueless flags, `metavar` is `""`.
Alias arrays contain distinct ASCII names; short aliases are alphanumeric and
long aliases match `[a-z][a-z0-9-]{0,62}`. No alias can belong to two entries.
Entry order is definition order. Limits are logical grammar limits, not
argc-sized allocation limits. A display datatype is not a machine input type.
Defaults, interactions, and extra validation stay in shared public help text;
this format is not a complete LLM input schema.

All JSON rules from section 1 apply. Reject unknown/missing fields, duplicate
command entries, invalid counts, and metadata identity/version/name-set mismatch.
Metadata command order and manifest `commands` order are ASCII name order.
Manifest path arrays are independently sorted in ASCII path order. For commands
`a` and `a-a`, docs order is `docs/a-a.md`, then `docs/a.md`.
Packaged generated metadata uses fixed field order as listed above, two-space
indentation, and one final LF. Direct command JSON may be compact; it must use
fixed field order and one final LF. The source manifest is copied unchanged.
Readers accept legal JSON whitespace/key order. Escape JSON strings correctly;
no shell interpolation.

Generate docs at `docs/<command>.md` from the same name, summary, long help,
syntax, and argument/option entries. Use a command heading, summary, usage code
block, help text, and argument/option lists; omit empty sections. Each page contains those facts, not a separately edited
option list. Generation must be byte-repeatable for unchanged inputs; no clock,
absolute build path, or host environment fields. A failed generation must not
publish a partial output directory. Installation checks metadata schema and
manifest agreement, but does not reconstruct Markdown to prove help equivalence.
Build-time agreement tests establish docs/help/parser consistency.

### Export integration

At the initial source review, `cmd_spec_t` had no metadata callback. Command builders are static
and invocation-owned. The installed `arg_hdr` exposes `shortopts`, `longopts`,
`datatype`, `glossary`, `mincount`, `maxcount`, and value flags
`ARG_HASVALUE`/`ARG_HASOPTVALUE`. The terminator and `arg_rem` entries are not
arguments. Header fields do not describe defaults, cross-option constraints,
semantic types, or all logical cardinalities. For example, `head` sizes repeated
options from argc; `touch` changes help cardinality; `rg` uses a combined operand
label. Do not export these storage details as public grammar.

Use an optional status-returning metadata visitor on `cmd_spec_t`, implemented
first for `hello`. It builds
its shared table, visits structured entries and logical limits, then frees every
entry on success/failure. The exporter owns JSON and Markdown rendering; it does
not parse terminal help or C source. New static builtin initializers use NULL
until adopted. Do not require every existing command to migrate in item 2.
The implementation owner selects the smallest internal callback signature and
records its ownership rules. Shared definitions own logical limits; the exporter
only renders them. Keep header/source changes local to this integration; no
public ABI stability promise or builtin-wide migration is required.

## 3. `pkg` CLI and streams

AiShell entry points are `main pkg ...` and `pkg ...` inside the main loop.
No standalone `pkg` wrapper is required initially. Supported forms:

- `pkg -h` or `pkg --help`
- `pkg list`
- `pkg info NAME`
- `pkg install [--] DIRECTORY`
- `pkg remove [--] NAME`

Only the top-level help forms are accepted; unknown options/subcommands, wrong
arity, and invalid NAME syntax return 2, even if help also appears. `--` is only
an operand separator in the two forms shown; use `./-name` or `--` for a
dash-prefixed directory. `info` and `remove` select a package name, not a path,
version, or command. No prompt, force flag, JSON mode, or implicit update.

| Form | Exact successful stdout; each displayed line ends with LF |
| --- | --- |
| `list` | One `NAME VERSION` line per package, ASCII name order; empty state emits no bytes. |
| `info NAME` | `Name: NAME`, `Version: VERSION`, `Description: DESCRIPTION`, `Commands: C1 C2 ...`, in that order. Command names use ASCII order. |
| `install` | `Installed NAME VERSION` after publication. |
| `remove` | `Removed NAME VERSION` only after complete removal. |
| Help | Syntax above, purpose, limits, update/partial-removal policy, statuses; no filesystem access. Freeze exact bytes with the first help test. |

Status 0 means success. Status 2 means CLI syntax/module-argument error. Status 1
means validation, state, lock, allocation, or I/O failure, including missing
packages, collisions, corrupt records, and output failure. Failures emit a
nonempty `pkg: ` diagnostic on stderr, no success line. System error wording is
not byte-stable; tests assert category/context, not localized strerror text.
Successful operations have empty stderr. Read/validate complete list/info results
before output; validation failure emits no partial list. Output failure can leave
partial stdout and cannot undo a committed install/remove. Preserve that fact in
the diagnostic. Preserve caller argv, exported getopt state, cwd, environment,
and borrowed descriptors. Internal Argtable allocation panic recovery is not
promised. Retain the current shell SIGPIPE policy; do not change it globally.

## 4. Installed state, lock, and failure rules

Use the absolute nonempty `HOME` to form `$HOME/.mysh/pkgs`. Do not fall back to
cwd if HOME is missing/relative. Reject symlink root components. Create missing
private directories with mode 0700; require existing managed directories and lock
to be owned by the effective user and not group/other-writable. Do not chmod
unrelated existing parents. No privilege escalation. This is a local same-user
manager, not a sandbox for hostile executables or a defense against that user.
Installed directories are 0700, binaries 0700, and other regular files 0600;
strip set-id and special bits. Do not preserve owners, ACLs, or timestamps.

Use one persistent regular `.lock` file, mode 0600, with nonblocking POSIX fcntl
record locks. ALL package snapshot/read/write operations use an exclusive lock.
Mutations hold it from state validation through cleanup/publication;
list/info/catalog/help/discovery hold it while taking a snapshot. No shared
reader locks. Release descriptors on every path. A busy lock is
status 1, without waiting. Never unlink `.lock`; process death releases its lock.
No root means an empty list/catalog; info/remove return not found without root
creation. On an existing root, readers may create the missing lock safely.

A visible package directory is exactly `<name>-<version>` and contains its
unchanged `pkg.json` plus payload. The manifest is the only package record.
Scan and validate all visible manifests, paths, and collisions before mutation
or installed discovery. Removal has only the missing-payload exception below.
Reject directory/identity mismatch, malformed/missing records or metadata,
duplicate package/command names, unsafe entries, and missing/nonexecutable
binaries for discovery. Do not guess records from directory names, ignore a
corrupt record, or auto-delete it. Return an error; existing builtin-only dispatch
must remain usable. Administrative repair is manual and outside this CLI.

Install preflights source and installed state, copies only validated entries into
an exclusive `.tmp-<unique>` directory inside the root, checks copy/read/write/
close failures, then publishes with one same-root rename to an absent final name.
Never overwrite a final entry. Resolve descendants from directory descriptors
with no-follow checks; do not use string-prefix containment or follow links on
reopen. Check source identity/type/size during copy and reject observed changes.
Stage manifest last. An ordinary failure removes only this operation's staged
files; report cleanup failure and leave the hidden stage, never a visible partial
package. A complete rename is the commit point. Process-crash namespace safety
is required; power-loss durability is not promised. Do not claim atomic updates.

After process death, `.tmp-*` entries remain hidden and reserve no package or
command names. List/discovery ignore them. Do not traverse or automatically
remove stale stages: report them on the next mutation and stop for manual cleanup.
Unknown root entries other than `.lock` and stages are corrupt state. This avoids
recursive deletion of untrusted leftovers. Kernel lock recovery needs no stale
PID-file removal.

Remove validates ALL manifests, path safety, containment, and collisions before
the first unlink. It tolerates missing recorded payload in ANY package, including
metadata/binaries, so two partial packages do not block each other's removal.
No package may omit its manifest. Present payload must still pass applicable
type, path, and metadata validation; missing files are the only exception.
This exception applies only to removal, not install, list/info, catalog, help,
discovery, or execution. Unlink only the target's present recorded
regular files; reject changed file types or unsafe parent paths. Missing
files are success; other errors stop removal. Then remove empty derived parent
directories, then the manifest, then the empty package directory. Check for
unlisted entries before deleting the manifest. Preserve unlisted entries and
return 1, retaining the record. Earlier recorded unlinks can already have occurred.

If final rmdir fails after manifest unlink, restore the saved manifest through a
safe exclusive file creation if possible; report restoration failure explicitly.
No rollback of payload deletions is promised. A residual directory without a
record is corrupt and requires manual repair. No remove path follows symlinks,
uses recursive deletion, touches source/build files, or deletes unrecorded files.
A partial package is not executable/discoverable until a successful reinstall;
removal is the only relaxed missing-file path. Remove-then-install is the
only update: a failed installation can leave the package unavailable.

## 5. Shell integration requirements

The current builtin registry has 32 borrowed, process-lifetime slots and no
unregister API. Keep it for static builtins; do not register pointers into freed
installed snapshots or consume its capacity with external commands. Maintain a
separate bounded, owned installed snapshot. Refresh under the root lock for each
help/catalog/dispatch, so a running loop sees successful install/remove on its
next command. Do not keep the lock while a child runs. Concurrent remove after
snapshot release can cause exec failure; report it, never fall back to PATH.

`help` and `exit` are shell controls outside the registry. Preserve them, reserve
their names, and add installed lookup to `help COMMAND` without running the binary.
Builtin help/dispatch semantics and legacy traversal routing stay unchanged.
`main --commands-json` needs an explicit route before current legacy getopt
routing; otherwise it reaches `legacy_main`. Reject extra catalog arguments with
status 2. Package `pkg` registration must retain overflow and lifetime checks.

Catalog wire format: object with `schema_version: 1` and `commands` array in ASCII
name order. Entries contain `name`, `summary`, `origin` (`builtin`, `control`, or
`package`), `package` (name or null), and `version` (version or null). Include
registry builtins and `help`/`exit`; include installed entries from metadata. This
minimal catalog is discovery, not a full LLM schema. Never execute installed
binaries to get catalog/help text. Corrupt installed state fails catalog/help
listing rather than publishing an incomplete available-command list.

Dispatch uses fork/exec with the recorded absolute installed executable path,
never PATH or a system() command string. Forward argv and inherited standard
streams; close internal descriptors and reap the child. Return child exit status;
use 128 + signal for a signaled child, 126 for recorded executable launch failure,
and 127 for an unknown command. No pipelines/background jobs. Builtins keep their
existing status rules. Test actual main and persistent loop, not a mock registry.

## 6. Named acceptance matrix

The matrix specifies required evidence, not a claim that cases exist or pass.
At this checkpoint the parent observed `hello` help RED (status 2), with missing
usage/metadata markers; no complete suite pass is recorded here. Each case
captures stdout/stderr separately and asserts status plus allowed/forbidden
namespace and byte changes.
Use an isolated HOME, controlled locale/environment, and bounded subprocesses.
`S` means section 3 success bytes/status 0/empty stderr. `E1` means status 1,
nonempty contextual stderr, no success line. `E2` means status 2, diagnostic,
no filesystem changes. A row's variants must become independently named cases.

| Test name | Inputs / fault / required observation |
| --- | --- |
| `pkg_manifest_valid` | Small valid package and each exact limit; install S, byte-identical record/payload except modes. |
| `pkg_manifest_schema` | Each missing/unknown/duplicate key, wrong type, dependency, unsupported schema, trailing data/BOM/invalid UTF-8/surrogate/NUL; E1, no publication. |
| `pkg_name_version` | Boundary names/components; uppercase, empty, slash, suffix, leading zeros and overflow; invalid package E1, invalid CLI name E2. |
| `pkg_resource_limits` | Each section 1 limit at maximum and one beyond, nested JSON, size growth/overflow; bounded memory/time, E1 beyond limit. |
| `pkg_paths_inventory` | Absolute, traversal, empty/dot components, backslash, repeated paths, wrong executable mapping, missing/unlisted files/dirs; E1, outside sentinel unchanged. Explicitly listed safe regular support files are accepted; required generated docs use `docs/<command>.md`. |
| `pkg_links_types_modes` | Linked root/ancestors/leaves, hardlinks, FIFO/device/socket, nonexecutable binary, special mode bits; reject unsafe input, normalize accepted installed modes. |
| `pkg_metadata_schema` | Every metadata field/type/count/alias error, duplicate command and manifest name/version/set mismatch; E1 before copy. |
| `pkg_generated_agreement` | Definition → parse/help/JSON/Markdown; option aliases, ordered operands, summary/help and syntax agree; no separate list. |
| `pkg_export_repeatable` | Different argc, repeated calls, unset/synthetic POSIXLY_CORRECT, escaped text; fixed logical counts and identical generated bytes. |
| `pkg_export_failures` | Constructor/owned allocation/render/write/close failure; release owned entries, preserve caller state, no partial generated directory. |
| `pkg_cli_grammar` | All forms, missing/extra operands, unknown flags, help with error, `--`, dash directory, malformed short clusters under deadline; S/E2 as specified. |
| `pkg_empty_info_list` | Absent root, valid populated state, missing name; exact sorted list/info, no implicit root for absent reads, E1 for missing info. |
| `pkg_install_collision` | Same name/version, other version, builtin/control/pkg command, other installed command, limits; E1, existing bytes unchanged. |
| `pkg_root_ownership` | HOME unset/empty/relative, unwritable root, unsafe ownership/modes/lock type; E1, no cwd fallback or chmod of parents. |
| `pkg_copy_faults` | Short read/write, EINTR, zero write, allocation/open/read/write/close/rename failure and observed source mutation; recover or E1; only owned stage cleanup. |
| `pkg_publish_boundary` | Stop child before/after rename; visible state is absent or complete, no overwrite; hidden crash stage stays undiscoverable. |
| `pkg_cleanup_failure` | Stage unlink/rmdir failure; E1, explicit cleanup diagnostic, hidden residue, existing packages/source/build unchanged. |
| `pkg_lock_contention` | Two real processes; reader/reader, reader/writer, writer/reader, and writer/writer all contend on one nonblocking exclusive root lock, including help/catalog/discovery snapshots; busy fails without wait or state change; kill holder, next operation acquires same lock inode. |
| `pkg_stale_stage` | Hidden stage after death, unsafe/unlisted root entry; reads ignore stage only, next mutation E1, no automatic recursive cleanup. |
| `pkg_corrupt_records` | Missing/malformed manifest, directory mismatch, duplicate identities, corrupt/missing metadata or binaries; list/info/catalog/install E1, no silent skip/deletion; builtin dispatch still works. |
| `pkg_remove_complete_retry` | Complete target, absent payload, failed prior removal; S when complete, record retained until safe final deletion. |
| `pkg_remove_two_partial_packages` | Two packages have valid manifests and safe, noncolliding paths but missing payload, including metadata/binaries. From fresh paired fixtures, remove A then B and B then A; both removals S. Malformed manifests, unsafe present entries, and collisions still E1 before unlink. List/info/install/catalog/help/discovery/execution remain strict before removal; no partial package can execute. |
| `pkg_remove_partial_unlisted` | Unlisted file/directory, wrong type, changed parent, unlink/rmdir failure; E1, partial deletion allowed, unlisted/outside bytes unchanged, record retained when possible. |
| `pkg_remove_record_restore` | Final rmdir failure then manifest restoration success/failure; E1, distinguish recoverable record from explicitly corrupt residue. |
| `pkg_output_failure` | List/info/help and post-commit install/remove output failures; no false success; state reflects actual commit, not promised rollback. |
| `pkg_module_ownership` | Invalid argc/NULL argv/element, repeated success/error/help, getopt sentinels, permuting parser seam; preserve argv strings/pointers, cwd/env/descriptors, release locks/storage. |
| `pkg_registry_capacity` | Current builtin inventory, repeated registration, identical pointer, duplicate name and capacity boundary probes; no package use of borrowed static slots. |
| `pkg_shell_discovery` | One-shot main and one live loop: install → help/catalog → dispatch → remove → unknown; refresh without restart, valid exact catalog fields/order. |
| `pkg_shell_exec_status` | Deliberate child exits/signals/exec failure, poisoned PATH, cwd change, argv quoting, inherited streams, descriptor closure and wait failure; specified status and no fallback. |
| `pkg_shell_controls_legacy` | help/exit reservation, help without executing payload, existing builtins, `-r`, `-h`, `--help`, `--commands-json` extra args; preserve old routes except explicit new catalog route. |
| `pkg_update_lifecycle` | Build/install/list/info/help/execute/catalog/remove, removal disappears, versioned remove/install, failed replacement leaves absent package; source/build unchanged by pkg. |
| `pkg_safety_negative_controls` | Deliberate traversal-follow, ignored-copy-error, stale snapshot and premature-record-delete mutants; require intended assertion failures, then unchanged-source pass. |
| `pkg_regression_delivery` | Applicable current suites, build, lint, format-check, sanitizers and bounded malformed JSON/path fuzz; record actual commands/results and approved omissions. |

Implement one behavior at a time with observed RED → GREEN → refactor evidence.
No GNU package tool is an oracle for this project format. Fixtures and independent
JSON/namespace/byte checks are the oracle. Current commands for later delivery:
`make test`, `make all`, `make lint`, `make format-check`. Future package/export
target names and test runner paths must be added and verified with their code;
none is claimed to exist now. Independent review precedes any authorized local
commit. This documentation step needs no build/test run.

## 7. Delivery boundary

`hello` / `0.1.0`, schema version 1, the wire formats, docs layout, support-file
inventory, and initial limits are selected. Routine internal API decisions are
delegated. No new dependency, database, broad builtin migration, or repeated
user approval step is required. Independent review and actual verification
remain open until recorded. No source implementation is marked complete here.

After the milestone passes, build a separate `aishell` / `0.1.0` release that
bundles the shell, existing builtins, and generated docs/catalog. Follow the
plan’s isolated, relocated-artifact checks and separate shell install/update
instructions. It is not a `pkg` payload or evidence of shell self-update.
Local commits are authorized after review and scoped verification. No push.
