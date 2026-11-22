"""Test the shell through its real input loop."""
import os
from pathlib import Path
import pty
import random
import shutil
import signal
import select
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
SHELL = Path(os.environ.get("LEAN_SHELL_UNDER_TEST",
                            str(ROOT / "build/lean-shell"))).resolve()
FAULT_SHELL = Path(os.environ.get(
    "LEAN_FAULT_SHELL_UNDER_TEST", str(ROOT / "build/lean-shell-fault"))).resolve()


class ShellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.release = tempfile.TemporaryDirectory(prefix="lean-release-")
        cls.addClassCleanup(cls.release.cleanup)
        folder = Path(cls.release.name)
        cls.shell = folder / "lean-shell"
        shutil.copy2(SHELL, cls.shell)
        cls.fault_shell = folder / "lean-shell-fault"
        shutil.copy2(FAULT_SHELL, cls.fault_shell)
        shutil.copytree(ROOT / "build/modules", folder / "modules")
        shutil.copy2(ROOT / "build/probe.so", folder / "modules/probe.so")

    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix="lean-shell-")
        self.addCleanup(self.scratch.cleanup)
        self.home = Path(self.scratch.name)
        self.env = dict(os.environ, HOME=str(self.home), LC_ALL="C")

    def run_shell(self, text):
        return subprocess.run(
            [str(self.shell)], input=text, capture_output=True,
            cwd=self.home, env=self.env, timeout=8,
        )

    def test_arguments_quotes_and_final_line(self):
        (self.home / "a b").write_bytes(b"one\ntwo\n")
        result = self.run_shell(b"head -n 1 'a b'")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, b"one\n")

    def test_blank_and_bad_syntax(self):
        for text in (b"cat 'bad\n", b"pwd > out\n", b"pwd\\\n"):
            with self.subTest(text=text):
                result = self.run_shell(text)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
        self.assertEqual(self.run_shell(b" \n\n").returncode, 0)

    def test_two_and_three_stages(self):
        (self.home / "data").write_bytes(b"one\ntwo\nthree\n")
        for text in (b"cat data | head -n 2\n",
                     b"cat data|cat|head -n 2\n"):
            with self.subTest(text=text):
                result = self.run_shell(text)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, b"one\ntwo\n")

    def test_large_stream_and_early_reader_exit(self):
        data = b"line of data\n" * 100000
        (self.home / "large").write_bytes(data)
        result = self.run_shell(b"cat large | cat | cat\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, data)
        result = self.run_shell(b"cat large | head -n 1\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, b"line of data\n")

    def test_pipeline_validation_before_side_effects(self):
        for text in (b"touch forbidden |\n", b"touch forbidden || pwd\n",
                     b"touch forbidden | cd /\n", b"|pwd\n"):
            with self.subTest(text=text):
                result = self.run_shell(text)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertFalse((self.home / "forbidden").exists())

    def test_last_stage_status(self):
        result = self.run_shell(b"pwd | absent\n")
        self.assertEqual(result.returncode, 127, result.stderr)
        result = self.run_shell(b"absent | cat\n")
        self.assertEqual(result.returncode, 0, result.stderr)

    def start_shell(self, executable=None, env=None):
        process = subprocess.Popen(
            [str(executable or self.shell)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=self.home,
            env=env or self.env,
            start_new_session=True, bufsize=0,
        )
        def cleanup():
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.communicate(timeout=5)
        self.addCleanup(cleanup)
        return process

    def wait_for_children(self, process):
        path = Path(f"/proc/{process.pid}/task/{process.pid}/children")
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if process.poll() is not None:
                self.fail("Shell stopped before its child started.")
            children = path.read_text().split()
            if children:
                return children
            time.sleep(0.01)
        self.fail("No child started.")

    def test_interrupt_keeps_shell_alive(self):
        process = self.start_shell()
        assert process.stdin is not None
        process.stdin.write(b"probe wait | probe wait\n")
        process.stdin.flush()
        self.wait_for_children(process)
        # Wait until both stages exist before sending a group interrupt.
        deadline = time.monotonic() + 3
        while len(self.wait_for_children(process)) != 2:
            self.assertLess(time.monotonic(), deadline)
        os.killpg(process.pid, signal.SIGINT)
        stdout, stderr = process.communicate(b"exit\n", timeout=5)
        self.assertEqual(process.returncode, 130, stderr)
        self.assertEqual(stdout, b"")

    def test_parent_state_and_exit(self):
        (self.home / "sub").mkdir()
        result = self.run_shell(b"cd sub\npwd\nabsent\nexit\npwd\n")
        self.assertEqual(result.returncode, 127, result.stderr)
        self.assertEqual(result.stdout, os.fsencode(self.home / "sub") + b"\n")

    def construction_signal(self, number):
        process = self.start_shell(
            self.fault_shell, dict(self.env, LEAN_TEST_STOP_FORK="1"))
        assert process.stdin is not None
        process.stdin.write(b"|".join([b"probe wait"] * 32) + b"\n")
        process.stdin.flush()
        deadline = time.monotonic() + 3
        while True:
            got, status = os.waitpid(process.pid, os.WUNTRACED | os.WNOHANG)
            if got:
                self.assertTrue(os.WIFSTOPPED(status))
                self.assertEqual(os.WSTOPSIG(status), signal.SIGSTOP)
                break
            self.assertLess(time.monotonic(), deadline)
            time.sleep(0.001)
        self.assertEqual(len(self.wait_for_children(process)), 1)
        os.killpg(process.pid, number)
        os.kill(process.pid, signal.SIGCONT)
        try:
            stdout, stderr = process.communicate(b"exit\npwd\n", timeout=5)
        except subprocess.TimeoutExpired:
            self.fail(f"Construction-time {number.name} left stages running.")
        self.assertEqual(process.returncode, 128 + number, stderr)
        self.assertEqual(stdout, b"")

    def test_construction_sigint(self):
        self.construction_signal(signal.SIGINT)

    def test_construction_sigquit(self):
        self.construction_signal(signal.SIGQUIT)

    def test_child_identity_and_buffer_flush(self):
        process = self.start_shell()
        stdout, stderr = process.communicate(b"probe pid\nprobe buffer | cat\n")
        self.assertEqual(process.returncode, 0, stderr)
        identity, output = stdout.splitlines()
        child, parent = map(int, identity.split())
        self.assertEqual(parent, process.pid)
        self.assertNotEqual(child, parent)
        self.assertEqual(output, b"buffered")

    def test_signal_status(self):
        self.assertEqual(self.run_shell(b"probe signal\n").returncode, 143)

    def test_limits_and_literal_operators(self):
        (self.home / "a|b").write_bytes(b"literal\n")
        result = self.run_shell(b"cat 'a|b'\ncat a\\|b\n")
        self.assertEqual(result.stdout, b"literal\nliteral\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        for text in (b"pwd\0bad\n", b"x" * 65537 + b"\n",
                     b"|".join([b"pwd"] * 33) + b"\n",
                     b"pwd " + b"x " * 256 + b"\n"):
            with self.subTest(size=len(text)):
                result = self.run_shell(text)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, b"")
        result = self.run_shell(b"pwd" + b"|cat" * 31 + b"\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, os.fsencode(self.home) + b"\n")

    def test_no_descriptor_leak_or_uncollected_children(self):
        process = self.start_shell()
        input_stream, output_stream = process.stdin, process.stdout
        assert input_stream is not None and output_stream is not None
        children = Path(f"/proc/{process.pid}/task/{process.pid}/children")
        def barrier():
            input_stream.write(b"probe pid\n")
            input_stream.flush()
            self.assertTrue(select.select([output_stream], [], [], 5)[0])
            self.assertTrue(output_stream.readline())
            deadline = time.monotonic() + 3
            while children.read_text().strip():
                self.assertLess(time.monotonic(), deadline)
                time.sleep(0.001)
        barrier()
        before = len(list(Path(f"/proc/{process.pid}/fd").iterdir()))
        input_stream.write(b"probe buffer | cat | cat\n" * 100)
        input_stream.flush()
        for _ in range(100):
            self.assertTrue(select.select([output_stream], [], [], 5)[0])
            self.assertEqual(output_stream.readline(), b"buffered\n")
        barrier()
        after = len(list(Path(f"/proc/{process.pid}/fd").iterdir()))
        self.assertEqual(after, before)
        self.assertEqual(children.read_text().strip(), "")
        process.communicate(b"exit\n", timeout=5)
        self.assertEqual(process.returncode, 0)

    def test_setup_faults_and_wait_interrupt(self):
        # The baseline probe uses one fork, but no pipe or dup2 calls.
        for fault in ("pipe:1", "pipe:2", "fork:2", "fork:3"):
            with self.subTest(fault=fault):
                process = self.start_shell(
                    self.fault_shell, dict(self.env, LEAN_TEST_FAIL=fault))
                assert process.stdin is not None
                assert process.stdout is not None
                assert process.stderr is not None
                children = Path(
                    f"/proc/{process.pid}/task/{process.pid}/children")
                descriptors = Path(f"/proc/{process.pid}/fd")
                def wait_empty():
                    deadline = time.monotonic() + 3
                    while children.read_text().strip():
                        self.assertLess(time.monotonic(), deadline)
                        time.sleep(0.001)
                process.stdin.write(b"probe pid\n")
                self.assertTrue(select.select([process.stdout], [], [], 5)[0])
                self.assertTrue(process.stdout.readline())
                wait_empty()
                before = len(list(descriptors.iterdir()))
                process.stdin.write(b"probe wait|cat|cat\n")
                self.assertTrue(select.select([process.stderr], [], [], 5)[0])
                self.assertIn(b"lean: pipeline:", process.stderr.readline())
                wait_empty()
                self.assertIsNone(process.poll())
                self.assertEqual(len(list(descriptors.iterdir())), before)
                stdout, stderr = process.communicate(b"exit\npwd\n", timeout=5)
                self.assertEqual(process.returncode, 1, stderr)
                self.assertEqual(stdout, b"")
        result = subprocess.run(
            [str(self.fault_shell)], input=b"pwd|cat\n", capture_output=True,
            cwd=self.home, env=dict(self.env, LEAN_TEST_FAIL="waitpid:1"),
            timeout=5,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, os.fsencode(self.home) + b"\n")

    def test_dup2_fault_uses_last_stage_status(self):
        for fault, expected in (("dup2:1", 1), ("dup2:2", 0)):
            with self.subTest(fault=fault):
                result = subprocess.run(
                    [str(self.fault_shell)], input=b"probe buffer|cat|cat\n",
                    capture_output=True, cwd=self.home,
                    env=dict(self.env, LEAN_TEST_FAIL=fault), timeout=5,
                )
                self.assertEqual(result.returncode, expected, result.stderr)
                self.assertIn(b"lean: dup2:", result.stderr)

    def test_package_live_install_remove_and_select(self):
        package = ROOT.parents[1] / "build/compat-packages-abi-v1/hello-0.2.0"
        text = (f"pkg install '{package}'\nhello --name learner | cat\n"
                "pkg remove hello\nhello\npkg use hello 0.2.0\n"
                "hello --name restored | cat\n").encode()
        result = self.run_shell(text)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"learner", result.stdout)
        self.assertIn(b"restored", result.stdout)
        self.assertIn(b"hello: unknown command", result.stderr)
        selected = self.home / ".mysh/pkgs/.active/hello"
        self.assertEqual(selected.read_bytes(), b"0.2.0")

    def test_shared_input_has_no_shell_read_ahead(self):
        result = self.run_shell(b"head -n 1 | cat\ndata line\npwd\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout,
                         b"data line\n" + os.fsencode(self.home) + b"\n")

    def test_pipeline_rejects_every_parent_only_command(self):
        for command in (b"cd /", b"exit", b"pkg list"):
            result = self.run_shell(command + b" | cat\n")
            self.assertEqual(result.returncode, 2, result.stderr)
        result = self.run_shell(b"/usr/bin/pwd\n")
        self.assertEqual(result.returncode, 127)

    def test_terminal_interrupt_and_prompt(self):
        pid, master = pty.fork()
        if pid == 0:
            os.chdir(self.home)
            os.execve(self.shell, [str(self.shell)], self.env)
        collected = False
        def until_prompt():
            output = b""
            deadline = time.monotonic() + 5
            while b"lean> " not in output:
                self.assertLess(time.monotonic(), deadline)
                if select.select([master], [], [], 0.1)[0]:
                    output += os.read(master, 4096)
            return output
        try:
            until_prompt()
            os.write(master, b"probe wait | probe wait\n")
            children = Path(f"/proc/{pid}/task/{pid}/children")
            deadline = time.monotonic() + 3
            while len(children.read_text().split()) != 2:
                self.assertLess(time.monotonic(), deadline)
                time.sleep(0.01)
            os.write(master, b"\x03")
            until_prompt()
            os.write(master, b"pwd\n")
            self.assertIn(os.fsencode(self.home), until_prompt())
            os.write(master, b"exit\n")
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                got, status = os.waitpid(pid, os.WNOHANG)
                if got:
                    collected = True
                    self.assertEqual(os.waitstatus_to_exitcode(status), 0)
                    break
                time.sleep(0.01)
            self.assertTrue(collected)
        finally:
            if not collected:
                os.killpg(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            os.close(master)

    def test_generated_parser_inputs(self):
        rng = random.Random(420)
        alphabet = b"abq XYZ|'\"\\ &;<>\t"
        lines = [bytes(rng.choice(alphabet) for _ in range(60))
                 for _ in range(200)]
        result = self.run_shell(b"\n".join(lines) + b"\npwd\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, os.fsencode(self.home) + b"\n")

    def test_closed_standard_descriptor(self):
        result = subprocess.run(
            [str(self.shell)], input=b"pwd\n", capture_output=True,
            cwd=self.home, env=self.env, preexec_fn=lambda: os.close(1),
            timeout=5,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"lean: startup", result.stderr)

    def test_single_command(self):
        result = self.run_shell(b"pwd\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, os.fsencode(self.home) + b"\n")
        self.assertEqual(result.stderr, b"")


if __name__ == "__main__":
    unittest.main()
