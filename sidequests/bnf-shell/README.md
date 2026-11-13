# BNFC shell sidequest

A small C shell for the notebook 07 language features and Session 06 execution
model. This is a learning subset, **not a POSIX shell**. It is isolated from the
assignment's main shell and command-registry ABI.

## Build and run

Requirements: BNFC 2.9.6.1, flex, bison, POSIX threads, gcc-13, Python 3, and the
external utilities used in tests (`cat`, `wc`, `seq`, `head`, `sleep`, `true`,
`false`, `pwd`, `printenv`, `touch`). Tests target Linux, including `/proc` and
`/dev/full`. The local default GCC 15 has an assembler compatibility problem;
this Makefile selects gcc-13. You can override it with `make CC=...`.

```sh
make
./build/shell                 # Type one complete input per line; EOF ends it.
make test                    # Grammar, driver, and runtime tests.
make sanitize                # ASan, LeakSanitizer, UBSan on driver/runtime.
make demo                    # Checked demo with temporary file fixtures.
./demo.py                    # Same demo after a build.
make clean && make test      # Rebuild all generated files and run all tests.
```

The demo uses `tempfile`, which honors `TMPDIR`. It does not write fixtures in
the source tree. See [GRAMMAR.md](GRAMMAR.md) for diagrams and a concrete AST.

## Supported input

```text
echo hello world | cat | wc -w
cat < input.txt > output.txt ; cat < output.txt
sleep .1 & echo foreground
x=5 ; echo $x
export x
printenv x
echo count `cat input.txt | wc -l`
if true ; then echo yes ; fi
if false ; then echo skipped ; fi
hello
cd ./some-directory
exit 0
```

- A `Word` contains letters, digits, or `_ . / - :`. It is not quoted.
  `if`, `then`, and `fi` are reserved tokens, even in argument positions.
- A pipeline has one or more commands, with `|` between commands.
- Arguments are literal words, `$name`, or backquote pipelines. Each expansion
  produces **one argument**, even if its value is empty or contains whitespace.
  There is no field splitting, concatenation, glob expansion, or reparsing of
  expansion text. Command names and redirection paths are literal words only.
- `<` and `>` belong to each command. Put them after its arguments. One input
  and one output file are permitted in either order. Output is truncated.
  A redirection replaces that stage's pipe endpoint.
  FIFO (named-pipe) redirection is rejected without waiting for another opener.
  System pipes created for pipeline stages remain supported.
- `;` waits for a pipeline before the next input. `&` starts a pipeline in the
  background. A final `&` is allowed; a final `;` is not.
- Assignments use a literal nonempty `Word` value. They end a line, or precede
  another input after `;`. Names must match `[A-Za-z_][A-Za-z0-9_]*` at runtime.
  Local values override inherited values. `export name` passes the current
  value to later child programs; later assignments retain its export flag.
  An inherited environment name remains exported when assigned. Unset values
  expand to an empty argument. `export name=value` is not supported.
- Backquotes use this runtime, not `/bin/sh`, `system`, or `popen`. They accept
  a pipeline with literal/variable arguments and per-command redirection.
  Trailing newlines are removed; embedded newlines are retained. A nonzero
  last-stage status, NUL byte, or output-limit error prevents the outer
  pipeline from starting. Standard input defaults to `/dev/null` in a capture.
- The one-line conditional is exactly `if Pipeline ; then Pipeline ; fi`.
  It runs the body only when the condition status is zero. A skipped body
  returns zero; an executed body returns its last-stage status. No `else`,
  nested conditional, or body job sequence is supported. A conditional can
  be the final input after `;` or `&`, not a pipeline stage or a sequence prefix.
- `cd [path]`, `export name`, and `exit [0..255]` run in the main thread.
  `cd` without a path uses `HOME`. `exit` without a number uses the preceding
  command status. They are rejected in pipelines, background jobs, and
  substitutions. Invalid arguments return failure without changing state.
- `echo` prints arguments with spaces and a newline. It has no options or
  escape processing. `hello` prints a fixed greeting and ignores arguments.
  These two commands use a small function-pointer registry and worker threads.
  `cat`, `wc`, `true`, and `false` are external programs, not new built-ins.

