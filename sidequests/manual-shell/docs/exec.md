# execv, execvp, and argv

`execv(path, argv)` identifies a program by pathname. `execvp(file, argv)` also accepts a bare name and can search the calling process's `PATH`. Both replace the current process image; neither creates a new child. This shell calls `fork` first so that replacement happens in the child, not in the shell. See the [POSIX exec family reference](https://pubs.opengroup.org/onlinepubs/9799919799/functions/exec.html).

The `p` variant does not search `PATH` when its file argument contains a slash. Our makefile embeds the containing repository's absolute `../../build` directory, and dispatch permits only `pwd`, `cat`, and `head` beneath it. `cd` and a hostile or missing `PATH` therefore cannot select a system utility. This pins the pathname, not the file contents: the local build directory must remain trusted. Relocating the project requires a clean rebuild.

`argv` is a NULL-terminated array of pointers to NUL-terminated strings. For `head -n 2 lines.txt`, the entries are `head`, `-n`, `2`, `lines.txt`, then NULL; `argv[0]` identifies the invoked command. The shell performs only whitespace splitting. The existing external program handles its own options; the shell neither links its command implementation nor adds an option-parsing dependency.

Successful exec never returns. On failure, the child reports the path error and calls `_exit`, using 127 for a missing path and 126 for other exec failures; it must not fall back into the input loop or flush a copied stdio buffer. The parent waits and continues. POSIX also specifies an interpreter fallback for `execvp` on certain non-executable-format files, so the pinned targets should remain the built binaries, not arbitrary text scripts.

```text
input:  head -n 2 lines.txt
argv:  [head] [-n] [2] [lines.txt] [NULL]
                       |
parent --fork--> child | execvp(".../A/build/head", argv)
                       v
                 same child PID, new program image
                 success: no return
                 error: diagnostic -> _exit(126 or 127)
```
