# date

```sh
./build/main date                 # Current local time
./build/main date -u              # UTC
./build/main help date
./build/date --help               # Optional standalone wrapper
```

- `-u`, `--utc`, `--universal`: UTC instead of local time.
- `-h`, `--help`: usage without reading the clock. Duplicate flags are idempotent.
- No operands; bare `--` is allowed. Invalid arguments override help.
- Local stdout: `YYYY-MM-DDTHH:MM:SS` plus newline. UTC adds `Z` before newline.
  ASCII numeric fields ignore locale; seconds only. Local timezone follows libc
  `TZ`/system configuration; offset-less local output is ambiguous at DST folds.
- Read-only: never sets the clock, changes environment/cwd/signals, or reads stdin.
- Status: 0 success, 1 operational/output failure, 2 invalid arguments;
  diagnostics on stderr. A failed write can leave a prefix. SIGPIPE is inherited.
- Years 0000–9999 only; negative epochs allowed when libc can represent them.
  Failed clock/calendar conversion is an error. POSIX leap handling is inherited;
  second 60 is rendered if supplied by libc, not synthesized.

Not GNU date's locale-dependent default. No format language, date parsing,
clock setting, file timestamps, fractional seconds, JSON or version mode.
Argtable internal OOM and interrupted private getopt scans are not recoverability
promises. Local mode refreshes libc timezone caches but does not change `TZ`.

[Contract, manual inventory and executed verification](../reports/date-implementation.md).
