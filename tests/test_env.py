#!/usr/bin/env python3
"""Independent synthetic environment expectations; never inherit host values."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import unittest

PARSER = argparse.ArgumentParser()
PARSER.add_argument('program')
PARSER.add_argument('--reference', action='store_true')
PARSER.add_argument('--slice', default='all')
PARSER.add_argument('--main')
ARGS = PARSER.parse_args()
PROGRAM = str(Path(ARGS.program).resolve())


class EnvTests(unittest.TestCase):
    def invoke(self, env, args=(), out=subprocess.PIPE):
        return subprocess.run([PROGRAM, *args], env=env, input=b'',
                              stdout=out, stderr=subprocess.PIPE, timeout=10)

    def test_raw(self):
        fixtures = [({}, b''), ({b'A': b''}, b'A=\n'),
                    ({b'Z': b'last', b'A': b'first'}, b'A=first\nZ=last\n'),
                    ({b'X': b'a=b\nnext\r\xff'}, b'X=a=b\nnext\r\xff\n'),
                    ({b'9 weird': b'value'}, b'9 weird=value\n')]
        for env, expected in fixtures:
            with self.subTest(env=env):
                p = self.invoke(env)
                self.assertEqual((p.returncode, p.stderr), (0, b''))
                if ARGS.reference and len(env) > 1:
                    # Compatible content; GNU deliberately keeps input order.
                    self.assertEqual(sorted(p.stdout.split(b'\n')),
                                     sorted(expected.split(b'\n')))
                else:
                    self.assertEqual(p.stdout, expected)

    @unittest.skipIf(ARGS.reference or ARGS.slice == 'raw', 'project slice')
    def test_json_encoding(self):
        fixtures = [({}, b'{"env":{}}\n'),
                    ({b'Z': b'last', b'A': b'q"\\\n\t\x01=\xc3\xa9'},
                     b'{"env":{"A":"q\\"\\\\\\n\\t\\u0001=\xc3\xa9","Z":"last"}}\n')]
        # Use explicit UTF-8 bytes; no production encoder as oracle.
        for env, expected in fixtures:
            p = self.invoke(env, ['--json'])
            self.assertEqual((p.returncode, p.stdout, p.stderr),
                             (0, expected, b''))
            self.assertEqual(list(json.loads(p.stdout)['env']),
                             sorted(k.decode() for k in env))
        for env in [{b'A': b'\xff'}, {b'\xff': b'v'},
                    {b'A': b'\xed\xa0\x80'}, {b'A': b'\xc0\x80'}]:
            p = self.invoke(env, ['--json'])
            self.assertEqual((p.returncode, p.stdout), (1, b''))
            self.assertTrue(p.stderr.startswith(b'env:'))

    @unittest.skipIf(ARGS.reference or ARGS.slice != 'all', 'parser slice')
    def test_help_and_errors(self):
        p = self.invoke({}, ['--help'])
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        self.assertTrue(p.stdout.startswith(b'Usage: env'))
        self.assertIn(b'--json', p.stdout)
        self.assertIn(b'No assignments', p.stdout)
        self.assertEqual(self.invoke({}, ['-h']).stdout, p.stdout)
        for args in [['-i'], ['--unset=A'], ['A=changed'], ['/bin/true'],
                     ['-'], [''], ['--json', '--json'], ['--help', 'bad'],
                     ['--', 'A'], ['--bad'], ['--json=1']]:
            bad = self.invoke({b'A': b'original'}, args)
            self.assertEqual((bad.returncode, bad.stdout), (2, b''), args)
            self.assertTrue(bad.stderr)
        self.assertEqual(self.invoke({}, ['--']).stdout, b'')
        self.assertEqual(self.invoke({}, ['--js']).stdout, b'{"env":{}}\n')
    @unittest.skipIf(ARGS.reference or ARGS.slice != 'all', 'project slice')
    def test_generated_byte_order_and_large(self):
        import random
        rng = random.Random(147)
        for repeat in range(100):
            keys = [bytes([ch]) for ch in rng.sample(
                [n for n in range(1, 256) if n != 61], 40)]
            env = {key: bytes([repeat + 1]) + b'=value' for key in keys}
            expected = b''.join(k + b'=' + env[k] + b'\n' for k in sorted(keys))
            p = self.invoke(env)
            self.assertEqual((p.returncode, p.stdout, p.stderr), (0, expected, b''))
        env = {f'K{n:04}'.encode(): b'x' * 50 for n in reversed(range(2000))}
        expected = b''.join(k + b'=' + env[k] + b'\n' for k in sorted(env))
        p = self.invoke(env)
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, expected, b''))
        p = self.invoke(env, ['--json'])
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        self.assertEqual(list(json.loads(p.stdout)['env']),
                         sorted(k.decode() for k in env))
        self.assertEqual(json.loads(p.stdout)['env'],
                         {k.decode(): v.decode() for k, v in env.items()})
        controls = bytes(range(1, 32)) + b'"\\=\xc3\xa9\xf0\x9f\x92\xa9'
        p = self.invoke({b'K\n"\\': controls}, ['--json'])
        self.assertEqual((p.returncode, p.stderr), (0, b''))
        self.assertEqual(json.loads(p.stdout),
                         {'env': {'K\n"\\': controls.decode()}})

    @unittest.skipIf(ARGS.reference or ARGS.slice != 'all', 'project slice')
    def test_output_failure(self):
        with open('/dev/full', 'wb') as full:
            for args in [[], ['--json'], ['--help']]:
                p = self.invoke({b'A': b'value'}, args, full)
                self.assertEqual(p.returncode, 1)
                self.assertTrue(p.stderr.startswith(b'env:'))

    @unittest.skipUnless(ARGS.main, 'main integration requested separately')
    def test_main_dispatch_reuse(self):
        main = str(Path(ARGS.main).resolve())
        env = {b'Z': b'last', b'A': b'value'}
        def call(args=(), data=b''):
            p = subprocess.run([main, *args], env=env, input=data,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               timeout=10)
            return p.returncode, p.stdout, p.stderr
        js = b'{"env":{"A":"value","Z":"last"}}\n'
        raw = b'A=value\nZ=last\n'
        self.assertEqual(call(['env', '--json']), (0, js, b''))
        self.assertEqual(call(['env']), (0, raw, b''))
        help_result = self.invoke(env, ['--help'])
        help_out = help_result.stdout
        self.assertEqual(call(['help', 'env']), (0, help_out, b''))
        self.assertIn(b'  env      List environment variables.\n', call(['help'])[1])
        bad = self.invoke(env, ['bad'])
        self.assertEqual(call(['env', 'bad']), (2, b'', bad.stderr))
        script = b'env --json\nenv bad\nhelp env\nenv --help\nenv\n' * 50
        self.assertEqual(call(data=script + b'exit\n'),
                         (0, (js + help_out * 2 + raw) * 50, bad.stderr * 50))
        self.assertEqual(call(data=b'env bad\nexit\n'), (2, b'', bad.stderr))

    @unittest.skipUnless(ARGS.main, 'main integration requested separately')
    def test_main_child_owned_pty(self):
        import errno
        import fcntl
        import pty
        import select
        import termios
        import time
        master, slave = pty.openpty()
        attrs = termios.tcgetattr(slave)
        attrs[3] &= ~termios.ECHO
        termios.tcsetattr(slave, termios.TCSANOW, attrs)
        def own_terminal():
            os.setsid()
            fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
        env = {b'A': b'synthetic'}
        p = subprocess.Popen([str(Path(ARGS.main).resolve())], env=env,
                             stdin=slave, stdout=slave, stderr=slave,
                             preexec_fn=own_terminal)
        os.close(slave)
        try:
            os.write(master, b'env --json\nenv bad\nenv\nexit\n')
            result = b''
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                ready, _, _ = select.select([master], [], [], 0.1)
                if ready:
                    try:
                        chunk = os.read(master, 65536)
                    except OSError as error:
                        if error.errno == errno.EIO:
                            break
                        raise
                    if not chunk:
                        break
                    result += chunk
                elif p.poll() is not None:
                    break
            self.assertEqual(p.wait(timeout=2), 0)
            error = self.invoke(env, ['bad']).stderr.replace(b'\n', b'\r\n')
            self.assertEqual(result, b'main> {"env":{"A":"synthetic"}}\r\n'
                             b'main> ' + error + b'main> A=synthetic\r\nmain> ')
        finally:
            if p.poll() is None:
                p.kill()
                p.wait(timeout=2)
            os.close(master)


if __name__ == '__main__':
    unittest.main(argv=[__file__], verbosity=2)
