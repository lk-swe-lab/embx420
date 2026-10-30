# Run both demos

Run from `A/`. Set `TMPDIR` to an existing absolute directory.

## 1. Module shell and packages

```sh
make all package-hello release-shell
python3 demo/packaging-demo.py
```

Checks live package install, use, removal, and release relocation.
Prints a result summary; saves output in `demo/packaging-demo.log`.
For a live walkthrough, see [package steps](PACKAGING.md).

## 2. Pipes, fork, and foreground children

```sh
make -C sidequests/lean-ai-shell test
./sidequests/lean-ai-shell/demo.sh
```

Shows pipe output, parent directory changes, and real process calls.
Needs `strace`. Pauses five seconds per step.
The sidequest is separate from the main shell.
