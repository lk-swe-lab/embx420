# Manual and strtok tokenization

`getline` owns a reusable, dynamically sized line buffer in `main`. The reader checks EOF before indexing the buffer and does not discard the final byte of a line without a newline. Embedded NUL bytes are rejected using the returned byte length, preventing a hidden suffix from being silently ignored. Input buffering is disabled before reading so the shell itself does not prefetch bytes a child might need.

Build with `make TOKENIZER=manual` (the default) to have `tokenise(line, tokens)` scan characters directly. A local cursor skips delimiters, records the next word's starting address, and advances to the next delimiter or end of input. It replaces the delimiter ending each word with a NUL byte and continues. Space, tab, carriage return, newline, vertical tab, and form feed are recognized explicitly by `is_separator`.

The token pointers refer into that same line buffer. `Command.argv` borrows the token-pointer array rather than duplicating every string. Neither the tokens nor `argv` may be freed separately or retained across the next input read. The foreground wait finishes before the line is reused. The `prompt` builtin is the exception: it duplicates its word into independent owned storage because the prompt must survive later reads.

Build with `make TOKENIZER=strtok` for the library version: the first `strtok` call receives the writable line; subsequent calls receive NULL to continue scanning. Unlike the manual version, [POSIX strtok](https://pubs.opengroup.org/onlinepubs/9799919799/functions/strtok.html) retains internal scan state. Both versions reserve 1001 pointer slots for at most 1000 tokens plus a final NULL and reject overflow before execution. Neither interprets quotes, escapes, globbing, expansion, pipelines, sequences, or redirection. Changing modes requires no clean because make always rebuilds the token module.

```text
before: line -> "head -n 2 lines.txt\n\0"
after:  line -> [head][NUL][-n][NUL][2][NUL][lines.txt][NUL][NUL]
                 ^          ^        ^        ^
tokens:         [0]        [1]      [2]      [3] [NULL]
                 \__________________/
                 Command.argv borrows tokens
next getline may overwrite/reallocate line: do not keep these pointers
```
