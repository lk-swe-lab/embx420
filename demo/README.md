# Run both demos

Run from `A/`. Set `TMPDIR` to an existing absolute directory.

## 1. Empty host and selected packages

```sh
make
make catalog
python3 demo/catalog-demo.py
```

Checks empty startup, catalog listing, explicit loading, and a host-only copy.
Saves actual output in `demo/catalog-demo.log`. For live steps, see
[host controls](../docs/HOST.md). The old packaging capture is historical.
Use `--catalog FILE` to demonstrate another collection.

## 2. Pipes, fork, and foreground children

```sh
make -C sidequests/lean-ai-shell test
./sidequests/lean-ai-shell/demo.sh
```

Shows pipe output, parent directory changes, and real process calls.
Needs `strace`. Pauses five seconds per step.
The sidequest is separate from the main shell.
