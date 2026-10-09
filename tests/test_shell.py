#!/usr/bin/env python3
"""Exact shell integration checks; all temporary fixtures stay in scratch."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

PARSER = argparse.ArgumentParser()
PARSER.add_argument('--program', required=True)
PARSER.add_argument('--standalone-dir', required=True)
PARSER.add_argument('--slice', choices=['dispatch', 'loop', 'all'], default='all')
ARGS = PARSER.parse_args()
PROGRAM = str(Path(ARGS.program).resolve())
STANDALONE = Path(ARGS.standalone_dir).resolve()
STANDALONE_NAMES = ['cat', 'head', 'tail', 'pwd', 'stat', 'ls', 'mkdir',
                    'touch', 'clear', 'cp', 'rg']
BUILTIN_NAMES = STANDALONE_NAMES[:-1] + ['cd', 'rg']
SUMMARIES = [
    'Concatenate files to standard output.',
    'Show the first lines of a file or stream.',
    'Display the last N lines (default 10); never create input files.',
    'Print physical working directory.',
    'Inspect file metadata.',
    'List directory contents.',
    'Create directories.',
    'Create empty files or update timestamps without changing contents.',
    'Clear an ANSI-compatible terminal screen.',
    'Copy files and directories.',
    'Change working directory.',
    'Search explicit files and directories.',
]
HELP = (
    'Usage: main COMMAND [ARGS...]\n'
    '       main                 # read commands from stdin\n'
    '       main --help\n'
    '       main -r DIR [--mtime] [--local|--utc] [--histogram]\n'
    '\nCommands:\n'
    + ''.join(f'  {n:<8} {s}\n' for n, s in zip(BUILTIN_NAMES, SUMMARIES))
    + '  help [COMMAND]  List commands or show command usage.\n'
    '  exit            Leave the loop with the last command status.\n'
).encode()


class ShellTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(
            dir=os.environ.get('TMPDIR', '/home/name/.hermes/cache/scratch'))
        self.addCleanup(self.temp.cleanup)
        self.cwd = Path(self.temp.name)
        (self.cwd / 'data').write_bytes(b'first\nsecond\nthird\n')

    def run_cli(self, args=(), data=b'', binary: str | Path = PROGRAM):
        proc = subprocess.run([str(binary), *args], input=data,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              cwd=self.cwd, env={**os.environ, 'LC_ALL': 'C'},
                              timeout=5)
        return proc.returncode, proc.stdout, proc.stderr

    def check(self, args=(), data=b'', status=0, out=b'', err=b''):
        self.assertEqual(self.run_cli(args, data), (status, out, err))

    def test_cp_integration(self):
        payload = (self.cwd / 'data').read_bytes()
        self.check(['cp', 'data', 'copy'])
        self.assertEqual((self.cwd / 'copy').read_bytes(), payload)
        expected = b'{"source":"data","destination":"two words","status":"ok"}\n'
        self.check(data=b'cp --json data "two words"\n'
                   b'cp --json data "two words"\n', out=expected * 2)
        self.assertEqual((self.cwd / 'two words').read_bytes(), payload)
        failure = self.run_cli(['missing', 'copy'], binary=STANDALONE / 'cp')
        self.assertEqual(self.run_cli(['cp', 'missing', 'copy']), failure)
        self.assertEqual(self.run_cli(data=b'cp missing copy\n'), failure)
        self.check(data=b'cp data copy\npwd\n',
                   out=os.fsencode(self.cwd) + b'\n')

    def test_rg_integration(self):
        expected = b'data:2:second\n'
        self.check(['rg', '-n', 'second', 'data'], out=expected)
        self.check(data=b'rg -n second data\nrg -n second data\npwd\n',
                   out=expected * 2 + os.fsencode(self.cwd) + b'\n')
        self.check(['rg', 'absent', 'data'], status=1)
        self.check(data=b'rg absent data\nexit\n', status=1)
        invalid = self.run_cli(['rg', '[', 'data'])
        self.assertEqual(invalid[0], 2)
        self.assertTrue(invalid[2])
        self.assertEqual(self.run_cli(data=b'rg "[" data\nexit\n'), invalid)
        self.check(data=b'rg "[" data\nrg -n second data\n',
                   out=expected, err=invalid[2])

    def test_touch_integration(self):
        payload = (self.cwd / 'data').read_bytes()
        os.utime(self.cwd / 'data', ns=(1, 1))
        self.check(['touch', 'data'])
        self.assertGreater((self.cwd / 'data').stat().st_mtime_ns, 1)
        self.assertEqual((self.cwd / 'data').read_bytes(), payload)
        expected = b'[{"path":"two words","status":"ok","message":null}]\n'
        self.check(data=b'touch --json "two words"\n'
                   b'touch --json "two words"\n', out=expected * 2)
        self.assertEqual((self.cwd / 'two words').read_bytes(), b'')
        failure = self.run_cli(['missing/child'], binary=STANDALONE / 'touch')
        self.assertEqual(self.run_cli(['touch', 'missing/child']), failure)
        self.assertEqual(self.run_cli(data=b'touch missing/child\n'), failure)

    def test_dispatch_pwd(self):
        self.check(['pwd'], out=os.fsencode(self.cwd) + b'\n')

    def test_dispatch_help(self):
        self.check(['--help'], out=HELP)
        self.check(['help'], out=HELP)
        for name in STANDALONE_NAMES:
            expected = self.run_cli(['--help'], binary=STANDALONE / name)
            self.assertEqual(self.run_cli([name, '--help']), expected)
            # Existing head/tail run tables size repeated -n by argc;
            # print_usage uses capacity 1. Do not change module behavior.
            callback_out = expected[1]
            if name in ('head', 'tail'):
                callback_out = callback_out.replace(
                    b'[-n N] [-n N]', b'[-n N]', 1)
            self.assertEqual(self.run_cli(['help', name]),
                             (expected[0], callback_out, expected[2]))

    def test_dispatch_unknown_and_arity(self):
        self.check(['not-a-command'], status=127,
                   err=b'main: not-a-command: unknown command\n')
        self.check(['help', 'missing'], status=127,
                   err=b'main: missing: unknown command\n')
        self.check(['help', 'pwd', 'extra'], status=2,
                   err=b'main: usage: help [COMMAND]\n')
        self.check(['exit', '1'], status=2,
                   err=b'main: usage: exit\n')
        self.check(['exit'])
        self.check(['--help', 'extra'], status=2,
                   err=b'main: usage: main --help\n')

    def test_dispatch_cd_help(self):
        direct = self.run_cli(['cd', '--help'])
        self.assertEqual(direct[0], 0)
        self.assertEqual(direct[2], b'')
        self.assertTrue(direct[1].startswith(b'Usage: cd'))
        self.assertEqual(self.run_cli(['help', 'cd']), direct)
        self.assertEqual(self.run_cli(['cd', '-h']), direct)

    def test_loop_cd_physical_persistence(self):
        nested = self.cwd / 'real' / 'nested'
        nested.mkdir(parents=True)
        (self.cwd / 'alias').symlink_to('real/nested')
        expected = b''.join(os.fsencode(path) + b'\n' for path in
                            [nested, nested.parent, nested.parent])
        invalid = self.run_cli(['cd', 'missing'])
        self.assertEqual(invalid[0], 1)
        help_result = self.run_cli(['help', 'cd'])
        self.check(data=b'cd alias\npwd\nclear\ncd ..\npwd\n'
                   b'cd missing\npwd\nhelp cd\ncd --help\n',
                   out=expected + help_result[1] * 2, err=invalid[2])

    def test_dispatch_standalone_parity(self):
        cases = {'cat': ['-n', 'data'], 'head': ['-n', '2', 'data'],
                 'tail': ['-n', '2', 'data'], 'pwd': ['--json'],
                 'stat': ['--json', 'data'], 'ls': ['-a', '--json'],
                 'mkdir': ['-p', 'directory'], 'touch': ['--json', 'data'],
                 'clear': [], 'cp': ['--json', 'data', 'copy'],
                 'rg': ['--json', 'second', 'data']}
        self.assertEqual(list(cases), STANDALONE_NAMES)
        for name, args in cases.items():
            with self.subTest(name=name):
                expected = self.run_cli(args, binary=STANDALONE / name)
                self.assertEqual(self.run_cli([name, *args]), expected)
                invalid = ['--not-an-option']
                expected = self.run_cli(invalid, binary=STANDALONE / name)
                self.assertEqual(self.run_cli([name, *invalid]), expected)
        self.assertTrue((self.cwd / 'directory').is_dir())

    def test_dispatch_legacy(self):
        repo = Path(__file__).resolve().parents[1]
        proc = self.run_cli(['-r', str(repo / 'tests/fixtures/tree')])
        self.assertEqual(proc[0], 0)
        self.assertEqual(proc[2], b'')
        self.assertEqual(sorted(proc[1].splitlines()),
                         sorted([b'tree', b'child', b'top.txt', b'leaf.txt']))
        self.assertTrue(self.run_cli(['-h'])[1].startswith(
            b'Usage: main -r DIR '))
        self.assertEqual(self.run_cli(['-r'])[0], 2)

    def test_dispatch_legacy_diagnostics(self):
        # Accepted static argtable getopt interposition, not libc wording.
        # Ignore only the program prefix, never diagnostic punctuation/text.
        cases = [
            (['-r'], b'option requires an argument -- r\n'),
            (['--recursive'], b"option `--recursive' requires an argument\n"),
            (['-Z'], b'invalid option -- Z\n'),
            (['--bogus'], b"unrecognized option `--bogus'\n"),
            (['--mtime=x'], b"option `--mtime' doesn't allow an argument\n"),
        ]
        for args, diagnostic in cases:
            with self.subTest(args=args):
                status, out, err = self.run_cli(args)
                self.assertEqual((status, out), (2, b''))
                prefix, separator, message = err.partition(b': ')
                self.assertTrue(prefix)
                self.assertEqual(separator, b': ')
                self.assertEqual(message, diagnostic)

    def test_loop_tail_zero_preserves_next_command(self):
        # Characterization: existing tail returns before reading for -n 0.
        self.check(data=b'tail -n 0\npwd\n',
                   out=os.fsencode(self.cwd) + b'\n')

    def test_loop_multiple_and_eof(self):
        pwd = os.fsencode(self.cwd) + b'\n'
        self.check(data=b'pwd\npwd\n', out=pwd * 2)
        self.check(data=b'\n\t  \r\npwd', out=pwd)
        self.check()
        self.check(data=b'help\npwd\nexit\nnot-executed\n', out=HELP + pwd)

    def test_loop_status_and_continuation(self):
        pwd = os.fsencode(self.cwd) + b'\n'
        error = b'main: missing: unknown command\n'
        self.check(data=b'missing\n', status=127, err=error)
        self.check(data=b'missing\nexit\npwd\n', status=127, err=error)
        self.check(data=b'missing\n\n\n', status=127, err=error)
        self.check(data=b'missing\npwd\n', out=pwd, err=error)
        self.check(data=b'exit bad\npwd\n', out=pwd,
                   err=b'main: usage: exit\n')
        expected = self.run_cli(['stat', 'nonexistent'])
        self.assertEqual(self.run_cli(data=b'stat nonexistent\n'), expected)
        self.assertEqual(self.run_cli(data=b'head -n bad data\n'),
                         self.run_cli(['head', '-n', 'bad', 'data']))

    def test_loop_stdin_not_read_ahead(self):
        pwd = os.fsencode(self.cwd) + b'\n'
        self.check(data=b'head -n 1\npayload\npwd\n', out=b'payload\n' + pwd)
        self.check(data=b'head -n 0\npwd\n', out=pwd)
        self.check(data=b'cat\nthis is data\npwd\n',
                   out=b'this is data\npwd\n')
        self.check(data=b'tail -n 1\ndata\npwd\n', out=b'pwd\n')
        self.check(data=b'help\nhead -n 1\ndata\npwd\n',
                   out=HELP + b'data\n' + pwd)

    def test_loop_repeated_modules(self):
        (self.cwd / 'directory').mkdir()
        commands = ['cat -n data', 'cat data', 'head -n 1 data',
                    'head -n 2 data', 'tail -n 1 data', 'tail -n 2 data',
                    'pwd', 'pwd --json', 'stat --json data', 'ls -a --json',
                    'mkdir -p directory', 'mkdir -p directory']
        expected = b''.join(self.run_cli(cmd.split())[1] for cmd in commands)
        self.check(data=('\n'.join(commands) + '\n').encode(), out=expected)
        for name in BUILTIN_NAMES:
            help_result = self.run_cli(['help', name])
            invalid = self.run_cli([name, '--not-an-option'])
            data = f'help {name}\n{name} --not-an-option\nhelp {name}\n'.encode()
            self.check(data=data, out=help_result[1] * 2, err=invalid[2])

    def test_quotes_fragments_and_escapes(self):
        fixtures = {'two words': b'space\n', 'a"b': b'quote\n',
                    "a'b": b'single\n', 'a\\b': b'slash\n',
                    'ab cd': b'joined\n', '|;&<>': b'meta\n',
                    '$HOME`x`*?[]#': b'literal\n'}
        for name, data in fixtures.items():
            (self.cwd / name).write_bytes(data)
        cases = [b"cat 'two words'", b'cat "two words"',
                 b'cat two\\ words', b'cat "a\\\"b"',
                 b"cat \"a'b\"", b"cat 'a\\b'", b'cat a\\\\b',
                 b"cat a'b c'\"d\"", b"cat '|;&<>'",
                 b'cat \\|\\;\\&\\<\\>', b'cat $HOME`x`*?[]#']
        expected = [b'space\n'] * 3 + [b'quote\n', b'single\n'] + [
            b'slash\n'] * 2 + [b'joined\n', b'meta\n', b'meta\n', b'literal\n']
        for cmd, out in zip(cases, expected):
            self.check(data=cmd + b'\n', out=out)
        self.assertEqual(self.run_cli(data=b'cat ""\n'), self.run_cli(['cat', '']))
        self.assertEqual(self.run_cli(data=b"cat ''\n"), self.run_cli(['cat', '']))
        self.check(data=b"'pwd'\n", out=os.fsencode(self.cwd) + b'\n')

    def test_syntax_errors_no_partial_execution(self):
        cases = [(b"mkdir made 'unfinished", b'unterminated quote'),
                 (b'mkdir made "unfinished', b'unterminated quote'),
                 (b'mkdir made \\', b'dangling escape'),
                 (b'mkdir made\x00other', b'NUL byte')]
        cases += [(b'mkdir made ' + bytes([ch]) + b' pwd',
                   b'unsupported operator') for ch in b'|&;<>']
        for data, reason in cases:
            with self.subTest(data=data):
                self.check(data=data + b'\n', status=2,
                           err=b'main: syntax error: ' + reason + b'\n')
                self.assertFalse((self.cwd / 'made').exists())
                self.check(data=data + b'\npwd\n',
                           out=os.fsencode(self.cwd) + b'\n',
                           err=b'main: syntax error: ' + reason + b'\n')
        self.check(data=b'pwd\\', status=2,
                   err=b'main: syntax error: dangling escape\n')

    def test_long_lines_many_tokens(self):
        self.check(data=b' ' * 17000 + b'pwd\n',
                   out=os.fsencode(self.cwd) + b'\n')
        data = b'cat ' + b'data ' * 600 + b'\n'
        self.check(data=data, out=(self.cwd / 'data').read_bytes() * 600)
        before = (self.cwd / 'data').read_bytes()
        self.check(data=b'cat data\n' * 100, out=before * 100)
        self.assertEqual((self.cwd / 'data').read_bytes(), before)

    def test_no_external_fallback_or_expansion(self):
        self.check(['echo', 'unexpected'], status=127,
                   err=b'main: echo: unknown command\n')
        self.check(data=b'/usr/bin/printf external\n', status=127,
                   err=b'main: /usr/bin/printf: unknown command\n')
        self.check(data=b'# comment\n', status=127,
                   err=b'main: #: unknown command\n')
        self.check(data=b'$(pwd)\n', status=127,
                   err=b'main: $(pwd): unknown command\n')

    def test_output_and_input_failure(self):
        with open('/dev/full', 'wb') as out:
            proc = subprocess.run([PROGRAM, '--help'], stdout=out,
                                  stderr=subprocess.PIPE, timeout=5)
            self.assertEqual(proc.returncode, 1)
        # A directory descriptor is readable-open but read fails with EISDIR.
        fd = os.open(self.cwd, os.O_RDONLY | os.O_DIRECTORY)
        self.addCleanup(os.close, fd)
        proc = subprocess.run([PROGRAM], stdin=fd, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              env={**os.environ, 'LC_ALL': 'C'}, timeout=5)
        self.assertEqual((proc.returncode, proc.stdout, proc.stderr),
                         (1, b'', b'main: standard input: Is a directory\n'))

    def test_generated_quoted_filenames(self):
        import random
        rng = random.Random(420)
        alphabet = 'abc XY\t\'"\\$`*?[]#|&;<>é'
        commands = []
        expected = b''
        for index in range(180):
            name = f'{index}_' + ''.join(rng.choice(alphabet)
                                        for _ in range(rng.randrange(1, 40)))
            payload = bytes([index, 0, 255]) + b'\n'
            (self.cwd / name).write_bytes(payload)
            if index % 3 == 0:
                argument = "'" + name.replace("'", "'\\''") + "'"
            elif index % 3 == 1:
                argument = '"' + name.replace('\\', '\\\\').replace(
                    '"', '\\"') + '"'
            else:
                argument = ''.join('\\' + ch for ch in name)
            commands.append('cat -- ' + argument)
            expected += payload
        self.check(data=('\n'.join(commands) + '\n').encode(), out=expected)

    def test_interactive_prompt(self):
        import pty
        master, slave = pty.openpty()
        try:
            proc = subprocess.Popen([PROGRAM], stdin=slave,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, cwd=self.cwd)
            try:
                os.write(master, b'pwd\nexit\n')
                out, err = proc.communicate(timeout=5)
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.communicate()
            self.assertEqual((proc.returncode, out, err),
                             (0, os.fsencode(self.cwd) + b'\n',
                              b'main> main> '))
        finally:
            os.close(master)
            os.close(slave)


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ShellTests)
    if ARGS.slice == 'dispatch':
        suite = unittest.TestSuite(t for t in suite
                                   if 'test_dispatch_' in t.id())
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(not result.wasSuccessful())
