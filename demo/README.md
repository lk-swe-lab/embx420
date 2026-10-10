# Five-command demo

`commands-demo.mpg` is a real PTY replay of committed HEAD
`e1c3ac9fa74a9ef4fd1abd98258df5e0208fbdbb`: 1280×720, 25 fps,
MPEG program stream with MPEG-2 video, 25.08 seconds, no audio.
Each command ran successfully and remained visible for about five seconds.

After installing the project's build dependencies:

```bash
make clean
make test
make clean
make all
make lint
./demo.sh
```

The five builtins run through `./build/main`: `pwd`, `ls` on the existing
small tree fixture, `rg` on a committed header, `printf`, and `date -u`.
The script uses separate arguments, no `eval`, and fails on command errors.
It resolves its own directory; `./demo.sh /absolute/project/root` optionally
selects a separately built project. Tests/build/lint are outside the video.

## Evidence

- `commands-demo.cast`: real PTY bytes and monotonic output timestamps.
- `commands-demo.log`: readable execution transcript.
- `recording.json`: five command starts, display intervals, and statuses.
- `verification/results.json`: clean/test/clean/build/lint all exited 0.
- `verification/*.log`: complete combined output for those commands.
- `verification/mpg-ffprobe.json`: actual container, codec, size, frame rate,
  and duration; `media-results.json` summarizes checks.
- `verification/mpg-decode.log`: complete video decoded with exit 0.

Verification used a clean Git archive of the exact HEAD, a private temporary
directory, and links only to installed argtable3/CUnit dependencies. No dirty
implementation files were built. Lint exited 0; its existing warning-generation
and suppression messages remain in the log. The video was rendered from the
recorded terminal stream, not fabricated command output. Decoded command frames
were inspected for readability. No MP4 is included in delivery.
