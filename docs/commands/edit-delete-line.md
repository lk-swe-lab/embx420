# edit-delete-line

```sh
./build/main edit-delete-line [--json] [--] FILE N
./build/main help edit-delete-line
# Optional standalone: ./build/edit-delete-line FILE N
```

Delete one existing 1-based line, including its LF terminator if present.
LF separates lines; a nonempty unterminated suffix counts as a line. CRLF is
removed with its selected line; bare CR is content. Other bytes, including binary
content and unaffected final-newline state, are unchanged. Deleting the only line
leaves an empty file; empty files and absent lines fail without editing.

- `-h`, `--help`: parser-derived help, no edit.
- `--json`: one `{path,line,status,message}` result with `ok`/`error`, `null`
  success message. JSON paths require UTF-8; content need not be UTF-8.
- N is positive ASCII decimal through `UINTMAX_MAX`; leading zeroes accepted.
- `--` protects option-like paths; `-` is a literal filename.

Complete-file staging in the same directory precedes rename. Require source
read/write access and writable parent. Reject final symlinks, hard links,
nonregular files, foreign ownership and special mode bits; parent symlinks are
followed. Ordinary mode and uid/gid are retained, but inode changes. ACLs, xattrs,
sparse layout and timestamps are not retained. Reading can change atime.

Status: 0 success, 1 operational/output failure, 2 invalid arguments. Non-JSON
success is silent; errors go to stderr. Argument errors emit no JSON. Failure
before rename leaves source unedited; cleanup failure is diagnosed. Output failure
after rename does **not** undo the edit. Trusted stable paths only: observed
concurrent changes are rejected, but the final check/rename race remains. No
locking, snapshot, directory fsync or power-loss durability guarantee. Internal
argtable allocation failure can terminate the host. SIGPIPE policy is inherited.

[Contract and execution evidence](../reports/edit-delete-line-implementation.md).
[Independent review](../reports/edit-delete-line-review.md) passes the selected
contract; exact-index delivery is a separate gate.
