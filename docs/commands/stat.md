# stat

Inspect metadata for exactly one filesystem object; do not read its contents.

```sh
./build/stat [-L|--dereference] [--json] [--] FILE
./build/stat --help
```

- `-L`, `--dereference`: inspect a symlink target; default inspects the link.
- `--json`: object with `path`, `type`, `size`, `mode`, `mtime`, `atime`, `ctime`.
  Mode is an octal string; times are integer epoch seconds; path must be UTF-8.
- `-h`, `--help`: show help. `--` ends options.
- `-` is a literal filename, not stdin. Human output is a project summary,
  not GNU's layout. No multiple inputs, format language or filesystem statistics.
- Status: 0 success, 1 operational/encoding failure, 2 argument error.
  Invalid arguments take precedence over help.

[Historical inventory and verification snapshot](../reports/stat.md).
