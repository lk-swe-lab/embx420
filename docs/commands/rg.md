# rg

```sh
./build/main rg [-n] [-i] [-w] [-C N] [--fixed-strings] [--json] PATTERN PATH...
./build/main help rg
# Optional standalone: ./build/rg with the same arguments.
```

- `-h`/`--help`: help; `-n`: line numbers; `-i`: ASCII case-insensitive.
- `-w`: ASCII letter/digit/underscore boundaries.
- `-C N`: decimal 0..INT_MAX lines either side; overlapping/adjacent groups merge.
- `--fixed-strings`: literal bytes instead of line-local POSIX extended regex.
- `--json`: one NDJSON object per non-overlapping match; ignores `-n`/`-C`.
- Use `--` before dash-prefixed patterns/paths. Paths are literal, '-' included.
- Argtable also accepts unambiguous prefixes of supported long options.

Text always prefixes the path; matching lines use `:`, context uses `-`,
separate context groups use `--`. Line numbers are 1-based. JSON fields:
`file`, `line`, `column` (1-based byte column), `index` (0-based file byte
offset), `text` (entire line without LF). JSON paths/content must be UTF-8.

Explicit files/directories only: **no implicit cwd or stdin search**. Trees
include hidden files; there are no ignore rules. Select safe roots explicitly.
Explicit symlinks/special files fail; discovered ones are skipped. NUL-containing
files are skipped entirely. Read-only, but atime can change. Input errors permit
later paths; output failure stops search. Status: 0 match/help, 1 none, 2 error.
The caller's SIGPIPE policy is unchanged; default SIGPIPE can terminate instead.

Not full ripgrep: C-locale byte matching, POSIX leftmost-longest regex, no Unicode
folding/PCRE2/lookaround/multiline. Empty matches advance one byte, including an
end-of-line match. Whole-file buffering, 64 MiB/file and 64 directory levels;
no concurrent-write snapshot or ancestor-symlink sandbox guarantee.

Verification: `make test-rg-impl test-shell`; [contract and evidence](../reports/rg-implementation.md).
