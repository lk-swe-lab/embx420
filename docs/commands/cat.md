# cat

Concatenate files to stdout in operand order, without added separators.

```sh
./build/cat [-n|--number] [--] [FILE...]
./build/cat --help
```

- `-n`, `--number`: number every output line continuously across files.
- `-h`, `--help`: show help. `--` ends options.
- No files, or a `-` operand: read stdin. Raw copying preserves binary bytes
  and missing final newlines; numbering is opt-in.
- Input failures diagnose and continue with later files; output failures stop.
  Same regular input/output file is rejected.
- No JSON or other GNU display transformations. Status: 0 success,
  1 I/O failure, 2 argument error; invalid arguments take precedence over help.

[Historical inventory and verification snapshot](../reports/cat.md).
