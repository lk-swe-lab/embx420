# echo

```sh
./build/main echo hello 'two words'
./build/main echo -- -n --help
./build/main help echo
./build/echo --help
```

`echo [-h|--help] [--] [TEXT...]` prints literal operands separated by one space
and followed by one newline. Zero operands print a newline; empty operands still
count. Backslashes, control bytes and invalid UTF-8 are not interpreted. Use `--`
before option-like text; it is not printed.

Help (`-h`/`--help`) suppresses text after successful parsing. Unsupported options,
duplicate help, or help plus invalid options fail without stdout. Unlike GNU echo,
`-n`, `-e`, `-E`, `--version`, escapes and JSON are unsupported. Embedded NUL cannot
be passed in argv; the shell rejects it before dispatch.

Status: 0 success, 1 output/allocation failure, 2 argument error. Stdin and caller
state are borrowed; output errors may leave a prefix. SIGPIPE is inherited.
Argtable internal OOM can terminate the host process; exported getopt restoration
does not preserve private interrupted scans or provide thread safety.

[Contract and verification](../reports/echo-implementation.md).
