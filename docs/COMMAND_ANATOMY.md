# Current command anatomy

This describes the committed C project, not a transcription of course slides or
an implemented package system. See the [API](COMMAND_API.md) and approved
[add-command workflow](ADDING_COMMANDS.md) before changing a command.

- `src/cmd_<name>.c` owns one command's implementation and specification;
  its header declares that specification using the shared
  [`cmd_spec_t`](../src/cmd_spec.h), rather than defining another type.
- The specification supplies name, summary, optional long help, run callback
  and usage callback. Argtable definitions belong to each invocation; parser
  storage is not kept in the registry between calls.
- [`registry.c`](../src/registry.c) explicitly registers completed specifications.
  [`main`](../src/main.c) and the [shell](commands/shell.md) use the actual
  registry for discovery/help/dispatch; there is no external-command fallback.
- Optional `src/<name>_main.c` wrappers call the same module. `cd` deliberately
  has no standalone wrapper because cwd changes must persist in the host loop.
- CLI tests cover selected output/status/side-effect contracts; module tests
  cover repeated calls, borrowed state and fault cleanup. Integration tests
  must exercise real main, not merely the standalone executable.
- `docs/commands/` contains concise usage and limits; `docs/reports/` and
  `logs/commands/` preserve historical evidence. [STATUS](STATUS.md) tracks the
  live committed/paused/skipped distinction; reports are not rewritten to do so.

Adding files alone does not deliver a command: complete registry/build/test
integration, independent review and exact-index verification within the approved
scope. Package loading, generated package metadata and full GNU compatibility
are not established by the specification header's illustrative comments.
