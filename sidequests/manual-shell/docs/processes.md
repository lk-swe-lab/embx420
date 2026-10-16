# fork, wait, and inherited state

`main` reads one line, `tokenise` splits it, and `separateCommands` fills the original `Command` structure. `execute_command` handles `cd`, `prompt`, and `exit` in the parent. Running `cd` in a child would change only that child's working directory, not the shell's directory for the next line.

For `pwd`, `cat`, or `head`, `fork()` creates a child. The return is zero in the child, the child's PID in the parent, or minus one on failure. The child inherits the current directory, environment, and open descriptors. Its descriptor entries refer to the same open file descriptions, so reading inherited stdin can consume the shell's future input. See the [POSIX fork reference](https://pubs.opengroup.org/onlinepubs/9799919799/functions/fork.html).

The child replaces its program with `execvp`; the parent calls `waitpid(pid, &status, 0)` for that exact child, retrying `EINTR`. This is a foreground shell: it cannot start the next command until the child terminates. Waiting also collects termination status instead of leaving an unreaped child. `WIFEXITED` and `WEXITSTATUS` decode normal completion; signal termination maps to `128 + signal` here. See [POSIX wait/waitpid](https://pubs.opengroup.org/onlinepubs/9799919799/functions/wait.html).

A failing command is not an instruction to stop the shell. Its nonnegative status is distinct from the negative `SHELL_EXIT` control result. `strace -f -e trace=process` shows actual PIDs, executable paths, and waits without adding debug output to production code. Linux may display `clone` and `wait4` for the C `fork` and `waitpid` calls. Job control and custom signal handling are deliberately deferred.

```text
parent shell (cwd X)
  |-- cd Y ----------------------> parent now has cwd Y
  |-- fork ----------------------> child (cwd Y, inherited fd 0/1/2)
  |                                  | execvp(A/build/pwd, argv)
  | waitpid(child PID, ..., 0)        | print Y; terminate
  |<---------------------------------+
  +-- collect status; read next line
```
