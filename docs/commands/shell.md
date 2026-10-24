# Builtin shell (`main`)

See [current status](../STATUS.md) for working-tree evidence and open delivery gates.

```sh
./build/main                 # Interactive when stdin is a terminal
./build/main pwd             # One-shot; arguments from the calling shell
./build/main head -n 2 FILE
./build/main --help          # 19 builtins, controls and installed commands
./build/main --commands-json # Sorted runtime catalog; no extra operands
./build/main help touch      # Usage; also: main touch --help
```

`pkg` lists, inspects, installs and removes local directory packages. See the
[brief guide](pkg.md). `hello` is package-only: install the generated example
before using it in main. It is not one of the 19 builtins or 17 builtin wrappers.
`env`, `echo`, `date`, `printf` and `sleep` are retired from the current baseline;
old guides and reports can contain stale links to them.

No arguments: one command per line; terminal-only `main> ` prompt on stderr.
`help [COMMAND]` shows usage. `exit`/EOF return the last nonblank command status
(initially 0). Command/syntax errors continue: unknown command 127, syntax/control
error 2, input/output/resource failure 1; builtin statuses propagate.

Whitespace separates arguments; single/double quotes group text (including empty
arguments); adjacent fragments concatenate. Backslash escapes the next byte except
inside single quotes, including inside double quotes. No multiline quotes or
continuation.
Unclosed quotes, dangling escapes, NUL and unquoted `| & ; < >` reject the whole
line without partial execution; quoted/escaped operators are literal. `$`,
backticks, glob characters and `#` are literal. No expansion, substitution,
comments, pipes, redirects, jobs, assignments or general external-command search.

Installed commands are discovered from fresh validated package snapshots, not
PATH. Help renders their stored metadata without running the payload. Execution
uses fork/execv/waitpid with the recorded absolute path and inherited streams.
Child exit status is preserved; signal termination returns 128+signal, launch
failure 126, unknown command 127, and package-state/system failure 1. Store locks
and descriptors are released before fork. Known builtin help/dispatch do not
need valid package state. Install/remove changes are visible in the same loop.
See [integration evidence](../reports/package-shell.md). Packages are trusted
local binaries, not sandboxed code. The [shell release](../reports/shell-release.md)
is separate and is not a command-package payload.

`cd` changes this process's physical cwd persistently; no environment updates
or standalone cd. `clear` emits ANSI screen-clear bytes only to terminal stdout;
valid redirected stdout is a silent success. See [cd](cd.md), [clear](clear.md)
and [combined integration](../reports/clear-cd-integration.md).

**Shared stdin:** no reader read-ahead. `head -n 1` consumes one data line, then
parsing resumes. Successful stdin cat and positive-count/default tail consume
remaining input through EOF; `tail -n 0` reads nothing, so following `pwd` executes.
Help/errors/file operands need not read stdin. Prefer files in scripts;
interactive cat/positive tail need terminal EOF, not a shell-specific delimiter.

**Legacy:** `main -r DIR ...` and exact `-h` help remain supported; standalone
`--help` now lists builtins. Valid legacy stdout/status remain verified. Static
argtable3 interposes getopt: invalid-option stderr deliberately differs from
libc in prefix/wording/punctuation (missing `-r`: `main: option requires an
argument -- r`, without quoted `r`). Errors still return 2/empty stdout;
byte-exact old diagnostics are not promised.

Not a POSIX shell, LLM client or sandbox. Existing head/tail `COMMAND --help`
may repeat `[-n N]`; `help COMMAND` renders one. Silent usage-callback/parser OOM,
exhaustive fuzzing and huge inputs are not verified; signals are inherited.
See [implementation/plan](../reports/shell-integration.md),
[independent review](../reports/shell-review.md) and
[accepted dispositions/readiness](../reports/shell-disposition.md).
