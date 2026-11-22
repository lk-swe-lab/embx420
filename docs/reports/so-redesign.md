# Shared-module shell redesign

Status: approved design on `feat/aishell-so`; implementation and verification
exist in the working tree. Publication and merge remain separate.
No merge or push is authorized by this design.

## Approved design

The following boundary and acceptance gates record the approved intent.
Observed implementation/evidence is separated below.

### Boundary

`aishell` is a single-threaded host for input/tokenization, module discovery,
loading, registry/dispatch, and shell lifecycle. Command implementations are
shared objects, including `help`, `exit`, `cd`, `pkg`, and legacy traversal.
There is no statically linked fallback implementation of those commands.
Standard modules ship with the release; optional packages install separately.
No worker threads, remote repositories, dependency solver, or sandbox is added.

### Authoritative definitions

- Command specifications own names, summaries, parsing/help behavior, and callbacks.
- Package definitions own package identity, version, and module membership.
- A shared, versioned ABI header owns the module and host interfaces.
- Discovery metadata, command inventories, documentation, and catalogs are derived
  from those definitions, not independently maintained command lists.

Build dependencies still require explicit ownership; avoiding duplicated command
metadata does not mean inferring arbitrary build dependencies from filenames.

### Loading and lifetime

Use explicit release/store paths, not the working directory or PATH. Validate
ABI version, descriptor size, entry points, and command conflicts before
publishing a module's commands. Use local symbol visibility and eager binding.
ABI rejection is not a security check: loading native code can execute library
initializers, and accepted commands run with the user's privileges.

Registry callbacks borrow module-owned storage. Published module handles remain
alive for the shell session; removal must never leave a callable dangling pointer.
Unpublished failures must not leave partially registered commands. No hot unload
or thread synchronization framework belongs in this version.

### Packages and versions

Install immutable versioned directories. Retain earlier versions; select an active
version explicitly so the previous version can be selected for rollback. Never
replace mapped library bytes in place. If a package was already loaded, selecting
a replacement affects a fresh shell session, not an unsafe live replacement.
Removal makes that package unavailable for subsequent command dispatch.

`pkg.so` uses the public host API rather than editing private registry storage.
Discovery refresh is a host operation. Standard modules bootstrap independently
of optional package state; package-manager recovery is external restoration of
release files, not another hidden package manager in the executable.

### Execution

Commands execute sequentially. Shell-affecting commands must operate in the shell
process, so `cd` persists and `exit` requests orderly termination through the host.
No threads are introduced. Existing command contracts are characterized before
changing execution context. The implementation report must state the selected
execution model for ordinary commands rather than implying shared-library format
provides process isolation.

### Acceptance gates

1. Inspect executable linkage/symbols: no command implementation is linked into
   the host; all command implementations are delivered as modules.
2. Preserve actual-main help, catalog, argument/status, stdin boundary, and
   standalone-parity behavior, with explicitly reviewed compatibility changes.
3. In one running shell: absent hello, install hello module, discover/help/run,
   remove, then receive unknown command status 127 without rebuilding/restarting.
4. Prove persistent `cd`, previous-status `exit`, and module-based `help`/`pkg`.
5. Reject missing entry points, incompatible ABI, malformed descriptors, and
   duplicate names without partial registry publication.
6. Exercise install of distinct versions, active selection, loaded-module restart
   boundary, and fresh-session rollback using real modules.
7. Preserve package path/type/inventory checks and failure handling. Distinguish
   validation from the trust required to load native code.
8. Relocate a complete release, run outside the source tree, and install its
   included example into an isolated HOME.
9. Record observed RED/GREEN checks, full regression/build results, review
   findings, and remaining limits. A dispatched implementation is not a pass.

### Migration

Implement a working module/host slice, migrate shell controls and ordinary
commands, integrate versioned packaging and generated outputs, then independently
review and verify the integrated tree. Existing package/release artifacts must
not be overwritten merely to make changed builds pass; use a new version or
fresh, known-owned output root.

## Observed implementation

Source inspection: `src/main.c`, `shell.c`, `shell_input.c` and
`module_runtime.c` form the host; `registry.c` was deleted. `build/aishell` is
the actual executable and `build/main` its alias. Twenty-two standard modules
ship under `build/modules`; even help/exit/cd/pkg/traverse are shared objects.
All callbacks execute sequentially in the parent, without threads or fork/exec.

`module_abi.h` defines ABI v1. The entry returns a size/version/spec-size checked
descriptor with commands and optional refresh. The host API supplies lookup,
enumeration, exit/previous status, batch activation, hiding, identity and the
appended size-gated ABI-v1 `inspect_command` callback (`module_abi.h:23`). It
returns a borrowed unpublished single-command spec or NULL; no command grammar
or Argtable was added to the tiny host.
All standard modules intentionally load eagerly at startup, not lazily.
Eager/local dlopen and hidden symbols avoid a host command-symbol export API.
Handles, even unsuccessful preflight loads, are retained through process life;
removal hides commands, never unloads callbacks. Constructors may run during
install validation: native code is trusted, not sandboxed.

