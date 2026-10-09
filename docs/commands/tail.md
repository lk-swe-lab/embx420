# tail

Display the last N LF-delimited lines (10 by default) of one file or finite
stdin stream. Does not create or modify input files; missing inputs fail.

```sh
./build/tail [-n N|--lines=N] [--json] [--] [FILE]
./build/tail --help
```

- `-n N`, `--lines=N`: unsigned decimal count, including zero; last count wins.
- `--json`: `{"path":"...","lines":["..."]}`; stdin path is `"-"`.
- `-h`, `--help`: show help. `--` ends options.
- Omitted FILE or `-` reads stdin to EOF for positive N; N=0 reads nothing.
  Raw output preserves selected bytes and missing final LF. JSON strips one
  terminating LF per line and requires valid UTF-8 in path and selected content.
- One input only; no follow mode, byte counts, `+N` counts or suffixes.
  Memory grows with retained content and the current line.
  Status: 0 success, 1 operational/encoding failure, 2 argument error.
  Invalid arguments take precedence over help.

[Historical inventory and verification snapshot](../reports/tail.md).
