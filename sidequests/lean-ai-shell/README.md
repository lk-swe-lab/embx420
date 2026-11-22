# Lean AI Shell

Separate pipe experiment. The main AiShell is unchanged.

From this directory:

```sh
make test
./build/lean-shell
./demo.sh
```

In the shell: `cat FILE | head -n 5`. Use `help` or `exit`.
The demo needs `strace` and an absolute, existing `TMPDIR`. Each step pauses
for five seconds. Check it with `make test-demo`.

See [design and integration](DESIGN.md).