`build_modules.py` reads the first regex-matching cmd_spec_t initializer in each
cmd_*.c, deriving names/spec symbols; it is not a C parser or automatic dependency
inference. Dependency exceptions and hello exclusion remain explicit. Live
help/catalog read descriptors; structured metadata is required for native
packaging, but optional for standard commands' argument schemas.
Install/use compare stored command metadata semantically to `cmd_metadata_write`
from the actual loaded spec: summary/help/usage and ordered option/argument
grammar must match. Whitespace/object key order are ignored; drift or a missing
visitor is rejected before active selection/publication. Catalog origin
labels builtin/control remain compatibility vocabulary, not linkage claims.

Native schema-2 hello 0.2.0 is `build/native-candidates/hello-0.2.0`, defined by
`packages/hello-so/pkg.json`. Executable-only schema-1 packages are rejected by
live pkg; old fixtures stay separate under `build/legacy-packages`.
`$HOME/.mysh/pkgs/NAME-VERSION` retains immutable payloads;
`.active/NAME` stores exact version bytes via locked atomic rename. Install
selects new versions; `pkg use NAME VERSION` reselects retained versions;
remove deactivates/hides while retaining payloads. A distinct previously
published version stays mapped/callable until restart; selection persists for
the next process. There is no purge or mapped-file replacement.

The full release is `build/so-reviewed-releases/aishell-0.2.0`, including
bin/modules, generated help/catalog and example. Release membership must exactly match the
standard module set derived from authoritative source specs, not merely agree
with its own generated catalog. The module builder preflights stale/unknown/
unsafe outputs before cc and refuses them without deletion or overwrite.
Make tracks the example ABI/spec/command/helper dependencies. Old native-packages,
so-releases and intermediate so-candidates outputs remain immutable and outdated.

Install/use/remove post-commit close failures return 1 and diagnose
`selection committed; close failed; inspect pkg info before retry`, with no
success output. Committed remove still hides callbacks. A published payload
whose selection failed is reported retained, not selected.
The [demo](../../demo/PACKAGING.md) passed against existing outputs: **11
processes, 22 loop inputs, 27 checks**, exit 0, cleanup/hash checks passed,
including live discovery/removal/reselection and complete-release relocation.
The refreshed capture records 80 input SHA-256 entries (23 host/modules, 4
package, 53 release); the release matches current build/package bytes. Both
launch and child cwd/HOME were isolated with PATH=/nonexistent, and cleanup
passed. Source/build/scripts/packages/tests hashes stayed unchanged during demo.
The unchanged runner first failed on the old schema-1 package, providing RED
evidence. This demo does not test distinct-version replacement or all gates.

## Review and verification

Independent review reproduced four findings: incomplete standard releases,
stale module outputs, metadata/descriptor drift, and missing example-module
header dependencies. All were repaired with regression tests. The focused
rerun passed 23 adversarial probes; its final numeric-equivalence finding was
then reproduced by separate install/use tests and repaired. Comparison now
normalizes schema-valid numeric `-0` and `0`, retaining strict JSON types,
decoded strings, object membership, and ordered grammar arrays.

Parent `make -j1 all test lint release-shell` passed after that repair, as did
the 10 shared-package tests and the refreshed real demo. Native transaction
tests cover 8 cases and 212 measured project-allocation failure points; recorded
ASan+UBSan passes cover modules, loader, shell and native transactions. Parent
reruns also include the shared-package suite after the numeric-equivalence fix.
These are deterministic one-shot faults, not exhaustive crash scheduling or
Argtable-internal OOM recovery. Global formatting fails only on five unchanged
baseline files; changed C formatting and `git diff --check` pass.

Evidence lives under `$TMPDIR`: `aishell-so-parent-final.log`,
`aishell-negative-zero-red.log`, `aishell-negative-zero-green.log`,
`aishell-so-parent-sanitize-final.log`, and `aishell-so-parent-snapshot.json` with its
exact-source build/test/lint/release log. Required module-builder/bootstrap
scripts are explicitly unignored for source closure. Nothing is staged,
committed, pushed or merged for this redesign.

Limits: Linux /proc/self/exe, renameat2 and compatible ELF/libc; 512 lifetime
registry slots, 2048 dlopen observations, 64 selection trackers. Resource
exhaustion requires restart. Atomic selection is not a crash-durable journal or
repair system. Native callbacks and dependency-internal Argtable OOM may
terminate the host. Retained versions accumulate; no garbage collector exists.
