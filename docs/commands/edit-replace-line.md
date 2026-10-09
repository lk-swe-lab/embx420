# edit-replace-line

Replace one existing 1-based line, staging the complete file before replacement.

```sh
./build/main edit-replace-line FILE N 'new text'
./build/main edit-replace-line --json FILE N 'new text'
./build/main help edit-replace-line
# Optional standalone: ./build/edit-replace-line FILE N TEXT
```

- `-h`, `--help`: parser-derived help, without editing.
- `--json`: one `{path,line,status,message}` object and newline. Status is
  `ok` or `error`; success message is `null`. Argument errors emit no JSON.
- `--`: end options; dash-prefixed FILE/TEXT and literal `-` are supported.
- N is positive ASCII decimal; TEXT may be empty but cannot contain CR/LF.

LF defines lines; preserve the selected LF/CRLF terminator or its absence.
All other bytes, including binary data, remain unchanged. Empty files and
out-of-range N fail unchanged. JSON paths must be UTF-8; file contents need not be.

Reject final symlinks, multiple hard links, nonregular files, foreign-owned
files and special-mode files. Parent symlinks are followed. Require readable
and writable source access and a writable parent. Preserve ordinary mode and
uid/gid; replacement has a new inode. ACLs, xattrs, timestamps and sparse layout
are not preserved; already-open descriptors retain the old inode.

A same-directory exclusive stage is completely written, synced and closed
before rename. Pre-rename failures leave original content intact and remove the
stage unless cleanup itself fails. Detected source/path changes abort; the
check-to-rename race remains. Use trusted, stable paths: no locking, hostile
namespace protection or power-loss durability guarantee.

Exit status: 0 success, 1 operational/output failure, 2 argument error.
Success is silent without JSON. JSON output fails **after** a committed edit
without rollback. SIGPIPE policy is inherited. Internal argtable OOM can terminate
the host; module-owned allocation failures are recoverable.

[Contract and executed evidence](../reports/edit-replace-line-implementation.md).
