# AiShell

A small host that starts with no command packages loaded.
The package catalog is separate. Select only the tools you need.
No command code is linked into the host. Native packages are trusted code.

## Build and run

```sh
make
make catalog
export AISHELL_CATALOG="$PWD/build/catalog-0.3.1/catalog.json"
./build/aishell
```

Use `:available`, `:load pwd`, `pwd`, `:loaded`, and `:quit` in the shell.
For an agent: `./build/aishell --load pwd pwd`.
See [host controls and limits](docs/HOST.md).

## Demos

```sh
python3 demo/catalog-demo.py
make -C sidequests/lean-ai-shell test
./sidequests/lean-ai-shell/demo.sh
```

The separate pipe demo needs `strace`. Both demos need an absolute, existing
`TMPDIR`. The main shell has no pipes or job control.

[Shell guide](docs/commands/shell.md) · [Packages](docs/commands/pkg.md) ·
[Build setup](docs/TESTING.md) · [Demo steps](demo/README.md) ·
[Status](docs/STATUS.md) · [AI log](ai_log/README.md) ·
[Earlier learning shell](sidequests/manual-shell/README.md)
