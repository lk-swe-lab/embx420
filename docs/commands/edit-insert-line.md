# edit-insert-line

Insert before 1-based N, or append at N=line_count+1; empty files accept N=1.

```sh
./build/main edit-insert-line FILE N 'new text'
./build/main edit-insert-line --json FILE N 'new text'
./build/main help edit-insert-line
# Optional standalone: ./build/edit-insert-line FILE N TEXT
```

- `-h`, `--help`: parser-derived help without editing.
- `--json`: one `{path,line,status,message}` object and newline, status `ok` or
  `error`, success message `null`. Argument errors emit no JSON.
- `--`: end options; option-like operands and literal filename `-` are supported.
- N is positive ASCII decimal; TEXT may be empty but cannot contain CR/LF.

LF defines existing lines; a nonempty unterminated final segment counts as a line.
Every inserted line ends in **LF**, including in CRLF files. Original bytes and
CRLF terminators remain unchanged. Appending to an unterminated file adds an LF
separator before TEXT+LF; a preserved bare final CR consequently becomes CRLF.
Out-of-range N fails unchanged. JSON paths must be UTF-8; content/TEXT need not be.

Reject final symlinks, multiple hard links, nonregular, foreign-owned and special-
mode files; parent symlinks are followed. Require readable/writable source access
and a writable parent. Preserve ordinary mode and uid/gid; replace with a new inode.
Already-open descriptors retain the old inode. ACLs, xattrs, timestamps and sparse
layout are not preserved; reads can update atime.

Exclusively stage the complete file in the same directory, sync and successfully
close before rename. Pre-rename failures leave source content unedited and remove
staging unless cleanup itself fails (diagnosed). Observed source/path changes abort;
the check-to-rename race remains. Trusted stable paths only: no locking, hostile
namespace protection, snapshot isolation or power-loss durability guarantee.

Exit status: 0 success, 1 operational/output failure, 2 argument error. Non-JSON
success is silent. JSON output failure after commit does not roll back. SIGPIPE
policy is inherited. Internal argtable OOM can terminate the host; module-owned
allocation failures are recoverable.

[Contract and executed evidence](../reports/edit-insert-line-implementation.md).
