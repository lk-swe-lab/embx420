# env

```sh
./build/main env              # Read-only KEY=VALUE listing
./build/main env --json       # {"env":{"KEY":"VALUE",...}}
./build/main help env
# Optional standalone: ./build/env [--json] [-h|--help]
```

Keys are sorted bytewise; the first duplicate key wins. No operands,
assignments, execution, unset, or environment mutation. `--` alone is accepted.
Argtable accepts unique long-option abbreviations; repeated flags are errors.

Raw output preserves bytes (including newlines). JSON escapes controls and
requires valid UTF-8 in every entry; malformed entries (empty key/no `=`) or
invalid JSON encoding fail before output. Empty environment: empty raw output
or `{"env":{}}` plus newline. Values may contain `=`; split at the first one.

Statuses: **0** success, **1** operational/output error, **2** argument error.
An output failure can leave partial output; an empty raw listing does not write
and cannot detect a bad stdout. The caller must keep the environment stable;
no concurrent mutation/thread-safety guarantee. Do not share output containing
real environment secrets.

Verification: `make test-env` (GNU-compatible content fixtures only);
`make test-env-impl` (project/module/main/PTY). All fixtures use synthetic
values. See [implementation evidence and limits](../reports/env-implementation.md).
