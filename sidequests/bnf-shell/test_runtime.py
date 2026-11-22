"""End-to-end tests for the supported shell subset."""
import os
from pathlib import Path
import subprocess
import select
import tempfile
import unittest

SHELL = Path(os.environ.get('BNF_SHELL',
             Path(__file__).resolve().parent / 'build/shell'))


class RuntimeTests(unittest.TestCase):
    def run_shell(self, text, **kwargs):
        return subprocess.run([str(SHELL)], input=text, text=True,
                              capture_output=True, timeout=8, **kwargs)

    def test_mixed_pipeline(self):
        r = self.run_shell('echo hello | cat | wc\n')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.split(), ['1', '1', '6'])

    def test_redirection_and_sequence(self):
        with tempfile.TemporaryDirectory() as d:
            r = self.run_shell('echo hello > output ; cat < output\n', cwd=d)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(r.stdout, 'hello\n')
            self.assertEqual(Path(d, 'output').read_text(), 'hello\n')

    def test_failed_open_starts_no_stage(self):
        with tempfile.TemporaryDirectory() as d:
            r = self.run_shell('touch marker | cat < absent\n', cwd=d)
            self.assertNotEqual(r.returncode, 0)
            self.assertFalse(Path(d, 'marker').exists())

    def test_rejects_fifo_redirection_without_blocking(self):
        with tempfile.TemporaryDirectory() as d:
            os.mkfifo(Path(d, 'fifo'))
            Path(d, 'alias').symlink_to('fifo')
            for text in (
                'echo hello > fifo | cat < fifo\n',
                'echo hello > fifo & cat < fifo\n',
                'touch marker | cat < fifo\n',
                'cat < alias\n',
            ):
                with self.subTest(text=text):
                    try:
                        r = subprocess.run(
                            [str(SHELL)], input=text, text=True,
                            capture_output=True, timeout=1, cwd=d,
                        )
                    except subprocess.TimeoutExpired:
                        self.fail('FIFO redirection blocked preparation.')
                    self.assertNotEqual(r.returncode, 0)
                    self.assertEqual(r.stdout, '')
                    self.assertNotEqual(r.stderr, '')
                    self.assertFalse(Path(d, 'marker').exists())

    def test_last_pipeline_status(self):
        self.assertEqual(self.run_shell('false | true\n').returncode, 0)
        self.assertEqual(self.run_shell('true | false\n').returncode, 1)

    def test_missing_command(self):
        self.assertEqual(self.run_shell('missingbnfcommand\n').returncode, 127)

    def test_external_input_not_read_ahead(self):
        r = self.run_shell('cat\nhello from stdin\n')
        self.assertEqual(r.stdout, 'hello from stdin\n')
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_words_flags_and_large_pipeline(self):
        r = self.run_shell('seq 1 100000 | cat | wc -l\n')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.strip(), '100000')

    def test_local_variable_and_export(self):
        env = os.environ.copy()
        env.pop('BNF_LOCAL', None)
        r = self.run_shell('BNF_LOCAL=5 ; echo $BNF_LOCAL\n'
                           'printenv BNF_LOCAL\n', env=env)
        self.assertEqual(r.stdout, '5\n')
        self.assertEqual(r.returncode, 1, r.stderr)
        r = self.run_shell('BNF_LOCAL=5\nexport BNF_LOCAL\n'
                           'printenv BNF_LOCAL\nBNF_LOCAL=6\n'
                           'printenv BNF_LOCAL\n', env=env)
        self.assertEqual(r.stdout, '5\n6\n')
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_unset_variable_is_empty_argument(self):
        r = self.run_shell('echo before $BNF_UNSET_TEST after\n')
        self.assertEqual(r.stdout, 'before  after\n')

    def test_conditional_status(self):
        r = self.run_shell('if true ; then echo yes ; fi\n'
                           'if false ; then echo no ; fi\n')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, 'yes\n')
        self.assertEqual(self.run_shell(
            'if true ; then false ; fi\n').returncode, 1)

    def test_cd_and_exit_in_main_thread(self):
        with tempfile.TemporaryDirectory() as d:
            r = self.run_shell(f'cd {d}\npwd\nexit 7 ; echo no\n')
            self.assertEqual(r.stdout.strip(), d)
            self.assertEqual(r.returncode, 7, r.stderr)

    def test_state_commands_rejected_in_pipeline(self):
        with tempfile.TemporaryDirectory() as d:
            r = self.run_shell('touch marker | cd .\n', cwd=d)
            self.assertNotEqual(r.returncode, 0)
            self.assertFalse(Path(d, 'marker').exists())

    def test_registered_hello(self):
        env = os.environ.copy()
        with tempfile.TemporaryDirectory() as d:
            env['PATH'] = d
            r = self.run_shell('hello\n', env=env)
        self.assertEqual(r.stdout, 'Hello from bnf-shell\n')
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_substitution_pipeline_variable_and_empty(self):
        r = self.run_shell('x=hello\necho before `echo $x | cat` after\n'
                           'echo before `true` after\n')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, 'before hello after\nbefore  after\n')

    def test_substitution_trims_all_final_newlines(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'input.txt').write_text('one\ntwo\n\n')
            r = self.run_shell('echo `cat input.txt`\n', cwd=d)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, 'one\ntwo\n')

    def test_substitution_failure_does_not_start_outer(self):
        r = self.run_shell('echo no `false`\n')
        self.assertNotEqual(r.returncode, 0)
        self.assertEqual(r.stdout, '')

    def test_substitution_large_capture_and_limit(self):
        r = self.run_shell('echo `seq 1 5000` | wc -l\n')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.strip(), '5000')
        r = self.run_shell('echo no `seq 1 100000`\n')
        self.assertNotEqual(r.returncode, 0)
        self.assertEqual(r.stdout, '')
        self.assertIn('substitution', r.stderr.lower())

    def test_background_nonblocking_and_owned_arguments(self):
        with tempfile.TemporaryDirectory() as d:
            with subprocess.Popen([str(SHELL)], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True,
                                  cwd=d) as p:
                assert p.stdin is not None and p.stdout is not None
                p.stdin.write('sleep 1 & echo ready\n')
                p.stdin.flush()
                self.assertTrue(select.select([p.stdout], [], [], .6)[0])
                self.assertEqual(p.stdout.readline(), 'ready\n')
                p.stdin.write('x=old\necho $x > bg.txt &\nx=new\n'
                              'echo $x\n')
                out, err = p.communicate(timeout=5)
                self.assertEqual(p.returncode, 0, err)
                self.assertEqual(out, 'new\n')
            self.assertEqual(Path(d, 'bg.txt').read_text(), 'old\n')

    def test_background_reaps_and_does_not_steal_stdin(self):
        r = self.run_shell('cat & echo foreground\n' + 'true &\n' * 80)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, 'foreground\n')

    def test_state_commands_rejected_in_substitution_and_background(self):
        for text in ('echo `cd .`\n', 'exit &\n', 'cd . &\n'):
            with self.subTest(text=text):
                r = self.run_shell(text)
                self.assertNotEqual(r.returncode, 0)

    def test_combined_redirection_and_override_pipe(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'in.txt').write_text('file\n')
            r = self.run_shell('cat > out.txt < in.txt ; '
                               'echo pipe | cat < in.txt\n', cwd=d)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(r.stdout, 'file\n')
            self.assertEqual(Path(d, 'out.txt').read_text(), 'file\n')

    def test_sigpipe_and_internal_output_failure(self):
        r = self.run_shell('seq 1 100000 | head -n 1\n')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, '1\n')
        r = self.run_shell('echo hello > /dev/full\n')
        self.assertNotEqual(r.returncode, 0)

    def test_exit_uses_previous_status_and_validates_number(self):
        r = self.run_shell('false ; exit ; echo no\n')
        self.assertEqual(r.returncode, 1)
        self.assertEqual(r.stdout, '')
        for code in ('256', '-1', 'nonsense'):
            r = self.run_shell(f'exit {code}\n')
            self.assertNotEqual(r.returncode, 0)

    def test_last_command_status_across_lines(self):
        self.assertEqual(self.run_shell('false\ntrue\n').returncode, 0)

    def test_assignment_resets_status_before_exit(self):
        self.assertEqual(self.run_shell('false\nx=5 ; exit\n').returncode, 0)

    def test_partial_parse_cleanup(self):
        r = self.run_shell('echo partial |\necho `echo partial\n'
                           'x=one ; echo two |\necho recovered\n')
        self.assertEqual(r.returncode, 1, r.stderr)
        self.assertEqual(r.stdout, 'recovered\n')
        self.assertNotIn('Sanitizer', r.stderr)

    def test_limits_and_line_recovery(self):
        for text in ('echo ' + 'x ' * 256 + '\n',
                     ' | '.join(['true'] * 65) + '\n',
                     'x' * 65537 + '\n'):
            with self.subTest(length=len(text)):
                r = self.run_shell(text)
                self.assertNotEqual(r.returncode, 0)
        r = self.run_shell('x' * 65537 + '\necho recovered\n')
        self.assertEqual(r.returncode, 1)
        self.assertEqual(r.stdout, 'recovered\n')

    def test_capture_exact_limit_and_binary_rejection(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'limit').write_bytes(b'x' * 65536)
            r = self.run_shell('echo `cat limit` | wc -c\n', cwd=d)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(r.stdout.strip(), '65537')
            Path(d, 'limit').write_bytes(b'x' * 65537)
            r = self.run_shell('echo `cat limit`\n', cwd=d)
            self.assertNotEqual(r.returncode, 0)
            self.assertEqual(r.stdout, '')
            Path(d, 'limit').write_bytes(b'one\x00two')
            self.assertNotEqual(self.run_shell(
                'echo `cat limit`\n', cwd=d).returncode, 0)

    def test_variable_name_and_capacity(self):
        self.assertNotEqual(self.run_shell('5bad=value\n').returncode, 0)
        r = self.run_shell(''.join(f'V{i}=value\n' for i in range(129)))
        self.assertNotEqual(r.returncode, 0)

    def test_background_limit(self):
        r = self.run_shell('sleep .3 &\n' * 33)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('job limit', r.stderr)

    def test_eof_without_newline(self):
        r = self.run_shell('echo final')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, 'final\n')

    def test_background_children_reaped_at_checkpoint(self):
        with subprocess.Popen([str(SHELL)], stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True) as p:
            assert p.stdin is not None and p.stdout is not None
            p.stdin.write('true &\nsleep .1\necho checkpoint\n')
            p.stdin.flush()
            self.assertEqual(p.stdout.readline(), 'checkpoint\n')
            # The next input boundary polls jobs before it reads a new line.
            p.stdin.write('echo reaped\n')
            p.stdin.flush()
            self.assertEqual(p.stdout.readline(), 'reaped\n')
            children = Path(f'/proc/{p.pid}/task/{p.pid}/children')
            self.assertEqual(children.read_text().strip(), '')
            _, err = p.communicate(timeout=5)
            self.assertEqual(p.returncode, 0, err)
