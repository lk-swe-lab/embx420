#!/usr/bin/env python3
"""Additional real-main terminal, redirect and physical-cwd gates."""
import argparse
import errno
import json
import os
from pathlib import Path
import pty
import select
import subprocess
import tempfile
import termios
import time

parser = argparse.ArgumentParser()
parser.add_argument('program')
options = parser.parse_args()
program = str(Path(options.program).resolve())
observations = []
env = {**os.environ, 'LC_ALL': 'C', 'TERM': 'xterm'}
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as temp:
    root = Path(temp).resolve()
    nested = root / 'real' / 'nested'
    nested.mkdir(parents=True)
    (root / 'alias').symlink_to('real/nested')
    for term in (None, '', 'dumb', 'xterm', 'unknown-terminal'):
        local_env = env.copy()
        if term is None:
            local_env.pop('TERM', None)
        else:
            local_env['TERM'] = term
        target = root / 'redirected'
        with target.open('wb') as output:
            result = subprocess.run([program, 'clear'], stdin=subprocess.DEVNULL,
                                    stdout=output, stderr=subprocess.PIPE,
                                    cwd=root, env=local_env, timeout=5)
        assert (result.returncode, target.read_bytes(), result.stderr) == (0, b'', b'')
        observations.append({'gate': 'regular-file clear', 'TERM': term, 'status': 0})

    master, slave = pty.openpty()
    settings = termios.tcgetattr(slave)
    settings[3] &= ~termios.ECHO
    termios.tcsetattr(slave, termios.TCSANOW, settings)
    child = subprocess.Popen([program], stdin=slave, stdout=slave,
                             stderr=subprocess.PIPE, cwd=root, env=env)
    os.close(slave)
    output = bytearray()
    try:
        os.write(master, b'pwd\nclear\ncd alias\npwd\ncd ..\npwd\nclear\nexit\n')
        deadline = time.monotonic() + 5
        while True:
            assert time.monotonic() < deadline, 'PTY deadline'
            ready, _, _ = select.select([master], [], [], 0.1)
            if not ready:
                continue
            try:
                block = os.read(master, 4096)
            except OSError as error:
                if error.errno == errno.EIO:
                    break
                raise
            if not block:
                break
            output.extend(block)
        _, errors = child.communicate(timeout=5)
        expected = (os.fsencode(root) + b'\r\n\x1b[H\x1b[2J' +
                    os.fsencode(nested) + b'\r\n' +
                    os.fsencode(nested.parent) + b'\r\n\x1b[H\x1b[2J')
        assert (child.returncode, bytes(output), errors) == (0, expected, b'main> ' * 8)
        observations.append({'gate': 'fully interactive PTY combined loop', 'status': 0,
                             'stdout_hex': bytes(output).hex(), 'stderr_hex': errors.hex()})
    finally:
        if child.poll() is None:
            child.kill()
            child.communicate()
        os.close(master)

    # Build a >PATH_MAX physical cwd without an oversized path syscall.
    descriptor = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
    components = ['d' * 150 + str(index) for index in range(30)]
    try:
        for name in components:
            os.mkdir(name, dir_fd=descriptor)
            next_fd = os.open(name, os.O_RDONLY | os.O_DIRECTORY, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = next_fd
    finally:
        os.close(descriptor)
    expected_path = str(root) + '/' + '/'.join(components)
    script = ''.join('cd ' + name + '\n' for name in components) + 'pwd\ncd ' + str(root) + '\npwd\n'
    result = subprocess.run([program], input=script.encode(), stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, cwd=root, env=env, timeout=5)
    assert len(os.fsencode(expected_path)) > 4096
    assert (result.returncode, result.stdout, result.stderr) == (
        0, os.fsencode(expected_path) + b'\n' + os.fsencode(root) + b'\n', b'')
    observations.append({'gate': 'physical cwd beyond PATH_MAX and absolute recovery',
                         'path_bytes': len(os.fsencode(expected_path)), 'status': 0})
print(json.dumps({'euid': os.geteuid(), 'observations': observations}, indent=2))
