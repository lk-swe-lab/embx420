# ls

List one directory's contents or one non-directory object; default is `.`.

```sh
./build/ls [-a|--all] [-l] [-1] [--json] [--] [FILE]
./build/ls --help
```

- `-a`, `--all`: include hidden entries, `.` and `..`.
- `-l`: tab-separated permissions, numeric uid/gid, byte size, epoch mtime, name.
- `-1`: one raw name per line (default); sorting is by filename bytes.
- `--json`: array of `name`, `type`, `size`, `mtime` objects; names must be UTF-8.
  JSON wins over `-l`. Raw names can contain tabs/newlines.
- `-h`, `--help`: show help (not GNU human-readable sizes). `--` ends options.
- Final symlinks are not followed; intermediate links and trailing slashes use
  native resolution (`link/` lists its directory target). `-` is literal.
- One operand only; flags at most once; no recursion, color or GNU long layout.
  Status: 0 success, 1 operational/encoding failure, 2 argument error.
  Invalid arguments take precedence over help.

[Historical inventory and verification snapshot](../reports/ls.md).
