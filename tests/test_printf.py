#!/usr/bin/env python3
"""Byte-oracle printf subset; reference and project gates stay separate."""
import argparse
import os
from pathlib import Path
import subprocess
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('program')
parser.add_argument('--reference', action='store_true')
parser.add_argument('--slice', type=int, default=1)
parser.add_argument('--main')
args = parser.parse_args()
PROGRAM = str(Path(args.program).resolve())
ENV = {'PATH': '/usr/bin:/bin', 'LC_ALL': 'C', 'HOME': '.', 'TERM': 'ansi'}

class PrintfTests(unittest.TestCase):
    def check_bytes(self, argv, expected):
        result = subprocess.run([PROGRAM, *argv], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, env=ENV, timeout=5)
        self.assertEqual((result.returncode, result.stdout, result.stderr),
                         (0, expected, b''), argv)
        if args.main:
            result = subprocess.run([str(Path(args.main).resolve()), 'printf',
                                     *argv], stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, env=ENV, timeout=5)
            self.assertEqual((result.returncode, result.stdout, result.stderr),
                             (0, expected, b''), argv)

    def test_literal_strings_and_reuse(self):
        for argv, expected in [
            (['hello'], b'hello'), ([''], b''),
            (['<%s>', 'one', 'two'], b'<one><two>'),
            (['%s:%s;', 'a', 'b', 'c'], b'a:b;c:;'),
            (['%s'], b''), (['%%'], b'%'),
            (['%c', ''], b'\0'), (['%c'], b'\0'),
            (['%c', 'word'], b'w'),
            (['%s', '--help', '-h', '--', '-n'], b'--help-h---n'),
            (['--', '-%s', '--help'], b'---help'),
            ([b'%s', b'\xff\xfe\x01\r\n'], b'\xff\xfe\x01\r\n'),
        ]:
            with self.subTest(argv=argv):
                self.check_bytes(argv, expected)

    @unittest.skipUnless(args.slice >= 2, 'later escape slice')
    def test_byte_escapes(self):
        self.check_bytes([r'\"\\\a\b\e\f\n\r\t\v'],
                         b'"\\\a\b\x1b\f\n\r\t\v')
        self.check_bytes([r'\0\101\377\400\777\x00\x41\xffZ'],
                         b'\0A\xff\0\xff\0A\xffZ')
        self.check_bytes([r'%s\n', r'\n%n'], b'\\n%n\n')
        for value in range(256):
            self.check_bytes([f'\\{value:03o}'], bytes([value]))

    @unittest.skipUnless(args.slice >= 3, 'later typed-format slice')
    def test_typed_integers_width_precision(self):
        fixtures = [
            (['%d %i %u %o %x %X', '-42', '0x2a', '42', '42', '42', '42'],
             b'-42 42 42 52 2a 2A'),
            (['[%05d][%-5d][%-05d]', '-42', '42', '-42'],
             b'[-0042][42   ][-42  ]'),
            (['[%5s][%-5.2s][%.s][%3c]', 'ab', 'abcd', 'ignored', 'word'],
             b'[   ab][ab   ][][  w]'),
            (['%d:%d;', '1', '2', '3'], b'1:2;3:0;'),
            (['%d:%u:%s:%c'], b'0:0::\0'),
            (['%d/%i/%u', '+42', '052', '0Xff'], b'42/42/255'),
            (['%d/%d/%u', '-9223372036854775808', '9223372036854775807',
              '18446744073709551615'],
             b'-9223372036854775808/9223372036854775807/18446744073709551615'),
            (['%d', ''], b'0'),
            (['%4096s', 'X'], b' ' * 4095 + b'X'),
            (['%.4096s', 'a' * 4097], b'a' * 4096),
        ]
        for argv, expected in fixtures:
            with self.subTest(argv=argv[:1]):
                self.check_bytes(argv, expected)
        import random
        rng = random.Random(420)
        for _ in range(100):
            value = rng.randrange(-(1 << 63), 1 << 63)
            width = rng.randrange(0, 50)
            text = str(value).encode()
            padding = b' ' * max(0, width - len(text))
            self.check_bytes([f'%{width}d', str(value)], padding + text)
            unsigned = rng.randrange(1 << 64)
            for spec, expected in [('u', str(unsigned)),
                                   ('o', format(unsigned, 'o')),
                                   ('x', format(unsigned, 'x')),
                                   ('X', format(unsigned, 'X'))]:
                self.check_bytes(['%' + spec, str(unsigned)], expected.encode())

    @unittest.skipUnless(args.slice >= 3, 'byte-domain characterization')
    def test_every_operand_and_literal_byte(self):
        for value in range(1, 256):
            byte = bytes([value])
            self.check_bytes([b'%s', byte], byte)
            self.check_bytes([b'%c', byte + b'trailing'], byte)
            fmt = b'%%' if byte == b'%' else b'\\\\' if byte == b'\\' else byte
            self.check_bytes([b'--', fmt], byte)

    @unittest.skipUnless(args.slice >= 4 and not args.reference,
                         'project strict validation slice')
    def test_strict_validation_before_output(self):
        cases = [[], ['--version'], ['--bogus'], ['--help', '--bogus'],
                 ['-h', '-h']]
        for fmt in ['%', '%n', '%b', '%q', '%f', '%ld', '%1$s', '%*s',
                    '%#x', '%+d', '% d', '%00d', '%--s', '%0s', '%0c',
                    '%.2d', '%.2c', '%4097s', '%.4097s', '%999999999999s',
                    '\\', r'\x', r'\xZ', r'\z', r'\c', r'\u0041']:
            cases.append(['prefix' + fmt, '1'])
        for spec, values in [('d', ['+', '-', ' 1', '1 ', '1x', '09',
                                   '0x', '0xZ', "'a", '1.0', '\xff',
                                   '9223372036854775808',
                                   '-9223372036854775809']),
                             ('u', ['-1', '-0', '18446744073709551616'])]:
            cases += [['prefix%' + spec, value] for value in values]
        cases += [['%s:%d', 'already valid', 'bad'],
                  ['%d;', '1', '2', 'bad'],
                  ['%4096s'] + [''] * 4097]
        programs = [[PROGRAM]]
        if args.main:
            programs.append([str(Path(args.main).resolve()), 'printf'])
        for prefix in programs:
            for argv in cases:
                with self.subTest(argv=argv[:3]):
                    proc = subprocess.run(prefix + argv, stdout=subprocess.PIPE,
                                          stderr=subprocess.PIPE, env=ENV,
                                          timeout=5)
                    self.assertEqual((proc.returncode, proc.stdout), (2, b''))
                    self.assertTrue(proc.stderr.startswith(b'printf:'))

    @unittest.skipUnless(args.slice >= 4 and not args.reference,
                         'project help and operational contract')
    def test_project_help_bounds_locale_output(self):
        for argv, out in [(['', 'ignored'], b''),
                          (['literal', 'ignored'], b'literal')]:
            self.check_bytes(argv, out)
        help_result = subprocess.run([PROGRAM, '--help'], env=ENV,
                                     capture_output=True, timeout=5)
        self.assertEqual((help_result.returncode, help_result.stderr), (0, b''))
        self.assertIn(b'FORMAT [ARGUMENT]...', help_result.stdout)
        self.assertIn(b'Print formatted bytes', help_result.stdout)
        for argv in [['-h'], ['--help', 'format', 'data', 'more']]:
            proc = subprocess.run([PROGRAM, *argv], env=ENV,
                                  capture_output=True, timeout=5)
            self.assertEqual((proc.returncode, proc.stdout, proc.stderr),
                             (0, help_result.stdout, b''))
        if args.main:
            for argv in [['help', 'printf'], ['printf', '--help']]:
                proc = subprocess.run([str(Path(args.main).resolve()), *argv],
                                      env=ENV, capture_output=True, timeout=5)
                self.assertEqual((proc.returncode, proc.stdout, proc.stderr),
                                 (0, help_result.stdout, b''))
        for locale in ['C', 'C.UTF-8']:
            for posix in [False, True]:
                env = dict(ENV, LC_ALL=locale)
                if posix:
                    env['POSIXLY_CORRECT'] = '1'
                proc = subprocess.run([PROGRAM, '%s:%d', '--help', '-42'],
                                      env=env, capture_output=True, timeout=5)
                self.assertEqual((proc.returncode, proc.stdout, proc.stderr),
                                 (0, b'--help:-42', b''))
        self.check_bytes(['%4096s'] + [''] * 4096, b' ' * (16 * 1024 * 1024))
        for argv in [['x'], ['--help']]:
            with open('/dev/full', 'wb') as stream:
                proc = subprocess.run([PROGRAM, *argv], stdout=stream,
                                      stderr=subprocess.PIPE, env=ENV, timeout=5)
            self.assertEqual(proc.returncode, 1)
            self.assertTrue(proc.stderr)

    @unittest.skipUnless(args.slice >= 4 and args.main and not args.reference,
                         'real main loop and child-owned PTY')
    def test_main_repeated_loop_and_pty(self):
        import errno
        import fcntl
        import pty
        import select
        import termios
        import time
        main = str(Path(args.main).resolve())
        help_bytes = subprocess.run([PROGRAM, '--help'], env=ENV,
                                    capture_output=True, timeout=5).stdout
        script = (b"printf '%s:%04d\\n' word 7\n"
                  b"printf 'prefix%n' value\nhelp printf\nprintf --help\n"
                  b"printf '%s' --help -h\nprintf '%s\\n' done\nexit\n")
        expected = b'word:0007\n' + help_bytes * 2 + b'--help-hdone\n'
        diagnostic = b'printf: invalid format or argument\n'
        result = subprocess.run([main], input=script, env=ENV,
                                capture_output=True, timeout=5)
        self.assertEqual((result.returncode, result.stdout, result.stderr),
                         (0, expected, diagnostic))
        result = subprocess.run([main], input=b"printf '%n'\nexit\n", env=ENV,
                                capture_output=True, timeout=5)
        self.assertEqual((result.returncode, result.stdout, result.stderr),
                         (2, b'', diagnostic))
        master, slave = pty.openpty()
        settings = termios.tcgetattr(slave)
        settings[3] &= ~termios.ECHO
        termios.tcsetattr(slave, termios.TCSANOW, settings)
        def own_terminal():
            os.setsid()
            fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
        proc = subprocess.Popen([main], stdin=slave, stdout=slave,
                                stderr=slave, env=ENV, preexec_fn=own_terminal)
        os.close(slave)
        try:
            os.write(master, b"printf '%s:%04d\\n' word 7\n"
                     b"printf '%n'\nprintf '%s\\n' done\nexit\n")
            captured = b''
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
                    captured += chunk
                elif proc.poll() is not None:
                    break
            self.assertEqual(proc.wait(timeout=2), 0)
            self.assertEqual(captured,
                             b'main> word:0007\r\nmain> ' +
                             diagnostic.replace(b'\n', b'\r\n') +
                             b'main> done\r\nmain> ')
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=2)
            os.close(master)

if __name__ == '__main__':
    result = unittest.TextTestRunner(verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(PrintfTests))
    raise SystemExit(not result.wasSuccessful())
