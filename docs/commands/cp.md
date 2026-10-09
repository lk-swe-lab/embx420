# cp — planned, not implemented

There is no project `build/cp` implementation or supported cp CLI yet.

Planned usage (not executable documentation):

```text
cp [-r|--recursive] [-f|--force] [--json] [--] SOURCE DEST
```

Proposed scope: one source/destination, directory destinations, recursive copying,
force retry, structured results and `-h`/`--help`. Exact JSON and path/race policy
remain to be settled before implementation.

File contents must be copied with `mmap`, including explicit handling of empty
files, destination sizing and mapping/descriptor cleanup. Same-inode copies and
unsupported special files must be rejected. No promise of atomic replacement,
metadata preservation, sparse allocation or hard-link topology; failures may
leave partial destinations. These are design constraints, not verified behavior.

[Historical inventory and design snapshot](../reports/cp.md).
