# Pipes: design and integration

## Contract

- Parse the full line before execution. Allow quotes and escapes.
- Limits: 32 stages, 256 arguments per stage, 65536 input bytes per line.
- Run ordinary module commands in children. No PATH lookup or exec yet.
- Run `cd`, `exit`, and `pkg` in the parent, alone. Reject them in pipes.
- Status: last stage; signal death is 128 + signal. Syntax is 2; unknown is 127.
- Share stdin without shell read-ahead. Prefer file operands in scripts.
- Ctrl-C stops children, not the shell. Ctrl-\\ can stop children.
- Ignore Ctrl-Z until job control exists. No background jobs or redirection.

## Ownership

`parse.c` borrows the line. `execute.c` borrows parsed arguments.
The shell loads modules before fork. Children inherit module memory.
Refresh package selection in the parent, never in a child.
Flush output before fork and before child `_exit`.

Each recursive call starts one stage, then starts the remaining stages.
Its input descriptor transfers to that call. The parent closes the old input
and new write end. The child uses `dup2` and closes all original pipe ends.
The final stage needs no new pipe. All stages start before any wait.
All children belong to the shell. Collect each PID with `waitpid`; retry EINTR.
On pipe/fork failure, close descriptors, kill started children, then collect them.
Block interrupts during construction. Forward pending INT/QUIT to all children
before restoring the parent mask. Children restore default signal actions.
Standard descriptors 0, 1, and 2 must be open at startup.

## Integration into final AiShell

1. Keep this experiment separate until review is complete.
2. Move parser and executor behavior into the host, not a command package.
3. Reuse the current module runtime; do not copy command implementations.
4. Review the ABI text: it currently promises parent-only callbacks.
5. Replace the three-name parent rule with explicit command execution metadata
   before adding more state-changing commands. The current ABI has no such field.
6. Transfer tests before replacing the main shell loop. Retain existing tests.
7. Add exec, redirection, process groups, then job control as separate changes.

The local build copies standard modules from `A/build/modules`. Run `make` after
module changes. A copied release needs both the executable and `modules/`.
Modules are trusted native code, not a sandbox. Threads are not supported.

## Evidence and sources

`make test` checks the real loop, large streams, early reader exit, status,
package changes, signals, descriptor reuse, and injected system-call failures.
Test modules and fault hooks are not included in the normal shell release.
See [test cases](tests/test_shell.py).

Checks on 2026-10-29: `make sanitize` passed all 23 tests in both builds.
The host used ASan/UBSan; the copied command modules were not instrumented.
Clang analyzer and changed-C format checks passed. Main-shell and package
regression targets passed. The wiki draft was read back; validation was clean.

Class 5: `Process Management.pptx`, slides 13 and 17–18, in the Session 5 ZIP.
Slide 18 uses a loop, not recursion. Slide 17 waits before the next reader starts;
that order can block on large output. This experiment uses recursive construction
and waits only after every stage has started.
