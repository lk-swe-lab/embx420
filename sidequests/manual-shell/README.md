# Minimal learning shell

A deliberately smaller adaptation of the supplied `Unix_Shell.c`, `token.c`,
`command.c`, and `builtin.c` skeleton. It retains `main`, `Command`,
`tokenise`, `separateCommands`, `execute_command`, and builtin dispatch.
Broken deferred-feature code was removed rather than carried as dead machinery.

## Build and run

From this directory (requires the containing repository at `../..`):

```sh
make -C ../.. build/pwd build/cat build/head && make
make clean
make
./Unix_Shell
make test
./demo.sh
```

Choose tokenization at build time with `make TOKENIZER=manual` (default) or
`make TOKENIZER=strtok`. Test with `make TOKENIZER=manual test` and
`make TOKENIZER=strtok test`. Switching modes needs no clean: the small token
module is rebuilt on every make invocation. Both preserve the same interface,
whitespace rules, borrowed pointers, and capacity limit.

No containing-repository command sources, build definitions, or documentation
are modified or copied. This publication snapshot changes only layout paths,
the demo runbook, and publication documentation; the C skeleton is unchanged.
The shell executes **those existing binaries**, not callbacks or host utilities.
`make` pins their absolute build directory. Rebuild with `make clean && make`
after moving either project; command selection stays fixed across `cd` and PATH
changes. Build paths must not contain shell/make quoting metacharacters.

## Supported contract

- One foreground command per line; whitespace-only input is ignored. EOF and a
  final unterminated line are handled. Maximum **1000 tokens total**, including
  the command name; longer lines have dynamic storage but the same token bound.
- Parent builtins: `cd [DIRECTORY]` (HOME if omitted), `prompt WORD`, `exit`
  (no arguments, successful exit). Prompt output appears only on a terminal.
- External allowlist: `pwd`, `cat`, `head`; options/operands pass unchanged to
  `A/build` programs. Unknown names, including absolute executable names, fail
  with a diagnostic and leave the shell running.
- Command failures never mean "exit shell". EOF returns the last command/error
  status (initially zero); a valid `exit` returns zero. Syntax/usage errors use
  status 2; unknown commands use 127; exec failures use 127/126. Signal-killed
  children map to 128 plus the signal number.

Pipes, background jobs, sequences, redirection, quotes, escapes, globbing,
variables/substitution, tilde/brace expansion, comments, job control, and custom
signal handling are deferred. Tokens containing any of
``| & ; < > * ? [ ] " ' ` $ \ ( ) { } ~ #`` are rejected, even inside filenames.
This is not a complete implementation of the original assignment's grammar.
The original blanket signal mask was removed; normal inherited signal behavior
applies (for example, Ctrl-C can terminate the shell as well as its child).

Use **file operands for cat/head in scripted demos**. Children inherit fd 0;
a stdin-reading command may consume future shell lines as data. The shell
avoids its own stdio read-ahead but does not isolate command stdin, and the
external utility may buffer input itself. Arbitrary stdin/script interleaving
is not promised. No shell line-length limit is imposed beyond available memory
and the external exec argument-size limit.

## Demo and learning notes

`demo.sh` is runnable from another directory and holds each step for five
seconds. One session demonstrates pwd, parent cd, inherited cwd, file cat,
`head -n 2`, a missing-file diagnostic, recovery, and exit. A separate focused
`strace` section displays fork/exec/wait PIDs; strace is required for the demo.
The fixture is `fixtures/lines.txt`. The real terminal recording is
[`demo/manual-shell.mp4`](demo/manual-shell.mp4); provenance and verification
are in [`demo/README.md`](demo/README.md). The runbook also builds, runs both
16-test tokenizer suites, restores the manual default, and shows command help.

- [Processes and inheritance](docs/processes.md)
- [execv versus execvp](docs/exec.md)
- [Manual tokenization and ownership](docs/tokenization.md)

## Verification evidence

The following development observations describe the source snapshot's earlier
work. Publication-layout results and the real recorded run are independently
documented in `demo/README.md` and its raw evidence files.

Tests use Python's standard library and a real compiled main loop. Sixteen tests
cover EOF/blank/unterminated input, argument passing, cd/HOME/errors, independent
prompt ownership, status/control separation, 100 repeated commands, NUL
rejection, token boundaries, unsupported syntax, child exec failure (ENOENT and
EACCES), a real PTY prompt, and exact A/build exec paths plus per-child waits in
strace, a blocking FIFO that proves foreground ordering, and signal exit status.
The trace test skips when strace is absent; it ran here without skips.
The exec-failure test builds a private scratch shell rather than changing A/.

Incremental RED/GREEN observations: original EOF and unterminated `exit` timed
out; repairing the read/parse loop passed. External execution then failed with
missing cat/head output; adding pinned fork/exec/wait passed. Syntax and
whitespace boundary tests failed before the bounded tokenizer/rejection slice.
Missing prompt arguments segfaulted, a long prompt aborted with heap corruption,
and invalid builtin arguments were accepted before builtin validation/ownership
fixes. NUL input executed an unwanted prefix before whole-line rejection. Each
slice was followed by the accumulated passing suite; later trace/PTY/failure
checks characterize the completed behavior rather than claiming a pre-fix RED.
A final injected `getline` allocation failure incorrectly returned success before
the reader distinguished actual EOF from other failures; that regression passes.

A clean warning-free build and the original 14 tests passed, as did a separate GCC
`-Werror -fanalyzer` build and AddressSanitizer/UndefinedBehaviorSanitizer run.
LeakSanitizer initially failed under strace because ptrace is unsupported; only
the trace test disables leak detection, with the remaining sanitized tests
retaining it. The full demo ran from outside the project with real five-second
pauses, successful recovery after the missing file, and a completed PID trace.
Fork resource-exhaustion failures and EINTR branches were not fault-injected.

The manual-scanning regression first failed with status 99 when a linker wrapper
intercepted the previous `strtok` call. Replacing it with a local character scan
passes that check without calling `strtok`, `strtok_r`, or `strsep`; the complete
15-test suite was rerun after the correction.

The selectable-version test then failed because `USE_STRTOK=1` still used manual
scanning. Both implementations now pass the direct behavior/mechanism tests;
the full 16-test suite is verified separately with each make selection.

Generated binaries/objects, the supplied assignment docx, and core dumps are
ignored. No staging, commit, push, or repository creation is part of this work.
