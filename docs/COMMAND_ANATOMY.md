# Current command anatomy

The default host now starts empty and loads only selected catalog packages.
See [host controls and distribution](HOST.md). The eager startup described
below applies only to the retained `build/main` compatibility host and sidequest.
`build/main` is no longer an alias of `build/aishell`.

Source authority: [`cmd_spec.h`](../src/cmd_spec.h),
[`module_abi.h`](../src/module_abi.h), and `src/cmd_*.c` specifications.
Follow the [workflow](ADDING_COMMANDS.md) before changing commands.

- `build/aishell` hosts input, catalog reading, registry and dispatch.
  No command implementation is linked into it. All tools, including help,
  exit, cd, pkg and traverse, are separate packages. Hello is an example tool.
- `scripts/build_modules.py` derives names/spec symbols from command sources and
  wraps them with `src/module_entry.c`. Its regex recognizes the first matching
  `cmd_spec_t` initializer per `cmd_*.c`, not arbitrary C syntax. Dependency
  exceptions and hello's package-only selection remain explicit in the builder.
  Adding a source does not prove correct dependencies or integration. Before
  compiling, the builder refuses stale/unknown/unsafe output entries; it never
  prunes them. Legacy releases require the source-derived standard module set.
- The legacy host eagerly loads all standard `modules/*.so` at startup,
  beside its actual executable, resolved through
  Linux `/proc/self/exe`; not cwd or PATH. `RTLD_NOW | RTLD_LOCAL`, hidden
  implementation symbols and the ABI entry replace the deleted static registry.
- Specifications supply name, summary, optional long help, run/usage callbacks
  and optional metadata visitor. Parser allocations belong to each invocation;
  borrowed argv lasts only through the callback. All callbacks execute
  sequentially in the parent process; no worker threads or fork/exec isolation.
- `help`, `help --json` and `--commands-json` use live descriptors. Package
  metadata generates packaging docs and must match actual loaded specs before
  install/use selection; it is not runtime callback storage.
  Catalog `builtin`/`control` labels survive for compatibility, not static linkage.
- `pkg.so` uses the host API to inspect unpublished specs, validate/publish
  batches and hide commands. `inspect_command` is size-gated in ABI v1.
  Immutable versions and active selections live under `$HOME/.mysh/pkgs`.
  New packages appear live; replacing a previously published version requires
  restart. Removal hides commands, retaining versions and mapped handles.
- Seventeen standard commands retain standalone wrappers; cd, pkg, help, exit
  and traverse do not. `hello_main.c` is a separate example/legacy test tool.
- Tests must cover real host discovery, same-session changes, stateful cd/exit,
  standalone parity, ABI rejection, relocation and borrowed-resource cleanup.

Native modules are trusted code, not a sandbox; constructors may run during
install validation. See [API](COMMAND_API.md), [package usage](commands/pkg.md),
[current status](STATUS.md) and [redesign](reports/so-redesign.md).
Historical reports retain their original architecture and gate status.
