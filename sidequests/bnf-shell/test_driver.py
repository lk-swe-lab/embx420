"""Test command execution through the line-based shell driver."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


DRIVER = Path(os.environ.get("BNF_SHELL",
              Path(__file__).resolve().parent / "build" / "shell"))


class DriverTests(unittest.TestCase):
    def test_executes_external_true(self):
        self.assertTrue(DRIVER.is_file(), "Build build/shell before testing.")
        result = subprocess.run(
            [str(DRIVER)],
            input="true\n",
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertEqual(result.stderr, "")

    def test_executes_echo_without_arguments(self):
        self.assertTrue(DRIVER.is_file(), "Build build/shell before testing.")
        result = subprocess.run(
            [str(DRIVER)],
            input="echo\n",
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "\n")
        self.assertEqual(result.stderr, "")

    def test_internal_echo_without_external_program(self):
        self.assertTrue(DRIVER.is_file(), "Build build/shell before testing.")
        # No external program can be found in this empty search directory.
        with tempfile.TemporaryDirectory() as directory:
            environment = os.environ.copy()
            environment["PATH"] = directory
            result = subprocess.run(
                [str(DRIVER)],
                input="echo hello world\n",
                env=environment,
                text=True,
                capture_output=True,
                timeout=5,
                check=False,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "hello world\n")
        self.assertEqual(result.stderr, "")

    def test_executes_echo_with_arguments(self):
        self.assertTrue(DRIVER.is_file(), "Build build/shell before testing.")
        result = subprocess.run(
            [str(DRIVER)],
            input="echo hello world\n",
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "hello world\n")
        self.assertEqual(result.stderr, "")

    def test_recovers_after_invalid_line(self):
        self.assertTrue(DRIVER.is_file(), "Build build/shell before testing.")
        result = subprocess.run(
            [str(DRIVER)],
            input="echo hello |\necho world\n",
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(result.stdout, "world\n")
        self.assertIn("syntax error", result.stderr.lower())

    def test_ignores_blank_line(self):
        self.assertTrue(DRIVER.is_file(), "Build build/shell before testing.")
        result = subprocess.run(
            [str(DRIVER)],
            input="\necho hello\n",
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "hello\n")
        self.assertEqual(result.stderr, "")

    def test_executes_two_lines_in_order(self):
        self.assertTrue(DRIVER.is_file(), "Build build/shell before testing.")
        result = subprocess.run(
            [str(DRIVER)],
            input="echo hello\necho world\n",
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "hello\nworld\n")
        self.assertEqual(result.stderr, "")


if __name__ == "__main__":
    unittest.main()
