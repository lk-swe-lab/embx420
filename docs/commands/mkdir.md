# mkdir

Create one or more directories; successful text output is silent.

```sh
./build/mkdir [-p|--parents] [--json] [--] DIRECTORY...
./build/mkdir --help
```

- `-p`, `--parents`: create missing parents; accept existing directories,
  including symlinks to directories. Without `-p`, existing entries fail.
- `--json`: ordered array of `path`, `status` (`ok`/`error`), `message` objects;
  success message is null. All paths must be UTF-8 before any creation.
- `-h`, `--help`: show help without creating anything. `--` ends options.
- Final mode is 0777 filtered by inherited umask; new intermediate parents
  additionally gain owner write/search. Existing directory modes are unchanged.
- Continue after independent operand failures; completed work is not rolled back,
  including when JSON output fails. No `-m`, verbose or security-label options;
  supported flags at most once. No hostile concurrent-path replacement guarantee.
- Status: 0 success, 1 operational/encoding failure, 2 argument error.
  Invalid arguments take precedence over help.

[Historical inventory and verification snapshot](../reports/mkdir.md).
[Independent review snapshot](../reports/mkdir-review.md).
