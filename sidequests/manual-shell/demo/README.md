# Real terminal demo and verification

`manual-shell.mp4` is an MP4/H.264 recording of real execution, not invented
terminal output. `record.py` executed `../demo.sh` under a Linux PTY **from a
different working directory**, captured every output event with monotonic
timestamps into asciicast v2 `recording.cast`, and rendered those bytes through
pyte/Pillow. The title bar is a rendering label, not program output. There is no
audio, no time compression, and no synthesized command results. The terminal is
144 columns by 36 rows; rendering samples it at 2 frames/second with 18px
DejaVu Sans Mono and five-second runbook holds.

## What the video shows

- Existing repository `pwd`, `cat`, and `head` build targets (already up to date
  on this machine); a clean shell build is visibly compiled later.
- `make TOKENIZER=manual test`, then `make TOKENIZER=strtok test` without an
  intervening clean: **16 passed, zero skips in each mode**. The display shows
  real final test summaries; full output is preserved in `tests-manual.log`
  and `tests-strtok.log`.
- A second clean and visible build restoring the default manual tokenizer.
- One foreground shell session, **11 scripted input lines**: help for each
  external command; pwd; parent cd; child pwd inheriting the directory; cat;
  head; a missing-file error; successful pwd recovery; exit.
- A separate real `strace -f -e trace=process` invocation: parent PID 75994
  clones child PID 75995, the child execs the repository `build/pwd`, and the
  parent completes `wait4` for that exact child. This short trace adds `pwd`
  and `exit` inputs outside the 11-line main demonstration.

The layout-adjustment trace test was first observed failing with the copied
old path (`A/A/build`); after adapting the makefile and both test expectations
to the containing repository's `build`, both complete suites passed. All seven
C/header files remain byte-identical to `03/ShellAssignment`. No registry or
new shell behavior was added. The supplied skeleton attribution remains in the
main README. The assignment docx, AI log, binaries, objects, caches, and unrelated
course materials were not copied. Build artifacts were cleaned after recording.

## Verified artifact

- Container: MP4 (`mov,mp4,m4a,3gp,3g2,mj2`).
- Codec/profile/pixels: H.264 / High / `yuv420p`.
- Dimensions: **1624 x 900**; frame rate **2/1**; decoded frames **194**.
- Video duration: **97.000000 seconds**; live PTY capture: **96.577165101 seconds**
  (last frame padded to the frame boundary).
- Size: **1,310,594 bytes**.
- SHA-256: `e888eed7861ed3cff82136a170b1250b73ac6edb7d70b3a018ab3ccfc1c6f969`.
- Faststart verified: `moov` offset 36 precedes `mdat` offset 3171.
- Full decode: exit **0**, **zero stderr bytes** (`decode.log` is intentionally
  empty). `ffprobe.json` contains the actual probe output.
- Runbook exit **0**. `bash -n ../demo.sh` and scoped source/docs whitespace
  checks passed. No source modifications outside this new subtree were made
  by this worker. Baseline comparison observed only the separately parent-owned
  `03/ShellAssignment/AI_LOG.md` changing concurrently; original C sources,
  `A/Makefile`, and `A/src/*` remained unchanged during this work.

`recording.json` records capture/render timing and artifact hashes.
`verification.json` records suite counts, exact main input lines, trace child,
and the exact verification command arguments. `recording.log` is the real PTY
transcript. `frame-{build,help,happy,error,trace,complete}.png` are rendered hold
frames. `decoded-{help,error,trace}.png` were extracted **from the final MP4**;
help, error, recovery, and PID trace readability were visually inspected.

Recheck from this directory (set the tool path if ffmpeg is not on PATH):

```sh
FFBIN=/home/name/.hermes/tools/ffmpeg-9.0.1-linux-x64/bin
"$FFBIN/ffprobe" -v error -count_frames \
  -show_entries format=format_name,duration,size:stream=codec_name,profile,pix_fmt,width,height,r_frame_rate,nb_read_frames \
  -of json manual-shell.mp4
"$FFBIN/ffmpeg" -v error -i manual-shell.mp4 -f null -
sha256sum manual-shell.mp4
```

Run the live demonstration from any working directory using the absolute path
to `sidequests/manual-shell/demo.sh`; it requires Linux, a C compiler, make,
Python 3, strace, and the containing repository's command sources/dependencies.
A single build command from `sidequests/manual-shell` is:

```sh
make -C ../.. build/pwd build/cat build/head && make
```

To make a **new** recording, preserve/move the existing `recording.cast` and
`manual-shell.mp4` first; the recorder refuses to overwrite them. Run
`python3 record.py --ffmpeg /absolute/path/to/ffmpeg` with Pillow and pyte
installed. This execution used an isolated scratch venv with Pillow 13.0.0,
pyte 0.8.2, and wcwidth 0.9.2, not a change to the coursework environment.

## Limits and publication status

This is the intentionally minimal learning side project, not the full main
assignment: no registry, pipes, redirection, background jobs, quoting, expansion,
job control, or custom signal management. `pwd`, `cat`, and `head` are existing
repository programs; their sources and help text were not rewritten. Builtins
retain their original minimal contract, not a newly added help interface.
Children share stdin: use file operands in scripted demonstrations. The trace
is mandatory in this runbook (not silently skipped). Linux/GCC/strace behavior
was exercised here; other platforms were not verified. AI-log integration and
Week02/main-coursework registry requirements belong to the parent publication
work, not this side-project snapshot. No staging, commit, or push was performed.
