# pkg — trusted local shared-module packages

First enter `:load pkg` in the empty host. For one command, use
`./build/aishell --load pkg pkg ...`. Then use this grammar:

```text
pkg -h | --help
pkg list
pkg info NAME
pkg install [--] DIRECTORY
pkg use NAME VERSION
pkg remove [--] NAME
```

## Example and lifecycle

Prepare `make catalog`, set `AISHELL_CATALOG` as shown in [host use](../HOST.md),
then start `./build/aishell`.
Within that **same running shell**, enter:

```text
:load pkg
hello
pkg install build/catalog-0.3.1/hello-0.3.1
hello --name learner
:load help
help hello
help --json
pkg remove hello
hello
pkg use hello 0.3.1
hello --name restored
:quit
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

`make release-shell` creates `build/host-releases/aishell-0.3.0/` with no
command packages. `make catalog` builds a separate local collection. Pass its
catalog path to a copied host. See [host controls](../HOST.md).
Schema 3 packages store authoritative usage without a structured option grammar;
schema 2 retains its full metadata checks. Neither format makes native code safe.
The earlier module-bundled releases remain immutable historical artifacts.

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
