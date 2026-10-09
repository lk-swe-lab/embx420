# touch

`touch [-h|--help] [--json] [--] FILE...`

- Set access and modification times to now; create missing empty regular files.
- Existing contents/modes are preserved; creation uses 0666 with inherited umask.
- `-h`, `--help`: show help without effects. `--json`: ordered
  `{path,status,message}` array; paths must be UTF-8. `--`: end options.
- Success is silent unless JSON is requested. Errors go to stderr; later
  operands continue. Status: 0 success, 1 operational/output failure,
  2 argument error. Invalid arguments are rejected before effects.
- Unlike GNU: `-` is a literal filename, `-h` is help, dangling final symlinks
  fail instead of creating their referent. Existing symlinks are followed;
  directories/FIFOs/sockets get metadata-only updates. Link traversal may
  change link atime. No date/reference/no-create options or parent creation.
- No rollback or pathname-race guarantee; output failure retains completed
  effects. SIGPIPE is inherited. Internal argtable OOM may terminate the process.

Build/test: `make build/touch test-touch-impl`; GNU subset: `make test-touch`.
Detailed contract and evidence: [report](../reports/touch.md).
