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
```

`long_help` may be NULL. Registration requires a nonempty name, non-NULL summary,
run and usage callbacks. Specifications are borrowed for process lifetime;
registration does not copy or free them. Keep parser/allocation ownership local
to each invocation and follow each command's documented status/error contract.
`print_usage` returns void, so it cannot propagate a status through this API.

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
  returns 0 or -1. Repeated initialization is safe; this is not plugin discovery.
- `find_command`: exact-name lookup returns a borrowed spec or NULL; NULL names
  do not match.
- `for_each_command`: visits specs in insertion order with borrowed userdata;
  NULL callback is a no-op. Callbacks must not register commands while enumerating.

The registry is single-threaded. Borrowed argv/descriptors/exported getopt state,
parser cleanup and output errors need command-specific tests; registration itself
does not prove their preservation. Dependency-internal Argtable OOM and private
interrupted getopt scans are not general recovery guarantees. Supported flags,
JSON and exit statuses are in the [command guides](../README.md#supported-commands),
not a universal registry policy.
