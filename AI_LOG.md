# AI decision log

Curated decisions through October 9, 2026; not a prompt archive or complete
conversation. Git history and linked reports record implementation evidence;
this log does not replace their failures, limits or historical delivery gates.

## Pivotal decisions

- **Start with selected command contracts.** Keep useful course-required behavior,
  argument/error handling and explicitly selected JSON output; defer unsupported
  GNU features rather than claim full compatibility. JSON is command-specific,
  not universal. See [workflow](docs/ADDING_COMMANDS.md) and
  [current command guides](README.md#supported-commands).
- **Use the real `main` shell.** Register shared command specifications for
  discovery, help and dispatch, with invocation-local Argtable parsing/help and
  cleanup. Verify repeated calls and borrowed argv/descriptors/exported getopt
  state; standalone success alone is insufficient. `cd` remains in-process only.
  The shell deliberately has no external-command fallback, expansion or pipes.
  See [shell contract](docs/commands/shell.md).
- **Delegate sequentially under explicit autonomous approval.** The parent
  coordinates command implementation/tests and independent review, then verifies
  the exact proposed index before each authorized commit. A worker report is not
  proof of delivery. Routine delegated decisions do not authorize scope expansion,
  publication, wiki changes or bypassing a denied execution gate.
- **Use reference-first TDD, not reference-generated expectations.** Retained GNU
  subsets run against real installed references and independently specified
  bytes/status/side effects. Custom editors use independent byte/span oracles.
  Preserve observed RED/GREEN results, harness corrections and negative controls
  separately; a missing executable or unrelated crash is not intended RED.
- **Require mmap for `cp`.** Output parity alone does not prove the requested
  mechanism; module and fault tests also check mapped transfer and cleanup.
  See [copy evidence](docs/reports/cp-implementation.md).
- **Stage file edits safely.** Line/global editors use exclusive same-directory
  staging, complete writes, sync/close and source checks before rename, rather
  than truncating the original. Define LF/CRLF/binary behavior explicitly.
  Pre-rename preservation does not imply post-rename rollback; namespace races,
  directory durability and metadata exclusions remain limits.
  See [editor review](docs/reports/edit-replace-review.md).
- **Preserve dirty shared work.** Build narrow HEAD-based candidates, review hunks
  and the exact allowlist, then verify an extracted proposed index. Do not import
  skipped `rmdir` integration through shared Makefile/registry changes. Cached-only
  commits can leave shared working files behind committed HEAD; subsequent work
  must start from fresh HEAD, not silently repair those files.
- **Keep failures and limits visible.** Independent sleep review found a genuine
  malformed short-option hang and added a deadline regression plus a pre-parser
  guard. Default O0 sanitizer gates passed in its reviewed candidate; an inherited
  optimized shell fault-test failure and six global formatting failures remain
  distinct from scoped passes. Argtable-internal OOM is not a recovery promise.
  See [sleep review](docs/reports/sleep-review.md). Editor regex sanitizer stress
  used a smaller workload because interceptor behavior changed cost; no largest
  instrumented-stress pass is claimed in the [editor review](docs/reports/edit-replace-review.md).
- **Stop implementation and publish only approved documentation.** At this
  checkpoint `true` was blocked before code, `false`/`rm` were not started and
  `rmdir` was skipped with uncommitted artifacts retained. This approved log is
  decisions-only; existing history, raw evidence, course data and wiki stay intact.

## Verified chronology

`cat` came first; `head`, `tail` and `pwd` preceded the later delivery series.
Git records `stat`, `ls`, `mkdir`, shell/`touch`, `clear`/`cd`, `cp`, `rg` and
`edit-replace-line` on October 8, followed on October 9 by `edit-insert-line`,
`edit-delete-line`, `edit-replace`, `mv`, `env`, `echo`, `date`, `printf`, `sleep`.
The committed checkpoint is `e1c3ac9` (sleep); current paused/skipped status is in
[STATUS](docs/STATUS.md). Historical reports describe their own earlier gates,
not the latest commit status.

## Documentation and demo delivery checkpoint

On October 9, the user approved a concise root decision log, current-documentation
reconciliation and a real five-command MPG demonstration, not a full discussion
export. The demo runs committed CLI builtins (`pwd`, `ls`, `rg`, `printf`,
`date -u`) through real PTY output, with five-second holds and five successful
statuses. Its 25.08-second MPEG-2/program-stream video passes full decoding;
[demo evidence](demo/README.md) preserves the actual clean/test/clean/build/lint
results. Current API/anatomy pages resolve previously missing project references
without publishing course transcriptions. Only the approved documentation/demo
paths enter this delivery; shared implementation, skipped work, existing raw
history, course data and wiki remain untouched. Private backups precede approved
working-file reconciliation. Exact-index verification, commit and remote
publication remain distinct gates; no push is claimed by this log.
