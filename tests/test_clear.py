#!/usr/bin/env python3
"""Byte-exact clear checks; all terminals belong to subprocesses."""
import argparse
import errno
import os
import pty
import subprocess
import unittest

PROGRAM: str = ""
MAIN: str | None = None


def invoke(args=(), term: str | None = "xterm", terminal=False,
           closed=False, main=False, script: bytes | None = None):
    env = dict(os.environ, LC_ALL="C")
    if term is None:
        env.pop("TERM", None)
    else:
        env["TERM"] = term
    if main:
        assert MAIN is not None
        command = [MAIN] if script is not None else [MAIN, "clear", *args]
    else:
        command = [PROGRAM, *args]
    if terminal:
        master, slave = pty.openpty()
        try:
            child = subprocess.Popen(command, stdin=(subprocess.PIPE if script
                                                       else subprocess.DEVNULL),
                                     stdout=slave, stderr=subprocess.PIPE,
                                     env=env)
            os.close(slave)
            slave = -1
            _, err = child.communicate(input=script, timeout=5)
            output = bytearray()
            while True:
                try:
                    chunk = os.read(master, 4096)
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    raise
                if not chunk:
                    break
                output.extend(chunk)
            return child.returncode, bytes(output), err
        finally:
            os.close(master)
            if slave >= 0:
                os.close(slave)
    result = subprocess.run(command, input=b"untouched input", env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=5,
                            preexec_fn=(lambda: os.close(1)) if closed else None)
    return result.returncode, result.stdout, result.stderr


class ClearTests(unittest.TestCase):
    def test_terminal_bytes(self):
        self.assertEqual(invoke(terminal=True), (0, b"\x1b[H\x1b[2J", b""))
        self.assertEqual(invoke(["--"], terminal=True),
                         (0, b"\x1b[H\x1b[2J", b""))

    def test_nonterminal(self):
        for term in (None, "", "dumb", "xterm", "unknown-terminal"):
            with self.subTest(term=term):
                self.assertEqual(invoke(term=term), (0, b"", b""))

    def test_invalid_before_effects(self):
        for args in (["file"], [""], ["-"], ["--", "file"], ["-x"],
                     ["--json"], ["--help", "file"], ["--help", "--bad"],
                     ["-h", "--help"], ["-hh"], ["--help=yes"]):
            with self.subTest(args=args):
                status, out, err = invoke(args, terminal=True)
                self.assertEqual(status, 2)
                self.assertEqual(out, b"")
                self.assertTrue(err)
                self.assertNotIn(b"\x1b", err)

    def test_term_guard(self):
        for term in (None, "", "dumb"):
            with self.subTest(term=term):
                status, out, err = invoke(term=term, terminal=True)
                self.assertEqual(status, 1)
                self.assertEqual(out, b"")
                self.assertTrue(err)
        self.assertEqual(invoke(term="unknown-terminal", terminal=True),
                         (0, b"\x1b[H\x1b[2J", b""))

    def test_closed_stdout(self):
        status, out, err = invoke(closed=True)
        self.assertEqual(status, 1)
        self.assertEqual(out, b"")
        self.assertTrue(err)

    def test_help_output_failure(self):
        with open("/dev/full", "wb") as output:
            result = subprocess.run([PROGRAM, "--help"],
                                    stdin=subprocess.DEVNULL, stdout=output,
                                    stderr=subprocess.PIPE, timeout=5)
        self.assertEqual(result.returncode, 1)
        self.assertTrue(result.stderr)
        self.assertEqual(invoke(["--help"], closed=True)[0], 1)

    def test_main_parity(self):
        if MAIN is None:
            self.skipTest("scratch main not supplied")
        cases = [((), "xterm", True), (("--",), "xterm", True),
                 ((), None, True), ((), "dumb", False),
                 (("--help",), None, False), (("-h",), "dumb", True),
                 (("operand",), "xterm", True),
                 (("--help", "--bad"), "xterm", True)]
        for args, term, terminal in cases:
            with self.subTest(args=args, term=term, terminal=terminal):
                self.assertEqual(invoke(args, term, terminal, main=True),
                                 invoke(args, term, terminal))
        listing = subprocess.run([MAIN, "help"], stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, timeout=5)
        self.assertEqual(listing.returncode, 0)
        self.assertIn(b"clear", listing.stdout)
        help_result = subprocess.run([MAIN, "help", "clear"],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, timeout=5)
        self.assertEqual((help_result.returncode, help_result.stdout,
                          help_result.stderr), invoke(["--help"]))
        repeated = subprocess.run([MAIN],
                                  input=b"clear\nclear --bad\nclear --help\nclear\n",
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  timeout=5)
        self.assertEqual(repeated.returncode, 0)
        self.assertEqual(repeated.stdout, invoke(["--help"])[1])
        self.assertTrue(repeated.stderr)
        status, output, errors = invoke(main=True, terminal=True,
                                        script=b"clear\nclear --bad\n"
                                               b"clear --help\nclear\n")
        self.assertEqual(status, 0)
        self.assertEqual(output, b"\x1b[H\x1b[2J" +
                         invoke(["--help"])[1].replace(b"\n", b"\r\n") +
                         b"\x1b[H\x1b[2J")
        self.assertTrue(errors)
        self.assertEqual(invoke(closed=True, main=True), invoke(closed=True))

    def test_help(self):
        status, out, err = invoke(["--help"], term=None)
        self.assertEqual(status, 0)
        self.assertTrue(out.startswith(b"Usage: clear"), repr(out))
        self.assertIn(b"--help", out)
        self.assertNotIn(b"\x1b", out)
        self.assertEqual(err, b"")
        self.assertEqual(invoke(["-h"], term="dumb"), (status, out, err))
        for term in (None, "", "dumb", "unknown-terminal"):
            result = invoke(["--help"], term=term, terminal=True)
            self.assertEqual(result, (0, out.replace(b"\n", b"\r\n"), b""))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--program", required=True)
    parser.add_argument("--main")
    args, extra = parser.parse_known_args()
    PROGRAM = os.path.abspath(args.program)
    MAIN = os.path.abspath(args.main) if args.main else None
    unittest.main(argv=[__file__, *extra])
