# printf

```sh
./build/main printf '%s:%04d\n' label 7
./build/main printf -- '-%s\n' --help
./build/main help printf
# Optional standalone: ./build/printf FORMAT ARGUMENT...
```

`printf [-h|--help] [--] FORMAT [ARGUMENT...]`: print bytes without adding a
newline. Options precede FORMAT; later operands (including `--help`) are data.
A leading option-like FORMAT needs `--`.

- `%s`, `%c` (first byte, empty/missing is NUL), `%d`, `%i`, `%u`, `%o`, `%x`,
  `%X`, `%%`. Format repeats for remaining operands; missing fields get empty/0.
  Formats without conversions print once and silently ignore extra operands.
- Literal width up to 4096; `-` left alignment, integer `0` padding (`-` wins).
  String `.PRECISION` truncates bytes, at most 4096; bare `.` means zero.
- FORMAT escapes: `\\`, `\"`, `\a`, `\b`, `\e`, `\f`, `\n`, `\r`, `\t`,
  `\v`, 1–3 octal digits and `\x` plus 1–2 hex digits. Octal keeps the low byte.
  `%s` operands remain literal, including escapes/percent signs.
- Numbers: optional sign and decimal/octal/hex integer text; strict
  intmax_t/uintmax_t bounds. Reject whitespace, quoted characters, garbage,
  overflow and negative unsigned numbers. Empty/missing numbers mean zero.
- Validate all consumed operands/format and the 16 MiB total output bound before
  printing. Invalid input returns 2 with empty stdout; operational errors return
  1 and may leave a prefix. Success/help returns 0. Inherited SIGPIPE applies.

No floating point, variable widths, numeric precision, length modifiers, `%b`,
`%q`, `%n`, Unicode escapes, `\c`, JSON or `--version`. Unknown escapes are
errors; GNU differs on these exclusions, strict numeric errors and warnings for
unused operands. Locale does not change byte formatting; no environment changes.

[Contract and execution evidence](../reports/printf-implementation.md).
