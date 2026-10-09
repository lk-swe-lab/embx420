# edit-replace

```sh
./build/main edit-replace FILE PATTERN REPLACEMENT
./build/main edit-replace -i --fixed-strings --json FILE PATTERN REPLACEMENT
./build/main help edit-replace
```

Replace all nonoverlapping whole-file POSIX ERE matches; `--fixed-strings`
matches literal bytes, `-i` ignores ASCII case. Replacement is always literal:
no `&`, backreference or escape expansion. Regex dot can match LF; `^`/`$`
anchor file bounds. Empty matches emit replacement and advance one retained
byte, including one match at EOF. No-match succeeds without replacing the inode.
`-h`/`--help` do not edit; `--` permits option-like operands; `-` is a literal path.

Successful `--json`: `{"path":"FILE","matches":M,"replacements":M}` plus LF.
File-attempt errors add `status:"error",message` with zero counts. Argument errors
and setup allocation failures emit no JSON. Non-JSON success is silent.
Status: 0 success/no-match/help; 1 operational/output failure; 2 argument/regex error.

Limits: input 8 MiB, output 32 MiB, pattern 1024 bytes, replacement 4096 bytes.
Regex rejects NUL input; literal mode preserves arbitrary bytes. C byte semantics,
not Unicode folding; JSON paths must be UTF-8. Regex pattern backreferences are
unsupported. Matching uses Linux/BSD `REG_STARTEND`; libc regex has no hard
CPU/memory guarantee for hostile expressions.

Complete same-directory staging before rename; reject final symlinks, hard links,
nonregular/foreign-owned/special-mode files. Readable/writable source required;
matched edits require writable parent. Preserve ordinary mode/uid/gid, not inode,
timestamps/ACLs/xattrs/sparse layout. Trusted stable paths only: no lock,
check-to-rename race elimination or directory fsync. Precommit failures preserve
source; cleanup failure is diagnosed. Postcommit output failure cannot undo edit.

Optional standalone: `./build/edit-replace`. Tests: `make test-edit-replace-impl`.
[Contract and execution evidence](../reports/edit-replace-implementation.md).
