# rmdir

```sh
./build/main rmdir DIRECTORY...
./build/main rmdir --json DIRECTORY...
./build/main help rmdir
# Optional standalone: ./build/rmdir with the same arguments.
```

Remove only explicitly named empty directories. Never recursively remove
contents or implicit parents. `-h`/`--help` prints usage; `--` permits option-like
names. Paths are literal; the module performs no glob expansion.

Operands run in order and continue after operational errors. Duplicates are
separate attempts: a second removal normally fails missing. Nonempty directories,
non-directories, final symlinks (also with trailing slashes), root/dot/parent
operands and aliases of this module's cwd are rejected. Intermediate symlinks
and parent components follow kernel pathname resolution. There is no guarantee
against concurrent namespace or cwd changes; use trusted stable trees.

Plain success is silent. Operational diagnostics go to stderr. JSON writes an
ordered array plus LF, retaining original operand spelling:

```json
[{"path":"empty","status":"ok","errno":0}]
```

Errors use `status:"error"` and the saved numeric errno; numbers are platform
specific. JSON requires valid UTF-8 paths; plain mode accepts arbitrary non-NUL
filename bytes. All syntax/encoding validation and owned allocation occur before
removals. JSON stdout descriptor preflight catches closed/read-only output, not
future transfer failures. Output failure after removal cannot restore directories.

Status: 0 all removed, 1 operational/allocation/output failure, 2 argument error.
GNU `-p`/`--parents`, `--ignore-fail-on-non-empty`, `-v`/`--verbose` and `--version`
are deferred. No signal-disposition changes, durability sync, or rollback.
Broken pipes follow inherited SIGPIPE: default handling may terminate the host;
with SIGPIPE ignored, a write error returns 1 after completed removals.

Tests: `make test-rmdir-impl`; GNU selected subset: `make test-rmdir`.
[Contract and executed evidence](../reports/rmdir-implementation.md).
