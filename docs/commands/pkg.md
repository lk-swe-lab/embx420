# pkg — trusted local shared-module packages

Use `./build/aishell pkg ...`, or enter these in its loop:

```text
pkg -h | --help
pkg list
pkg info NAME
pkg install [--] DIRECTORY
pkg use NAME VERSION
pkg remove [--] NAME
```

## Example and lifecycle

Prepare `make package-hello` before the demo, then start `./build/aishell`.
Within that **same running shell**, enter:

```text
hello
pkg install build/native-candidates/hello-0.2.0
hello --name learner
help hello
help --json
pkg remove hello
hello
pkg use hello 0.2.0
hello --name restored
exit
```

The first and post-removal hello are unknown (127). Install publishes the new
command without rebuilding/restarting. Remove hides it, but keeps immutable
versions. Use reactivates that retained same version. For isolated HOME and
actual captured output, use the [demo](../../demo/PACKAGING.md).

- `packages/hello-so/pkg.json` owns identity/version/inventory; schema 2 bundles
  contain `lib/COMMAND.so`, generated docs and `metadata/commands.json`.
  Source command specs own actual callbacks/help/catalog. Before install/use
  selection, stored command metadata must semantically match the loaded spec's
  `cmd_metadata_write` output, including summary/help/usage and ordered grammar.
  Whitespace/object key order do not matter; drift/missing visitors reject.
  Executable-only schema-1 packages are explicitly rejected by the live shell.
- Store: `$HOME/.mysh/pkgs/NAME-VERSION`; `.active/NAME` is a regular file with
  the exact version bytes (no newline). HOME must be absolute for pkg operations.
  Writes use the package lock and atomic selection rename. No standard-command
  replacement or overwriting an installed version directory.
- Install selects the new version and retains older ones. List/info mark every
  retained version `active` or `retained`; active means persisted selection.
  `pkg use NAME VERSION` selects an installed retained version for rollback.
- If another version was already published in this process, selection persists
  but the old callbacks stay until restart; a diagnostic says restart required.
  Removal always hides commands. There is no hot unload or purge command.
- Builders do not increment versions. Existing generated outputs are immutable:
  identical bytes are a no-op; changed bytes require a new version/output root.

## Full-shell release

`make release-shell` creates `build/so-reviewed-releases/aishell-0.2.0/`, defined by
`packages/aishell/release.json`. Copy the **complete** release, including
`bin/modules/`, to a user-owned versioned directory and launch `bin/aishell`.
It includes generated docs/catalog and `examples/hello-0.2.0`. Keep the old
release and explicitly switch launchers for rollback. It is not a `pkg install`
payload; there is no self-update or automatic sudo installation. Standard modules
load eagerly at startup, not lazily. The builder requires exactly the standard
module set derived from source specs; module builds refuse stale/unknown/unsafe
outputs before compiling, never delete them. Historical native-packages and
so-releases directories remain immutable, not current release inputs.

## Trust and limits

All callbacks run sequentially in the parent. Install validation uses dlopen:
**native constructors may execute before ABI validation**. Only trusted local
code; no sandbox, signatures, downloads or dependency solver. Exact inventories
reject symlinks, hardlinks, special files and unlisted entries, but do not make
native code safe. Never modify mapped versions while sessions run.

A post-commit close failure returns 1 with `selection committed; close failed;
inspect pkg info before retry`, never a success line. Removal still hides
deactivated commands. If only payload publication committed, failure instead
reports `release retained; selection failed`. Inspect list/info before retrying.
Atomic visibility is not a crash-durable journal or repair facility.
Pkg statuses: 0 success, 1 state/validation/I/O failure, 2 syntax. Unknown shell
commands return 127. See [current redesign](../reports/so-redesign.md);
older packaging reports describe the superseded executable backend.
