# Native package demo

## Prepare outside the demo

Build trusted outputs separately: `make all package-hello release-shell`.
Expected inputs: `build/aishell` (`build/main` alias), 22 `build/modules/*.so`,
`build/native-candidates/hello-0.2.0`, and `build/so-reviewed-releases/aishell-0.2.0`.
Earlier native-packages/so-releases outputs are preserved outdated artifacts.
The runner reads versions from `packages/hello-so/pkg.json` and
`packages/aishell/release.json`; it never builds, cleans or replaces artifacts.

## Simple live walkthrough

Use a fresh absolute, user-owned temporary HOME. From the repository root,
prepare a Bash subshell before launching aishell (TMPDIR must already exist):

```sh
bash
DEMO_HOME=$(mktemp -d "$TMPDIR/aishell-live.XXXXXX")
printf 'Temporary HOME: %s\n' "$DEMO_HOME"
HOME="$DEMO_HOME" PATH=/nonexistent ./build/aishell
```

Enter one line at a time **inside aishell**, without restarting:

1. `hello` — unknown command before install (expected 127).
2. `pkg install build/native-candidates/hello-0.2.0` — select the new module.
3. `hello --name learner` — greeting from the newly loaded command.
4. `help hello`, then `help --json` — live help and catalog identity.
5. `pkg remove hello` — hide it, retain immutable version bytes.
6. `hello` — unknown again (expected 127).
7. `pkg list` — hello 0.2.0 is retained, not active.
8. `pkg use hello 0.2.0`, then `hello --name restored` — same-version reactivation.
9. `exit` — return to Bash. Inspect `$DEMO_HOME` before removing your fixture;
   `exit` again leaves the subshell and restores the original environment.

A different version previously published in this process cannot be hot-swapped:
install/use persists its selection, but old callbacks remain until restart.
That distinct-version boundary is not exercised by this single-version demo.
All callbacks run sequentially in the parent, no threads. Only trusted native
code: constructors may execute during install validation; there is no sandbox.
All standard modules eagerly load at startup, intentionally not lazily. Native
install/use bind stored metadata to the actual spec exporter before selection.
A post-commit close error reports `selection committed; close failed`, returns 1
and prints no success; inspect pkg info before retry. Release membership is the
exact source-derived standard set; stale/unknown module outputs are refused
before compilation, never automatically pruned.

## Automated actual-output capture

From any cwd:

```sh
python3 /home/name/Documents/420-EMBDX/A/demo/packaging-demo.py
```

Requires an existing absolute TMPDIR and Python standard library only. It
prints a final summary, **not** a live streamed command walkthrough. It overwrites
only its sibling `packaging-demo.json` and `packaging-demo.log` evidence.
Children get only HOME, TMPDIR, LC_ALL=C and PATH=/nonexistent, with fresh
mode-0700 fixture homes; no host package state or credential environment is used.
The fixture is removed on success/handled failure and its absence checked.

The source-host session receives 16 literal input lines in one process:
absent hello → install → list/info/help/run/catalog → remove/list/catalog →
unknown → use/run → remove → unknown → exit. The final process status is 127;
three exact unknown diagnostics and actual stdout/catalogs are checked. Separate
one-shot invocations verify inactive payload retention, persisted selection,
fresh-session greeting and final deactivation. Intermediate loop command statuses
are **not individually exposed**; only actual process statuses are recorded.

A second fresh HOME and a copied complete release prove relocation outside the
repository: pwd, pkg help, and a six-input same-shell lifecycle with the included
example. Relocation is required, not silently skipped. No OS/libc portability
claim. All stdout/stderr is captured from subprocesses; none is synthesized.

JSON records actual argv, stdin, environment, cwd, 10-second timeout, stdout,
stderr and observed/expected process status. Host, all standard modules, full
bundle and release SHA-256 inventories are recorded and rechecked; copied and
installed bytes are compared. Before running, the release's host, all modules and
example must match the current build/package bytes. The capture records its
unrelated launch cwd and **80 SHA-256 entries** (23 host/modules, 4 package,
53 release). Catalog `builtin`/`control` values are compatibility
labels; all standard commands are physically shared objects.

## Observed migration checks

Before editing, the unchanged runner failed (exit 1) against the new host: its
schema-1 hello install was explicitly rejected. The migration RED capture is in
`$TMPDIR/packaging-demo-migration-red.json` for this development session. An
intermediate new-runner check caught the incorrect assumption of a newline in
`.active/hello`; source inspection confirmed exact version bytes, and it was
corrected. Final capture: **PASS: 11 processes; 22 loop inputs; 27 checks**,
exit **0**, cleanup verified and all expected statuses matched:
`0, 0, 127, 0, 127, 0, 0, 0, 0, 0, 127`. The refreshed runner itself was launched
from an unrelated scratch cwd with a fresh HOME and PATH=/nonexistent; that
outer fixture was also removed. No source/build/script/package/test bytes changed
during capture (590 hash entries compared). Python AST parsing passed without
bytecode generation.

Current host SHA-256:
`ae1b8dcb050ed78e2e13825ff90d7518a2ce23d79b014502249b6b087536c864`.
Native hello module SHA-256:
`7e749f3ef2b6de7470ec5d71b678d0642951ddab17526c24a42d864c77dcba38`.
The complete per-file inventories are in the JSON; these hashes identify the
captured bytes, not a completed independent review.

This is narrow lifecycle evidence, not final review, full regression,
publication or distinct-version rollback certification. Older reports describe
the executable-package predecessor. See [current pkg guide](../docs/commands/pkg.md).
