# mv

```sh
./build/main mv SOURCE DEST
./build/main mv -f --json SOURCE DEST
./build/main help mv
```

Rename exactly one file, directory, or symbolic link on the same filesystem.
Directory DEST (including a directory symlink) receives SOURCE basename.
Existing effective targets require `-f`/`--force`; otherwise return an error,
unlike GNU's noninteractive default. Same-inode/self targets are errors.
No prompts or stdin reads. Optional standalone: `./build/mv`.

`-h`/`--help` shows usage. `--` ends options. `--json` emits one object with
original `source`, original `destination`, and `status` (`ok` or `error`).
Empty paths and non-UTF-8 JSON paths are argument errors before mutation;
text paths retain raw bytes. Invalid syntax returns 2, operation/output errors 1,
success 0. Operational errors also print stderr diagnostics.

Final source links move as links; effective target links are replaced, not
followed. Trailing SOURCE slash requires a real directory, not a symlink;
trailing DEST slash requires an existing directory. No missing-parent creation.
Force never deletes first: incompatible/nonempty directory targets fail.

Cross-device `EXDEV` fails without copy/delete fallback. Single local rename
preserves the object's inode, content, mode, owner, and mtime; ctime and parent
timestamps can change. No fsync/durability or hostile/concurrent-namespace promise;
default no-overwrite preflight is not atomic under concurrent changes. JSON
output failure can occur **after rename** and does not roll back. Ambiguous
EIO/NFS errors cannot universally promise unchanged state. Internal argtable OOM
can terminate the host. Signals inherited; no thread-safe/interrupted getopt scan
promise. Multi-source moves, prompts, backups, update/verbose, and relabeling deferred.

[Contract and executed evidence](../reports/mv-implementation.md).
