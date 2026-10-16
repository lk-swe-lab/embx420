# Command status

Checkpoint: October 9, 2026, committed HEAD `e1c3ac9` (sleep).
Implementation is paused; this documentation update adds no commands.

## Committed and integrated

The committed registry contains 22 commands, with 21 optional standalone wrappers;
`cd` is in-process only. [README](../README.md#supported-commands) lists every
command and links its supported syntax, output format and limitations.
Committed integration can differ from preserved shared working files after
cached-only delivery; a dirty working tree is not the current committed checkout.

## Not delivered

- `true`: blocked before code; no implementation claimed.
- `false`, `rm`: not started.
- `rmdir`: skipped; uncommitted artifacts remain excluded from this delivery.

Resumption needs explicit authorization and fresh discovery of the first unfinished
gate. Do not retry a denied operation through another worker or execute skipped
command tests as part of this documentation task.

## Evidence and limits

[Reports](reports/README.md) are historical snapshots; their pending-gate text is
not current commit status. Implementation, independent review, exact-index
verification and commit are separate gates in the [workflow](ADDING_COMMANDS.md).
The [decision log](../ai_log/2026-10-09-AI_LOG.md) explains the scope and delivery choices.

These are selected subsets, not full GNU replacements. JSON is supported only
where the command guide says so: `echo`, `date`, `printf` and `sleep` have no JSON
mode. See [date](commands/date.md) for numeric local/UTC output,
[printf](commands/printf.md) for bounded byte formatting, and
[sleep](commands/sleep.md) for decimal seconds and inherited-signal behavior.
Existing limits include dependency-internal parser OOM, inherited formatting
failures and the optimized sanitizer shell fault-test issue recorded in
[sleep review](reports/sleep-review.md); these are not new fixes.

## Documentation and demo checkpoint

The approved decisions-only log and current documentation accompany the
[five-command recording](../demo/README.md). Its real PTY execution has five
successful statuses and approximately five-second holds; FFprobe confirms a
25.08-second MPEG program stream with MPEG-2 video, and full decoding succeeds.
The demo evidence records clean/test/clean/build/lint passing on committed HEAD.
Final exact-index verification is a separate delivery gate; commit and push
remain the parent's responsibility, not completed publication claims.
