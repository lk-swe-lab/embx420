# Shared-module shell (`aishell`, alias `main`)

```sh
./build/aishell                  # interactive when stdin is a terminal
./build/aishell pwd              # one-shot; calling shell supplies argv
./build/aishell head -n 2 FILE
./build/aishell --help           # current runtime command inventory
./build/aishell --commands-json  # sorted catalog; no extra operands
./build/aishell help touch
```

All 22 standard commands are `.so` modules, including help, exit, cd, pkg and
traverse. The host has no static command fallback. Seventeen standard commands
also have standalone wrappers; hello is an optional package, not standard.
All standard modules intentionally load eagerly at startup, not on first use.
They resolve beside the actual executable through `/proc/self/exe`, never
cwd/PATH. Copy `bin/modules` with a relocated release. Catalog `builtin` and
`control` are compatibility labels, not physical linkage claims.

No arguments: one command per line, terminal-only `main> ` prompt on stderr.
`help [COMMAND]` shows live usage; `help --json` emits the live catalog.
`exit`/EOF return the last nonblank status (initially 0). Errors continue:
unknown command 127, syntax 2, resource/I/O failure 1; command statuses propagate.
Callbacks execute sequentially in this process, without threads or fork/exec.
`cd` changes its physical cwd persistently, without environment updates.
`clear` writes ANSI bytes only to terminal stdout; redirected output is silent.

Whitespace separates arguments. Single/double quotes group even empty text;
adjacent fragments concatenate. Backslash escapes the next byte except inside
single quotes. No multiline quotes/continuation. Unclosed quotes, dangling
escapes, NUL and unquoted `| & ; < >` reject the whole line. Quoted/escaped
operators, `$`, backticks, glob characters and `#` are literal. No expansion,
comments, substitution, pipes, redirects, jobs, assignments or PATH fallback.

[Pkg](pkg.md) refreshes active selections between invocations. First install
can add commands to an already running shell; removal hides them. Immutable
versions remain installed; `pkg use NAME VERSION` reselects one. Replacing a
version already published in this process requires restart: the persisted
selection can differ from current callbacks until then. Help/catalog derive from
loaded descriptors; stored package metadata must match those specs before
install/use selection. A post-commit close failure returns 1 and reports
`selection committed; close failed`, not success: inspect pkg info before retry.
Corrupt unrelated package state is diagnosed without disabling standard commands;
absent pkg.so needs
external restoration, not a hidden host package manager.

**Shared stdin:** no reader read-ahead. `head -n 1` consumes one data line, then
parsing resumes. Stdin cat and positive-count/default tail consume through EOF;
`tail -n 0` reads nothing. Prefer file operands in scripts. Interactive stdin
commands use terminal EOF, not a special shell delimiter.

**Traversal:** `aishell -r DIR ...` routes to traverse.so; exact `-h` shows its
legacy help. `--help` lists commands. Time selectors affect dates/histograms;
last selector wins. Legacy diagnostics can differ from historical libc/getopt
wording because of dependency linkage; byte-identical old error text is not
promised. `traverse -r DIR` is also a module invocation.

Trusted native code is not a sandbox: constructors may run during install
validation; callbacks can crash, exit or corrupt the host. Dependency-internal
Argtable OOM may terminate it. Linux/compatible ELF/libc required. No POSIX-shell
or full GNU-tool compatibility. Retired env/echo/date/printf/sleep stay absent.
See [current status](../STATUS.md) and [redesign](../reports/so-redesign.md);
older reports preserve the previous architecture.
