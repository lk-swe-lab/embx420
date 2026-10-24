# Current command anatomy

This describes the current C project, not course slide text. See the
[API](COMMAND_API.md) and approved
[add-command workflow](ADDING_COMMANDS.md) before changing a command.

- `src/cmd_<name>.c` owns one command's implementation and specification;
  its header declares that specification using the shared
  [`cmd_spec_t`](../src/cmd_spec.h), rather than defining another type.
- The specification supplies name, summary, optional long help, run callback
  and usage callback, plus an optional metadata visitor. The provider owns its
  terminated Argtable and strings only through the visitor call; visitors must
  not change or retain them. Argtable definitions belong to each invocation; parser
  storage is not kept in the registry between calls.
- [`registry.c`](../src/registry.c) explicitly registers completed specifications.
  [`main`](../src/main.c) and the [shell](commands/shell.md) use the actual
  registry for builtin discovery/help/dispatch. Installed packages use separate,
  invocation-owned snapshots; their pointers never enter the static registry.
  Installed help uses stored metadata; execution uses fork/execv of the recorded
  absolute path. Unknown commands do not fall back to PATH.
- Optional `src/<name>_main.c` wrappers call the same module. `cd` deliberately
  has no standalone wrapper because cwd changes must persist in the host loop.
  `pkg` also has no wrapper. There are 19 builtins and 17 builtin wrappers.
  `hello_main.c` builds a separate package-only example, not a builtin.
- CLI tests cover selected output/status/side-effect contracts; module tests
  cover repeated calls, borrowed state and fault cleanup. Integration tests
  must exercise real main, not merely the standalone executable.
- `docs/commands/` contains concise usage and limits; `docs/reports/` and
  `logs/commands/` preserve historical evidence. [STATUS](STATUS.md) tracks the
  live working-tree/delivery distinction; old reports retain their checkpoints.

Adding files alone does not deliver a command: complete registry/build/test
integration, independent review and exact-index verification within the approved
scope. The implemented [package builder](reports/package-build.md) uses the
optional shared exporter for hello metadata and generated docs. Existing builtin
release pages use actual usage callbacks, not invented argument schemas. See
[package usage](commands/pkg.md), [integration](reports/package-shell.md) and
[release evidence](reports/shell-release.md). This is not full GNU compatibility.
