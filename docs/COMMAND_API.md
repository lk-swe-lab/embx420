# Current command API

Authority: [`cmd_spec.h`](../src/cmd_spec.h),
[`module_abi.h`](../src/module_abi.h), [`module_runtime.c`](../src/module_runtime.c).
See [anatomy](COMMAND_ANATOMY.md) and the [workflow](ADDING_COMMANDS.md).

## Command specification

`cmd_spec_t` contains `name`, `summary`, nullable `long_help`,
`int (*run)(int, char **)`, `void (*print_usage)(FILE *)`, and optional
`int (*visit_metadata)(cmd_metadata_visitor, void *)`.
Names must begin with lowercase ASCII, contain only lowercase letters, digits
or hyphens, and be at most 60 bytes. Summary, run and usage must be non-NULL.
Specs are module-owned, borrowed for process life. Argv is borrowed for the
sequential invocation only; parser storage remains invocation-local.
`print_usage` is void: callers must inspect stream errors themselves.

The metadata visitor is `int (*)(void **table, void *context)`. Its provider owns
all entries/strings through the callback only; visitors neither mutate nor retain
them. Nonzero visitor result means failure; `visit_metadata` returns 0/-1.
`cmd_metadata_write(FILE *, const cmd_spec_t *)` borrows both inputs, returns
0/-1 and can leave partial output; stage it. Hello shares its argument builder
across parsing, usage and metadata. Other commands may omit structured metadata;
release pages use actual usage callbacks, not invented schemas. Native packages
must provide the visitor: before install/use selection, pkg compares stored
command JSON semantically with `cmd_metadata_write` on each loaded spec. Name,
summary, long help, usage and ordered argument/option grammar must match; JSON
whitespace/object member order are immaterial. Drift/missing visitors reject.

## ABI v1

One default-visible export:
`const aishell_module_v1 *aishell_module_entry_v1(const aishell_host_v1 *)`.
The descriptor has `size`, `version`, `spec_size`, `command_count`, borrowed
`commands`, and optional `refresh`. The host checks sizes/version, required
callbacks and conflicts before publication. Standard wrappers expose one command;
package validation requires exactly the manifest's one command per `.so`.

The host table has size/version/context plus `find`, `each`, `request_exit`,
`previous_status`, `activate`, `hide_package`, `identity`, and the appended
`const cmd_spec_t *(*inspect_command)(void *, const char *)` (header line 23).
This size-gated ABI-v1 extension loads a single-command module without publishing
it and returns its process-lifetime borrowed spec, or NULL on rejection. Current
wrappers require `host->size >= sizeof(*host)` and ABI v1 before binding.
Consult the header for exact signatures; context is currently reserved NULL.
Lookup and identity return process-lifetime borrowed values. `each` borrows visitor/data,
retains neither, and must not mutate the registry. `refresh` runs between
invocations, sequentially and nonreentrantly. All command callbacks run in the
parent, including ordinary file commands. There are no worker threads.

`activate(context, package, version, absolute_paths, expected_names, count,
publish)` borrows arguments, copying published identities. `publish=0` validates;
`publish=1` publishes the complete batch: 0 success, 1 replacement needs restart,
-1 invalid/conflicting/resource failure. Another previously published version is
not hot-swapped. Removal hides callbacks but never unloads their libraries.
Even handles opened during unsuccessful validation remain retained. Current
lifetime bounds: 512 registry slots, 2048 dlopen observations and 64 package
selection trackers; exhaustion requires restart.

`registry.h` is a private compatibility facade for direct parser tests, not a
module import API or static command inventory. Modules use the versioned host
table; the executable does not export command implementations with `-rdynamic`.

## Trust and errors

All standard modules intentionally load eagerly at startup, not on first use.
`dlopen` uses eager/local binding; native constructors can run before descriptor
validation, including during install preflight. Trusted code can crash, exit or
corrupt the host. ABI checks are not isolation or a sandbox. Argtable-internal
OOM may terminate the process. Status/error and stream contracts remain
command-specific; registration alone does not prove them. No PATH fallback.
Post-commit install/use/remove close errors return 1 and diagnose
`selection committed; close failed; inspect pkg info before retry`, without
success output. Committed removal still hides commands. A published payload with
failed selection is instead reported retained, not selected.
See [shell](commands/shell.md) and [packages](commands/pkg.md).
