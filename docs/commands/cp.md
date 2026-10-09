# cp

```sh
./build/main cp [-r|--recursive] [-f|--force] [--json] [--] SOURCE DEST
./build/main help cp           # Also cp -h / --help
./build/cp SOURCE DEST         # Optional standalone wrapper
```

Exactly one source and destination. Existing directory DEST receives the source
basename. Regular files are copied using bounded `mmap` windows; empty files work.
Existing files are resized and overwritten, preserving their inode/hardlinks
and ordinary permissions. New files/directories use source rwx permissions and
inherited umask. No metadata/sparse/hardlink-topology preservation.

- `-r`: merge directory trees; copy symlinks as symlinks, never descend into them.
- `-f`: after a failed open, remove/recreate an existing regular, non-symlink file.
- `--json`: one UTF-8 object: `{"source":"...","destination":"...","status":"ok"}`
  (`"error"` on operational failure); destination is the original operand.
- `--`: end options; `-` is a literal filename. No multi-source or other GNU flags.

Without `-r`, source links follow to regular files. Destination links must have
an existing regular referent; dangling links fail even with `-f`. Recursive
**directory** destinations cannot be symlinks. Special files and same-inode
copies fail. Mapped destinations need read **and** write access (unlike GNU).

Status: 0 success, 1 operational/output failure, 2 argument error. Ordinary
success is silent. Invalid JSON-path encoding is rejected before changes.
Failures may leave resized/partial files or partially populated trees; no rollback.
Recursive children continue after errors; depth is bounded to 128 directory levels.
Use stable, trusted trees: namespace races and concurrent source changes are not
supported. Concurrent truncation/storage faults may raise **SIGBUS and terminate
the host shell**; no signal handler is installed. Reading may update source atime.

[Implementation, tests and limits](../reports/cp-implementation.md).
[Preserved historical inventory](../reports/cp.md).
