#!/usr/bin/env python3
"""Capture the real runbook PTY, then render its timed bytes to H.264 MP4.
Requires Pillow, pyte, ffmpeg. No generated command results.
"""
import argparse
import codecs
import errno
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import pty
import select
import shutil
import struct
import subprocess
import termios
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
    args = parser.parse_args()
    if not args.ffmpeg:
        parser.error('provide --ffmpeg /absolute/path/to/ffmpeg')
    import pyte
    from PIL import Image, ImageDraw, ImageFont
    here = Path(__file__).resolve().parent
    shell = here.parent
    for name in ('recording.cast', 'manual-shell.mp4'):
        if (here / name).exists():
            parser.error(f'{name} exists; preserve/move it before rerecording')
    columns, rows, fps = 144, 36, 2
    start = time.monotonic()
    pid, master = pty.fork()
    if pid == 0:
        fcntl.ioctl(1, termios.TIOCSWINSZ, struct.pack('HHHH', rows, columns, 0, 0))
        os.environ['TERM'] = 'xterm-256color'
        os.environ['COLUMNS'] = str(columns)
        os.environ['LINES'] = str(rows)
        os.chdir(os.environ.get('TMPDIR', str(here)))
        os.execv('/bin/bash', ['bash', str(shell / 'demo.sh')])
    events = []
    decoder = codecs.getincrementaldecoder('utf-8')('replace')
    with (here / 'recording.cast').open('w') as cast:
        cast.write(json.dumps({'version': 2, 'width': columns, 'height': rows,
                               'timestamp': int(time.time()), 'env': {'TERM': 'xterm-256color'},
                               'title': 'manual-shell: real demo.sh PTY'}) + '\n')
        while True:
            if not select.select([master], [], [], 1)[0]:
                continue
            try:
                data = os.read(master, 65536)
            except OSError as error:
                if error.errno == errno.EIO:
                    break
                raise
            if not data:
                break
            event = [round(time.monotonic() - start, 6), 'o', decoder.decode(data)]
            events.append(event)
            cast.write(json.dumps(event) + '\n')
            cast.flush()
    os.close(master)
    _, status = os.waitpid(pid, 0)
    code = os.waitstatus_to_exitcode(status)
    duration = time.monotonic() - start
    transcript = ''.join(event[2] for event in events)
    (here / 'recording.log').write_text(transcript)
    assert code == 0, f'runbook exit {code}; see recording.log'
    assert transcript.count('Ran 16 tests in') == 2
    assert transcript.count('\r\nOK\r\n') == 2
    for token in ('% pwd --help', '% cat --help', '% head --help', '% cat missing.txt',
                  'wait4(', 'execve(', 'Demo complete.'):
        assert token in transcript, token
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 18)
    screen = pyte.Screen(columns, rows)
    stream = pyte.Stream(screen)
    width, height = columns * 11 + 40, rows * 23 + 72
    frames = math.ceil(duration * fps)
    command = [args.ffmpeg, '-v', 'error', '-f', 'rawvideo', '-pixel_format', 'rgb24',
               '-video_size', f'{width}x{height}', '-framerate', str(fps), '-i', '-',
               '-an', '-c:v', 'libx264', '-preset', 'fast', '-crf', '20',
               '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(here / 'manual-shell.mp4')]
    targets = {'build': '$ make TOKENIZER=manual\r', 'help': '% head --help',
               'happy': '% head -n 2 lines.txt', 'error': '% cat missing.txt',
               'trace': 'Parent clone/fork', 'complete': 'Demo complete.'}
    shots = {}
    for name, marker in targets.items():
        hits = [event[0] for event in events if marker in event[2]]
        if hits:
            shots[name] = min(frames - 1, math.ceil((hits[0] + 4) * fps))
    frame_paths = []
    index = 0
    with subprocess.Popen(command, stdin=subprocess.PIPE) as encoder:
        assert encoder.stdin is not None
        for frame in range(frames):
            timestamp = frame / fps
            while index < len(events) and events[index][0] <= timestamp:
                stream.feed(events[index][2])
                index += 1
            image = Image.new('RGB', (width, height), '#10151d')
            draw = ImageDraw.Draw(image)
            draw.text((20, 10), f'manual-shell | real demo.sh PTY | {timestamp:05.1f}s',
                      font=font, fill='#8bbcff')
            for line, text in enumerate(screen.display):
                draw.text((20, 46 + line * 23), text, font=font, fill='#e4e9ef')
            for name, selected in shots.items():
                if frame == selected:
                    path = here / f'frame-{name}.png'
                    image.save(path)
                    frame_paths.append(path.name)
            encoder.stdin.write(image.tobytes())
        encoder.stdin.close()
        assert encoder.wait() == 0
    metadata = {'runbook': '../demo.sh', 'exit_status': code, 'capture_seconds': duration,
                'fps': fps, 'frame_count': frames, 'video_seconds': frames / fps,
                'width': width, 'height': height, 'pty_output_events': len(events),
                'terminal_columns': columns, 'terminal_rows': rows,
                'test_suites': {'manual': 16, 'strtok': 16}, 'frames': frame_paths,
                'sha256': {name: hashlib.sha256((here / name).read_bytes()).hexdigest()
                           for name in ('manual-shell.mp4', 'recording.cast', 'recording.log')}}
    (here / 'recording.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(json.dumps(metadata, indent=2))


if __name__ == '__main__':
    main()
