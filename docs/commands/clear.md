# clear

**Status:** integrated and tested in actual `main`, with an optional standalone
wrapper. Combined independent runtime review passes; final delivery is conditional.

```text
main clear
main clear -h|--help
main help clear
build/clear [--]          # optional standalone form
```

- With terminal stdout, writes exactly `ESC[H ESC[2J` (seven bytes, no space or
  newline): home the cursor, erase the visible screen, retain scrollback.
- Requires nonempty `TERM` other than `dumb` **only for terminal output**.
  Other values are assumed ANSI-compatible; there is no capability lookup.
- Valid nonterminal stdout is a silent success. A closed stdout is an error.
- `-h` / `--help` prints usage without clearing, independent of `TERM`.
- No operands; `--` alone is accepted. Invalid options, operands and repeated
  help are rejected before screen effects, including when combined with help.
- Status: **0** success/help, **1** operational failure, **2** argument error.
  Diagnostics go to stderr.

No `-T`, `-V`, `-x`, JSON, ncurses/terminfo dependency, subprocess, or scrollback
reset. The command borrows stdin/stdout/stderr and does not read stdin.

[Implementation, tests and evidence](../reports/clear.md).
[Actual combined integration](../reports/clear-cd-integration.md).

[Combined review and delivery conditions](../reports/clear-cd-review.md).
