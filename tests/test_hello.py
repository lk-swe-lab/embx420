"""Check standalone hello bytes and status with a selected executable."""

import os
from pathlib import Path
import subprocess
import sys
import unittest

PROGRAM = str(Path(sys.argv.pop(1)).resolve())


def invoke(*args):
    """Run hello with a deadline and fixed locale."""
    return subprocess.run(
        [PROGRAM, *args], capture_output=True, timeout=3,
        env={**os.environ, "LC_ALL": "C"}, check=False,
    )


class HelloTests(unittest.TestCase):
    def test_default_greeting(self):
        result = invoke()
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stderr, b"")
        self.assertEqual(result.stdout, b"Hello, world!\n")


    def test_name(self):
        for args, name in [(("--name", "Ada"), "Ada"),
                           (("--name=Grace",), "Grace"),
                           (("--name", ""), ""),
                           (("--name", "-h-"), "-h-"),
                           (("--name", "A %s B"), "A %s B")]:
            with self.subTest(args=args):
                result = invoke(*args)
                self.assertEqual(result.returncode, 0)
                self.assertEqual(result.stderr, b"")
                self.assertEqual(result.stdout,
                                 f"Hello, {name}!\n".encode())


    def test_unambiguous_long_abbreviations(self):
        for option in ("--n", "--na", "--nam"):
            for args, name in (((option, "Ada"), "Ada"),
                               ((option + "=Grace",), "Grace"),
                               ((option, ""), ""),
                               ((option + "=",), "")):
                with self.subTest(args=args):
                    result = invoke(*args)
                    self.assertEqual(result.returncode, 0)
                    self.assertEqual(result.stderr, b"")
                    self.assertEqual(result.stdout,
                                     f"Hello, {name}!\n".encode())
        expected = invoke("--help")
        for option in ("--h", "--he", "--hel"):
            with self.subTest(option=option):
                result = invoke(option)
                self.assertEqual(result.returncode, 0)
                self.assertEqual(result.stderr, b"")
                self.assertEqual(result.stdout, expected.stdout)

    def test_abbreviated_name_hyphen_values(self):
        for option in ("--n", "--na", "--nam"):
            with self.subTest(option=option):
                result = invoke(option, "-h-")
                self.assertEqual(result.returncode, 0)
                self.assertEqual(result.stderr, b"")
                self.assertEqual(result.stdout, b"Hello, -h-!\n")

    def test_abbreviation_errors(self):
        for args in (("--n",), ("--nam=A", "--name=B"),
                     ("--h", "--hel"), ("--he=value",),
                     ("--h", "--n"), ("--h", "extra"),
                     ("--n=", "--unknown"), ("--", "--hel"),
                     ("--names=A",), ("--helps",)):
            with self.subTest(args=args):
                result = invoke(*args)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
                self.assertTrue(result.stderr)

    def test_empty_name_forms(self):
        for args in (("--name", ""), ("--name=",)):
            with self.subTest(args=args):
                result = invoke(*args)
                self.assertEqual(result.returncode, 0, "HELLO_EMPTY_NAME")
                self.assertEqual(result.stdout, b"Hello, !\n")
                self.assertEqual(result.stderr, b"")


    def test_empty_name_does_not_hide_errors(self):
        for args in (("--name=", "--name"),
                     ("--name=", "--name=Ada"),
                     ("--name=Ada", "--name="),
                     ("--name=", "--unknown"),
                     ("--name=", "extra"),
                     ("--name=", "--name=", "--help")):
            with self.subTest(args=args):
                result = invoke(*args)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
                self.assertTrue(result.stderr)


    def test_help(self):
        for option in ("-h", "--help"):
            with self.subTest(option=option):
                result = invoke(option)
                self.assertEqual(result.returncode, 0, "HELLO_HELP_STATUS")
                self.assertEqual(result.stderr, b"", "HELLO_HELP_STDERR")
                self.assertIn(b"Usage: hello", result.stdout)
                self.assertIn(b"--name", result.stdout, "HELLO_HELP_NAME")
                self.assertIn(b"NAME", result.stdout)
                self.assertIn(b"-h", result.stdout, "HELLO_HELP_SHORT")
                self.assertIn(b"--help", result.stdout, "HELLO_HELP_LONG")
                self.assertIn(b"Set the greeting name.", result.stdout)
                self.assertIn(b"world", result.stdout, "HELLO_HELP_DEFAULT")
                self.assertNotIn(b"Hello,", result.stdout)
                self.assertTrue(result.stdout.endswith(b"\n"))

    def test_invalid_arguments(self):
        for args in (("--name",), ("--unknown",), ("extra",),
                     ("--name=A", "--name=B"), ("-h", "-h"),
                     ("--help=value",), ("--", "--help"), ("-",),
                     ("-h-",), ("-h--",), ("-h-\x01",)):
            with self.subTest(args=args):
                result = invoke(*args)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
                self.assertTrue(result.stderr)

    def test_output_failure(self):
        if not Path("/dev/full").exists():
            self.skipTest("/dev/full is not available")
        for args in ((), ("--help",)):
            with self.subTest(args=args), open("/dev/full", "wb") as out:
                result = subprocess.run(
                    [PROGRAM, *args], stdout=out, stderr=subprocess.PIPE,
                    timeout=3, check=False,
                )
                self.assertEqual(result.returncode, 1)
                self.assertTrue(result.stderr)


    def test_help_with_name(self):
        result = invoke("--name", "Ada", "--help")
        help_result = invoke("--help")
        self.assertEqual(result.returncode, 0, "HELLO_HELP_WITH_NAME")
        self.assertEqual(result.stderr, b"")
        self.assertEqual(result.stdout, help_result.stdout)
        self.assertNotIn(b"Hello, Ada!", result.stdout)


    def test_errors_take_priority_over_help(self):
        cases = [
            ("--help", "--unknown"),
            ("--unknown", "--help"),
            ("--help", "--name"),
            ("--help", "--name=Ada", "--name=Grace"),
            ("--help", "extra"),
        ]
        for args in cases:
            with self.subTest(args=args):
                result = invoke(*args)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"", "HELLO_ERROR_STDOUT")
                self.assertTrue(result.stderr, "HELLO_ERROR_STDERR")
                self.assertNotIn(b"Usage:", result.stderr)


if __name__ == "__main__":
    unittest.main()
