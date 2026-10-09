# pwd

Print the physical current working directory, ignoring environment `PWD`.

```sh
./build/pwd [-P|--physical] [--json]
./build/pwd --help
```

- `-P`, `--physical`: explicit physical mode (the default).
- `--json`: `{"cwd":"..."}`; requires a valid UTF-8 path.
- `-h`, `--help`: show help.
- No operands or logical `-L` mode. Raw output is the physical path plus LF;
  an unavailable current directory is an error. Use `./build/pwd` explicitly
  to avoid a shell builtin.
- Status: 0 success, 1 operational/encoding failure, 2 argument error.
  Invalid arguments take precedence over help.

[Historical inventory and verification snapshot](../reports/pwd.md).
