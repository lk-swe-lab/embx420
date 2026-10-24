# Current command API

Source authority: [`cmd_spec.h`](../src/cmd_spec.h),
[`registry.h`](../src/registry.h) and [`registry.c`](../src/registry.c).
Use the [workflow](ADDING_COMMANDS.md) and [anatomy](COMMAND_ANATOMY.md) for changes.

## Specification

The current `cmd_spec_t` fields and callback types are:

```c
const char *name;
const char *summary;
const char *long_help;
int (*run)(int argc, char **argv);
void (*print_usage)(FILE *out);
int (*visit_metadata)(cmd_metadata_visitor visit, void *context);
```

`long_help` may be NULL. Registration requires a nonempty name, non-NULL summary,
run and usage callbacks. Specifications are borrowed for process lifetime;
registration does not copy or free them. Keep parser/allocation ownership local
to each invocation and follow each command's documented status/error contract.
`print_usage` returns void, so it cannot propagate a status through this API.

## Optional metadata

`cmd_metadata_visitor` is `int (*)(void **table, void *context)`. A provider
passes a complete, terminated Argtable with logical grammar counts. The provider
owns every entry and string through the callback only. The visitor must not
change or retain them; nonzero means failure. `visit_metadata` returns 0 on
success, -1 for a NULL callback, allocation failure, or visit error. The hello
provider frees its table on every path. Other builtins can leave this member NULL.
This does not migrate all builtin parsers to structured metadata.

[`cmd_metadata_write(FILE *, const cmd_spec_t *)`](../src/cmd_metadata.h) borrows
the stream and specification. It writes one JSON command object and returns 0
after flush, or -1 for missing metadata, allocation or output error. Failure can
leave partial bytes; callers must stage output. Hello uses one argument builder
for parsing, help and metadata. See [package generation](reports/package-build.md).

## Registry

Current exported signatures:

```c
int register_command(const cmd_spec_t *spec);
int register_all_builtin_commands(void);
const cmd_spec_t *find_command(const char *name);
void for_each_command(void (*callback)(const cmd_spec_t *, void *),
                      void *userdata);
```

- `register_command`: 0 on success, including re-registration of the identical
  specification pointer; -1 for invalid data, a different spec with the same
  name, or full capacity. Current storage is 32 entries.
- `register_all_builtin_commands`: registers the explicit builtin array;
  returns 0 or -1. Repeated initialization is safe; this array is not
  package discovery.
- `find_command`: exact-name lookup returns a borrowed spec or NULL; NULL names
  do not match.
- `for_each_command`: visits specs in insertion order with borrowed userdata;
  NULL callback is a no-op. Callbacks must not register commands while enumerating.

Installed discovery is separate: [`pkg_store.h`](../src/pkg_store.h) supplies
invocation-owned snapshots. Load into unowned output; free before reload.
Snapshot text borrows its owned trees and must not enter the static registry.
The shell refreshes installed state, renders metadata help/catalog, and executes
the recorded absolute path with fork/execv/waitpid, not PATH search. Locks and
store descriptors are released before fork. See [integration](reports/package-shell.md).

The registry is single-threaded. Borrowed argv/descriptors/exported getopt state,
parser cleanup and output errors need command-specific tests; registration itself
does not prove their preservation. Dependency-internal Argtable OOM and private
interrupted getopt scans are not general recovery guarantees. Supported flags,
JSON and exit statuses are in the [command guides](../README.md#supported-commands),
not a universal registry policy.
