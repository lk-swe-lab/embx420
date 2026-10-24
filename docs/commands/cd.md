# cd

Status: integrated and tested in actual `main`; combined independent review
and final commit are pending. No standalone cd is built.

```text
cd [-h|--help] [--] [DIRECTORY]
```

Changes the invoking shell process's working directory, silently. With no
operand, uses `HOME`; unset or empty `HOME`, or an explicit empty operand,
fails without changing cwd. Paths use kernel symlink/`..` resolution.

- `-h`, `--help`: help only; no cwd change. Invalid syntax still fails.
- `--`: terminate options, e.g. `cd -- -directory`.
- At most one directory. `cd -` is unsupported (status 2); use `cd ./-` for
  a literal directory named `-`. A `HOME` value of `-` is a literal path.
- No `-L`/`-P`, `CDPATH` lookup, logical cwd, JSON, or environment updates.
  `PWD` and `OLDPWD` remain unchanged and may become stale; use `pwd`.

Feed multiple lines to the same main process:

```sh
printf 'cd "with space"\npwd\ncd ..\npwd\n' | ./build/main
```

A one-shot `./build/main cd DIRECTORY` cannot change its parent terminal.
Status: **0** success/help, **1** operational failure, **2** argument error.
Errors go to stderr. Dependency OOM limits and evidence are in
[the implementation report](../reports/cd.md).
[Actual combined integration](../reports/clear-cd-integration.md).
