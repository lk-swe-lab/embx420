# head

Print the first 10 LF-delimited lines of one file or stdin.

```sh
./build/head [-n N|--lines=N] [--json] [--] [FILE]
./build/head --help
```

- `-n N`, `--lines=N`: unsigned decimal count, including zero; last count wins.
- `--json`: `{"path":"...","lines":["..."]}`; stdin path is `"-"`.
- `-h`, `--help`: show help. `--` ends options.
- Omitted FILE or `-` reads stdin. Stops after N lines; raw output preserves
  selected bytes and missing final LF. JSON strips one terminating LF per line
  and requires valid UTF-8 in the path and selected content.
- One input only; no byte counts, negative counts, suffixes or filename headers.
  Status: 0 success, 1 operational/encoding failure, 2 argument error.
  Invalid arguments take precedence over help.

[Historical inventory and verification snapshot](../reports/head.md).
