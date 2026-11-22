"""Real helper and driver tests. Helpers are local, controlled fixtures."""
import os
from pathlib import Path
import subprocess
import select
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parent
SHELL = Path(os.environ.get('BNF_SHELL', ROOT / 'build/shell'))


class NaturalInputTests(unittest.TestCase):
    def test_fixed_stub_is_opt_in_and_receives_query(self):
        env = dict(os.environ, BNF_NL_HELPER=str(ROOT / 'mysh_llm_stub.py'))
        result = subprocess.run([str(SHELL)], input='@anything\nyes\n',
                                text=True, capture_output=True, timeout=5,
                                env=env)
        self.assertEqual(result.stdout, 'hello\n')
        self.assertEqual(result.returncode, 0, result.stderr)

    def run_helper(self, body, text='@greet me\nyes\n'):
        with tempfile.TemporaryDirectory() as directory:
            helper = Path(directory, 'helper')
            helper.write_text('#!/usr/bin/python3\n' + body)
            helper.chmod(0o700)
            env = dict(os.environ, BNF_NL_HELPER=str(helper))
            return subprocess.run([str(SHELL)], input=text, text=True,
                                  capture_output=True, timeout=5, env=env,
                                  cwd=directory)

    def test_confirmed_proposal_uses_runtime(self):
        result = self.run_helper("print('echo hello | cat')\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'hello\n')
        self.assertIn('echo hello | cat', result.stderr)
        self.assertIn('yes', result.stderr)

    def test_failed_helper_stops_script_before_unclaimed_reply(self):
        result = self.run_helper('raise SystemExit(1)\n',
                                 '@query\necho not-a-command\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '')

    def test_only_exact_yes_confirms(self):
        for answer in ('no', 'y', 'YES', ' yes', 'yes ', '',
                       'echo never-run', 'x' * 100, 'yes\x00'):
            with self.subTest(answer=answer):
                result = self.run_helper("print('echo unwanted')\n",
                                         '@query\n' + answer + '\n')
                self.assertEqual(result.stdout, '')

    def test_eof_does_not_confirm(self):
        result = self.run_helper("print('echo unwanted')\n", '@query\n')
        self.assertEqual(result.stdout, '')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_refusal_consumes_one_line_only(self):
        result = self.run_helper("print('echo unwanted')\n",
                                 '@query\nno\necho next\n')
        self.assertEqual(result.stdout, 'next\n')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_query_is_one_literal_argument(self):
        query = ' spaces ; $HOME `touch marker` "quoted" @ ! '
        body = ('import sys\n'
                f'assert sys.argv[1:] == [{query!r}]\n'
                "from pathlib import Path\n"
                "assert not Path('marker').exists()\n"
                "print('echo literal')\n")
        result = self.run_helper(body, '@' + query + '\nyes\n')
        self.assertEqual(result.stdout, 'literal\n')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_helper_has_null_stdin_and_silent_stderr(self):
        result = self.run_helper('import sys\n'
                                 'assert sys.stdin.read() == ""\n'
                                 'sys.stderr.write("\\x1b[2J")\n'
                                 "print('echo safe')\n")
        self.assertEqual(result.stdout, 'safe\n')
        self.assertNotIn('\x1b', result.stderr)

    def test_output_protocol_rejects_bad_bytes_and_lines(self):
        for output in (b'', b'\n', b'   ', b'echo no\necho no\n',
                       b'echo no\n\n', b'echo\x00no', b'echo\tno',
                       b'echo no\r\n', b'echo \x1b[2J', b'echo \x7f',
                       b'echo \xc2\xa0', b'x' * 4097,
                       b'x' * 100000):
            with self.subTest(output=output[:25]):
                result = self.run_helper('import os\n'
                                         f'os.write(1, {output!r})\n')
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, '')
                self.assertNotIn('Type yes', result.stderr)
                self.assertNotIn('\x1b', result.stderr)

    def test_helper_nonzero_or_signal_rejects_even_valid_output(self):
        for ending in ('raise SystemExit(7)',
                       'import os, signal; os.kill(os.getpid(), 9)'):
            result = self.run_helper("print('echo unwanted', flush=True)\n"
                                     + ending + '\n')
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, '')

    def test_no_trailing_newline_is_allowed(self):
        result = self.run_helper("import os\nos.write(1, b'echo hello')\n")
        self.assertEqual(result.stdout, 'hello\n')

    def test_exact_proposal_limit(self):
        proposal = 'echo ' + 'x' * 4091
        result = self.run_helper(f'print({proposal!r})\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'x' * 4091 + '\n')

    def test_query_limits(self):
        for query in ('', 'x' * 4097):
            result = self.run_helper("print('echo unwanted')\n",
                                     '@' + query + '\nyes\n')
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, '')
        result = self.run_helper("print('echo exact')\n",
                                 '@' + 'x' * 4096 + '\nyes\n')
        self.assertEqual(result.stdout, 'exact\n')

    def test_driver_line_overflow_aborts_at_input(self):
        result = self.run_helper("print('echo unwanted')\n",
                                 '@' + 'x' * 65536 +
                                 '\necho not-a-command\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '')

    def test_recursive_proposal_rejected(self):
        for proposal in ('@again', '  @again'):
            result = self.run_helper(f'print({proposal!r})\n')
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn('Type yes', result.stderr)

    def test_invalid_syntax_has_no_execution(self):
        result = self.run_helper("print('echo unwanted |')\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '')

    def test_approved_command_uses_shell_state(self):
        result = self.run_helper("print('x=world ; echo $x')\n",
                                 '@query\nyes\necho $x\n')
        self.assertEqual(result.stdout, 'world\nworld\n')

    def test_helper_timeout_with_open_or_closed_stdout(self):
        for setup in ('', 'os.close(1)\n'):
            result = self.run_helper('import os, time\n' + setup +
                                     'time.sleep(30)\n')
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, '')

    def test_timeout_also_kills_child_that_changes_group(self):
        start = time.monotonic()
        result = self.run_helper('import os, signal, time\n'
                                 'signal.alarm(3)\n'
                                 'os.setpgid(0, os.getpgid(os.getppid()))\n'
                                 'time.sleep(30)\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertLess(time.monotonic() - start, 2.5)

    def test_missing_or_relative_helper_rejected(self):
        for path in ('', 'helper', '/no/such/bnf-helper'):
            env = dict(os.environ, BNF_NL_HELPER=path)
            result = subprocess.run([str(SHELL)], input='@query\nyes\n',
                                    text=True, capture_output=True,
                                    timeout=5, env=env)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, '')


    def test_no_side_effect_before_confirmation(self):
        with tempfile.TemporaryDirectory() as directory:
            helper = Path(directory, 'helper')
            helper.write_text("#!/usr/bin/python3\nprint('touch marker')\n")
            helper.chmod(0o700)
            env = dict(os.environ, BNF_NL_HELPER=str(helper))
            with subprocess.Popen([str(SHELL)], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True,
                                  env=env, cwd=directory) as process:
                assert process.stdin is not None
                assert process.stderr is not None
                process.stdin.write('@query\n')
                process.stdin.flush()
                self.assertTrue(select.select([process.stderr], [], [], 3)[0])
                self.assertEqual(process.stderr.readline(),
                                 'Suggested command:\n')
                self.assertEqual(process.stderr.readline(), 'touch marker\n')
                self.assertFalse(Path(directory, 'marker').exists())
                out, err = process.communicate('yes\n', timeout=5)
                self.assertEqual(process.returncode, 0, err)
                self.assertEqual(out, '')
                self.assertTrue(Path(directory, 'marker').exists())

    def test_repeated_helpers_release_fds_and_children(self):
        with tempfile.TemporaryDirectory() as directory:
            helper = Path(directory, 'helper')
            helper.write_text("#!/usr/bin/python3\nprint('echo done')\n")
            helper.chmod(0o700)
            env = dict(os.environ, BNF_NL_HELPER=str(helper))
            with subprocess.Popen([str(SHELL)], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, text=True,
                                  env=env) as process:
                assert process.stdin is not None
                assert process.stdout is not None
                process.stdin.write('echo ready\n')
                process.stdin.flush()
                self.assertTrue(select.select([process.stdout], [], [], 3)[0])
                self.assertEqual(process.stdout.readline(), 'ready\n')
                fd_dir = Path(f'/proc/{process.pid}/fd')
                baseline = len(list(fd_dir.iterdir()))
                for _ in range(40):
                    process.stdin.write('@query\nyes\n')
                    process.stdin.flush()
                    self.assertTrue(select.select([process.stdout], [], [], 3)[0])
                    self.assertEqual(process.stdout.readline(), 'done\n')
                process.stdin.write('echo checkpoint\n')
                process.stdin.flush()
                self.assertTrue(select.select([process.stdout], [], [], 3)[0])
                self.assertEqual(process.stdout.readline(), 'checkpoint\n')
                self.assertEqual(len(list(fd_dir.iterdir())), baseline)
                children = Path(f'/proc/{process.pid}/task/{process.pid}/children')
                self.assertEqual(children.read_text().strip(), '')
                process.communicate(timeout=5)
                self.assertEqual(process.returncode, 0)


if __name__ == '__main__':
    unittest.main()
