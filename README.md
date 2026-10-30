# AiShell

A small host with all commands in loaded `.so` modules, including built-ins.
Standard modules ship with the release; `pkg` installs extra command packages.
No command code is linked into the host. No PATH fallback. Native code is trusted.

## Build and run

```sh
make
./build/aishell
./build/aishell --help
```

The main shell has no pipes. The [pipe sidequest](sidequests/lean-ai-shell/README.md)
adds foreground child processes and recursive pipes in a separate program.

## Demos

```sh
make package-hello release-shell
python3 demo/packaging-demo.py
make -C sidequests/lean-ai-shell test
./sidequests/lean-ai-shell/demo.sh
```

The pipe demo needs `strace` and an absolute, existing `TMPDIR`.

[Shell guide](docs/commands/shell.md) · [Packages](docs/commands/pkg.md) ·
[Build setup](docs/TESTING.md) · [Package demo](demo/PACKAGING.md) ·
[Status](docs/STATUS.md) · [AI log](ai_log/README.md) ·
[Earlier learning shell](sidequests/manual-shell/README.md)