## Execution and ownership

`Grammar.cf` produces a typed C AST. `Input` is the **only public entry point**;
this avoids the BNFC C multiple-entry-point result-union problem. `psInput`
parses each line and `free_Input` releases it after evaluation.

Preparation copies argument strings, expands variables/subcommands on the main
thread, opens redirection files, and makes system pipes. Every stage starts
before any stage is waited for. Internal commands receive explicit descriptors;
no worker performs process-global `dup2`, changes directory, or changes shell
variables. External commands use `posix_spawnp` with child-only file actions.
There is no unsafe C-library work in a fork child while threads are live.

Descriptors are owned by one stage and use close-on-exec. A worker closes its
own descriptors before publishing completion. Main closes each process stage's
parent descriptors after spawn. This releases pipe writers for EOF. Foreground
waits use exact child PIDs; they cannot reap another job's child by accident.
The shell ignores SIGPIPE and reports internal write errors through status;
spawned programs have the default SIGPIPE behavior restored.

Background plans own their strings and do not retain AST pointers. Their stdin
is `/dev/null` unless redirected. Main polls their child PIDs and worker
completion at command/input boundaries. **EOF and exit wait for all background
jobs** and release them. This is bounded basic tracking, not full job control:
no process groups, terminal transfer, `jobs`, `fg`, `bg`, or signal forwarding.
A finished child can remain a zombie while the shell is idle on input, until
the next boundary. There is no asynchronous reaper thread or signal handler.

Capture reads the pipe **before** waiting, so output larger than a pipe buffer
cannot cause a wait-before-read deadlock. A capture error closes its reader,
then joins/reaps its stages. As with a foreground command, a program that never
exits or ignores a broken output stream can still require external termination;
there is no command timeout or cancellation facility.

A preparation error starts no outer pipeline stage. Preparation is not a file
transaction: a previously opened output can already be truncated, and an earlier
substitution can already have run. Errors after launching starts do not roll
back other stages; all started stages are joined/reaped.

`prepare_parser.py` adds Bison error-path destructors to generated parser rules.
An untyped private start wrapper transfers the accepted `Input` to its caller;
syntax-error stack contents are freed. It also updates BNFC's deprecated Bison
pure-parser directive. It does not add accepted syntax or a public entry point.
Do not edit generated `build/` files by hand.

## Limits and statuses

| Resource | Limit |
| --- | --- |
| Input line, excluding newline | 65,536 bytes; NUL is rejected |
| Pipeline | 64 stages |
| Argument vector | 256 strings including command name |
| Local variable table | 128 distinct names |
| Tracked background jobs | 32 live/unreaped jobs |
| Captured stdout before newline removal | 65,536 bytes; NUL is rejected |

Allocation, open, and limit failures return nonzero. A missing external command
returns 127; another spawn error returns 126. Signal status is 128 plus signal
number. Pipeline status is the **last stage's status**, not `pipefail`. Sequence
and multi-line script status is the last evaluated command status. This replaces
the checkpoint's sticky runtime-failure status. Syntax and input errors remain
sticky: the shell recovers on the next line but finally returns 1 even when a
later command succeeds. Background completion does not change shell status;
accepted background launch returns zero, even if that job later fails to spawn.

## Deliberate exclusions and verification scope

No general quoting, escaping, comments, globbing, append/stderr redirection,
`&&`, `||`, arithmetic, `${...}`, `$?`, `$()`, nested backquotes, loops,
functions, full job control, natural-language `@` mode, or JSON command catalog.
The main assignment registry is sequential and has a different interface; this
sidequest does not change or link it.

The test suite uses real parser/runtime processes and stdlib `unittest`, with
large-output, exact-limit, error-path, `/proc` reaping, stdin ownership, and
background lifetime tests. Sanitizers include generated parser code. External
utility behavior is not reimplemented or exhaustively tested. No allocator or
pthread failure injection, thread sanitizer run, interactive terminal job-control
suite, or third-party property-testing dependency is included. Finite boundary
and malformed-input tests cover this constrained grammar instead.
