#!/usr/bin/env python3
"""Real main-loop integration tests; no third-party Python dependencies."""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROGRAM = Path(os.environ.get('SHELL_UNDER_TEST', ROOT / 'Unix_Shell')).resolve()


class ShellTests(unittest.TestCase):
    def test_tokenizer_build_modes_have_matching_behavior(self):
        import tempfile
        harness = r'''
#include <assert.h>
#include <string.h>
#include "token.h"
static int calls;
char *__real_strtok(char *, const char *);
char *__wrap_strtok(char *s, const char *d)
{ ++calls; return __real_strtok(s, d); }
int main(void)
{
    char line[] = " \t head\r\n-n\v2\f file.txt  ";
    char blank[] = " \t\r\n\v\f";
    char *tokens[MAX_NUM_TOKENS + 1];
    assert(tokenise(line, tokens) == 4);
    assert(strcmp(tokens[0], "head") == 0);
    assert(strcmp(tokens[1], "-n") == 0);
    assert(strcmp(tokens[2], "2") == 0);
    assert(strcmp(tokens[3], "file.txt") == 0 && tokens[4] == NULL);
    assert(tokenise(blank, tokens) == 0 && tokens[0] == NULL);
    assert(USE_STRTOK ? calls > 0 : calls == 0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            source = Path(temp) / 'token_modes.c'
            executable = Path(temp) / 'token_modes'
            source.write_text(harness)
            for mode in (0, 1):
                with self.subTest(USE_STRTOK=mode):
                    subprocess.run(['cc', '-std=c17', '-Wall', '-Wextra', '-Werror',
                                    f'-DUSE_STRTOK={mode}', '-I', str(ROOT), str(source),
                                    str(ROOT / 'token.c'), '-Wl,--wrap=strtok',
                                    '-o', str(executable)], check=True, capture_output=True)
                    result = subprocess.run([str(executable)], capture_output=True, timeout=5)
                    self.assertEqual(result.returncode, 0, result.stderr)

    def test_tokenizer_scans_manually_without_library_tokenizers(self):
        import tempfile
        harness = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"
char *__wrap_strtok(char *s, const char *d)
{ (void)s; (void)d; _Exit(99); }
char *__wrap_strtok_r(char *s, const char *d, char **state)
{ (void)s; (void)d; (void)state; _Exit(99); }
char *__wrap_strsep(char **s, const char *d)
{ (void)s; (void)d; _Exit(99); }
int main(void)
{
    char line[] = " \t head\r\n-n\v2\f file.txt  ";
    char other[] = "pwd";
    char blank[] = " \t\r\n\v\f";
    char *tokens[MAX_NUM_TOKENS + 1];
    char *second[MAX_NUM_TOKENS + 1];
    assert(tokenise(line, tokens) == 4);
    assert(tokens[0] == line + 3);
    assert(strcmp(tokens[0], "head") == 0);
    assert(strcmp(tokens[1], "-n") == 0);
    assert(strcmp(tokens[2], "2") == 0);
    assert(strcmp(tokens[3], "file.txt") == 0);
    assert(tokens[4] == NULL);
    assert(tokenise(other, second) == 1);
    assert(second[0] == other && second[1] == NULL);
    assert(strcmp(tokens[0], "head") == 0);
    assert(tokenise(blank, second) == 0 && second[0] == NULL);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            source = Path(temp) / 'manual_tokens.c'
            executable = Path(temp) / 'manual_tokens'
            source.write_text(harness)
            subprocess.run(['cc', '-std=c17', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT), str(source), str(ROOT / 'token.c'),
                            '-Wl,--wrap=strtok,--wrap=strtok_r,--wrap=strsep',
                            '-o', str(executable)], check=True, capture_output=True)
            result = subprocess.run([str(executable)], capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 0,
                             'manual tokenization required; library tokenizer exits 99')

    def run_shell(self, script, cwd=ROOT, env=None):
        return subprocess.run([str(PROGRAM)], input=script, text=True,
                              capture_output=True, cwd=cwd, env=env, timeout=5)

    def test_empty_eof_blank_and_exit_without_newline(self):
        for script in ('', '\n \t\n', 'exit', '\nexit\npwd\n'):
            with self.subTest(script=script):
                result = self.run_shell(script)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, '')
                self.assertEqual(result.stderr, '')

    def test_external_commands_arguments_cd_and_failure_recovery(self):
        import tempfile
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            folder = Path(temp)
            (folder / 'sample.txt').write_text('one\ntwo\nthree\n')
            script = (f'pwd\ncd {folder}\npwd\ncat sample.txt\n'
                      'head -n 2 sample.txt\ncat missing.txt\npwd\nexit\npwd\n')
            result = self.run_shell(script, env={'PATH': '/nonexistent'})
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, f'{ROOT}\n{folder}\none\ntwo\nthree\none\ntwo\n{folder}\n')
            self.assertIn('missing.txt', result.stderr)
            self.assertEqual(self.run_shell('cat missing.txt\n').returncode, 1)

    def test_unsupported_syntax_rejected_without_execution(self):
        for syntax in ('pwd|cat', 'pwd &', 'pwd;pwd', 'pwd > output',
                       'cat < input', 'cat *', 'cat ?', 'cat [ab]',
                       'cat "file"', "cat 'file'", 'cat $HOME',
                       'cat `pwd`', 'cat a\\b', '(pwd)', 'pwd # comment',
                       'cat ~', 'cat {a,b}'):
            with self.subTest(syntax=syntax):
                result = self.run_shell(syntax + '\npwd\nexit\n')
                self.assertEqual(result.stdout, str(ROOT) + '\n')
                self.assertIn('unsupported syntax', result.stderr)
        result = self.run_shell('echo no\npwd\nexit\n')
        self.assertEqual(result.stdout, str(ROOT) + '\n')
        self.assertIn('unknown command', result.stderr)

    def test_token_capacity_whitespace_and_unterminated_command(self):
        result = self.run_shell('  \t\r\v\f\n  pwd\t\r\n  pwd')
        self.assertEqual(result.stdout, (str(ROOT) + '\n') * 2)
        self.assertEqual(result.stderr, '')
        result = self.run_shell('pwd ' + 'x ' * 1000 + '\npwd\nexit\n')
        self.assertEqual(result.stdout, str(ROOT) + '\n')
        self.assertIn('too many tokens', result.stderr)
        # Exactly 1000 tokens must reach the command, not capacity rejection.
        result = self.run_shell('pwd ' + 'x ' * 999 + '\nexit\n')
        self.assertNotIn('too many tokens', result.stderr)
        self.assertNotEqual(result.stderr, '')

    def test_builtin_errors_do_not_exit_or_change_directory(self):
        for command, diagnostic in (('cd nowhere', 'cd:'),
                                    ('cd / /', 'usage: cd'),
                                    ('prompt', 'usage: prompt'),
                                    ('prompt a b', 'usage: prompt'),
                                    ('exit 9', 'usage: exit')):
            with self.subTest(command=command):
                result = self.run_shell(command + '\npwd\nexit\n')
                self.assertEqual(result.returncode, 0)
                self.assertEqual(result.stdout, str(ROOT) + '\n')
                self.assertIn(diagnostic, result.stderr)
        result = self.run_shell('cd\npwd\nexit\n', env={'HOME': str(ROOT.parent)})
        self.assertEqual(result.stdout, str(ROOT.parent) + '\n')
        result = self.run_shell('cd\npwd\nexit\n', env={})
        self.assertEqual(result.stdout, str(ROOT) + '\n')
        self.assertIn('HOME is not set', result.stderr)

    def test_long_prompt_owned_across_input_reads(self):
        prompt = 'p' * 4096
        result = self.run_shell(f'prompt {prompt}\npwd\nprompt short\npwd\nexit\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, (str(ROOT) + '\n') * 2)
        self.assertEqual(result.stderr, '')

    def test_embedded_nul_rejected_as_whole_line(self):
        result = self.run_shell('pwd\0ignored\npwd\nexit\n')
        self.assertEqual(result.stdout, str(ROOT) + '\n')
        self.assertIn('NUL byte', result.stderr)

    def test_repetition_and_eof_status_are_not_shell_control(self):
        result = self.run_shell('pwd\n' * 100)
        self.assertEqual(result.stdout, (str(ROOT) + '\n') * 100)
        self.assertEqual(result.returncode, 0)
        for script, code in (('unknown\n', 127), ('pwd|cat\n', 2),
                             ('cd nowhere\n', 1), ('exit extra\n', 2),
                             ('head --bad-option\npwd\n', 0),
                             ('pwd\ncat missing\n', 1)):
            with self.subTest(script=script):
                self.assertEqual(self.run_shell(script).returncode, code)

    def test_failed_exec_exits_child_and_parent_continues(self):
        import tempfile
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            folder = Path(temp)
            program = folder / 'shell'
            subprocess.run(['cc', '-std=c17', '-Wall', '-Wextra', '-Werror',
                            f'-DCOMMAND_DIR="{folder}"',
                            *[str(ROOT / name) for name in
                              ('Unix_Shell.c', 'token.c', 'command.c', 'builtin.c')],
                            '-o', str(program)], check=True, capture_output=True)
            for status in (127, 126):
                if status == 126:
                    (folder / 'pwd').write_text('not executable\n')
                result = subprocess.run([str(program)], input='pwd\n', text=True,
                                        capture_output=True, timeout=5)
                self.assertEqual(result.returncode, status, result.stderr)
                self.assertEqual(result.stderr.count(str(folder / 'pwd')), 1)
                result = subprocess.run([str(program)], input='pwd\npwd\nexit\npwd\n',
                                        text=True, capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, '')
                self.assertEqual(result.stderr.count(str(folder / 'pwd')), 2)

    def test_interactive_prompt_on_real_pty(self):
        import errno
        import pty
        import select
        import time
        pid, master = pty.fork()
        if pid == 0:
            os.chdir(ROOT)
            os.execve(str(PROGRAM), [str(PROGRAM)], {'PATH': '/nonexistent'})
        data = b''
        try:
            deadline = time.monotonic() + 5
            while not data.endswith(b'% '):
                self.assertLess(time.monotonic(), deadline, data)
                if select.select([master], [], [], 0.1)[0]:
                    data += os.read(master, 65536)
            os.write(master, b'prompt learn\npwd\nexit\n')
            while True:
                self.assertLess(time.monotonic(), deadline, data)
                if select.select([master], [], [], 0.1)[0]:
                    try:
                        chunk = os.read(master, 65536)
                    except OSError as error:
                        if error.errno == errno.EIO:
                            break
                        raise
                    if not chunk:
                        break
                    data += chunk
            _, status = os.waitpid(pid, 0)
            pid = None  # Reaped: never signal a PID that might be reused.
            self.assertEqual(os.waitstatus_to_exitcode(status), 0, data)
            self.assertIn(str(ROOT).encode() + b'\r\n', data)
            self.assertEqual(data.count(b'learn '), 2, data)
        finally:
            os.close(master)
            if pid is not None:
                try:
                    os.kill(pid, 9)
                    os.waitpid(pid, 0)
                except ProcessLookupError:
                    pass

    def test_fork_exec_wait_trace_pins_existing_programs(self):
        import re
        import shutil
        import tempfile
        strace = shutil.which('strace')
        if strace is None:
            self.skipTest('strace is not installed')
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            folder = Path(temp)
            (folder / 'input.txt').write_text('a\nb\nc\n')
            trace = folder / 'trace'
            result = subprocess.run([strace, '-f', '-e', 'trace=process', '-o', str(trace),
                                     str(PROGRAM)], input='pwd\ncat input.txt\nhead -n 2 input.txt\npwd\nexit\n',
                                    cwd=folder, text=True, capture_output=True, timeout=5,
                                    # LeakSanitizer cannot run under ptrace; all other
                                    # sanitized tests keep leak detection enabled.
                                    env={'PATH': '/nonexistent', 'ASAN_OPTIONS': 'detect_leaks=0'})
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, f'{folder}\na\nb\nc\na\nb\n{folder}\n')
            text = trace.read_text()
            paths = re.findall(r'execve\("([^"]+)"', text)
            expected = ROOT.parents[1] / 'build'
            self.assertEqual(paths, [str(PROGRAM)] +
                             [str(expected / name) for name in ('pwd', 'cat', 'head', 'pwd')])
            children = re.findall(r'^(\d+)\s+execve\("' + re.escape(str(expected)) + '/',
                                  text, re.MULTILINE)
            self.assertEqual(len(children), 4, text)
            for child in children:
                self.assertRegex(text, r'wait4\(' + child + r',')
                self.assertRegex(text, r'(?:wait4[^\n]*|<\.\.\. wait4 resumed>[^\n]*)= ' + child + r'\b')

    def test_foreground_wait_blocks_next_command_until_child_finishes(self):
        import select
        import tempfile
        import time
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            fifo = Path(temp) / 'input'
            os.mkfifo(fifo)
            with subprocess.Popen([str(PROGRAM)], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  cwd=ROOT) as process:
                assert process.stdin is not None
                assert process.stdout is not None
                try:
                    process.stdin.write(f'cat {fifo}\npwd\nexit\n'.encode())
                    process.stdin.flush()
                    deadline = time.monotonic() + 5
                    while True:
                        try:
                            writer = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
                            break
                        except OSError as error:
                            import errno
                            if error.errno != errno.ENXIO:
                                raise
                        self.assertLess(time.monotonic(), deadline)
                        time.sleep(0.01)
                    self.assertIsNone(process.poll())
                    self.assertFalse(select.select([process.stdout], [], [], 0.1)[0],
                                     'pwd ran while cat was still blocked')
                    os.write(writer, b'child-finished\n')
                    os.close(writer)
                    stdout, stderr = process.communicate(timeout=5)
                    self.assertEqual(process.returncode, 0, stderr)
                    self.assertEqual(stdout, b'child-finished\n' + str(ROOT).encode() + b'\n')
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.communicate(timeout=5)

    @unittest.skipUnless(Path('/proc/self/task').exists(), 'requires Linux procfs')
    def test_signal_killed_child_status_at_eof(self):
        import signal
        import tempfile
        import time
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            fifo = Path(temp) / 'input'
            os.mkfifo(fifo)
            with subprocess.Popen([str(PROGRAM)], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  cwd=ROOT) as process:
                assert process.stdin is not None
                child = None
                try:
                    process.stdin.write(f'cat {fifo}\n'.encode())
                    process.stdin.flush()
                    deadline = time.monotonic() + 5
                    children = Path(f'/proc/{process.pid}/task/{process.pid}/children')
                    while True:
                        ids = children.read_text().split()
                        if ids:
                            child = int(ids[0])
                            try:
                                target = Path(f'/proc/{child}/exe').resolve(strict=True)
                                if target == ROOT.parents[1] / 'build/cat':
                                    break
                            except FileNotFoundError:
                                pass
                        self.assertLess(time.monotonic(), deadline)
                        time.sleep(0.01)
                    os.kill(child, signal.SIGTERM)
                    stdout, stderr = process.communicate(timeout=5)
                    child = None  # The shell has reaped this child.
                    self.assertEqual(process.returncode, 128 + signal.SIGTERM, stderr)
                    self.assertEqual(stdout, b'')
                finally:
                    if child is not None:
                        try:
                            os.kill(child, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                    if process.poll() is None:
                        process.kill()
                        process.communicate(timeout=5)

    def test_input_allocation_failure_is_not_successful_eof(self):
        import tempfile
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            folder = Path(temp)
            wrapper = folder / 'reader_failure.c'
            wrapper.write_text('#define _POSIX_C_SOURCE 200809L\n'
                               '#include <stdio.h>\n#include <errno.h>\n#include <sys/types.h>\n'
                               'ssize_t __wrap_getline(char **p, size_t *n, FILE *f) {\n'
                               ' (void)p; (void)n; (void)f; errno = ENOMEM; return -1;\n}\n')
            program = folder / 'shell'
            subprocess.run(['cc', '-std=c17', '-Wall', '-Wextra', '-Werror',
                            f'-DCOMMAND_DIR="{folder}"',
                            *[str(ROOT / name) for name in
                              ('Unix_Shell.c', 'token.c', 'command.c', 'builtin.c')],
                            str(wrapper), '-Wl,--wrap=getline', '-o', str(program)],
                           check=True, capture_output=True)
            result = subprocess.run([str(program)], input='', text=True,
                                    capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 1)
            self.assertIn('input:', result.stderr)


if __name__ == '__main__':
    unittest.main()
