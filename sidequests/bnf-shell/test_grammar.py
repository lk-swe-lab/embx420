"""Test the generated shell grammar without command execution."""

from pathlib import Path
import subprocess
import unittest


PARSER = Path(__file__).resolve().parent / "build" / "TestGrammar"


class GrammarTests(unittest.TestCase):
    def parse(self, text):
        self.assertTrue(PARSER.is_file(), "Build build/TestGrammar before testing.")
        return subprocess.run(
            [str(PARSER)],
            input=text,
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )

    def test_command_with_arguments(self):
        result = self.parse("echo hello world\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Single (Cmd "echo" [(Literal "hello"), (Literal "world")] '
            'NoRedir))', result.stdout
        )

    def test_wide_words_and_assignment_sequence(self):
        result = self.parse('x=5 ; cat ./input-file_1.txt | wc -l\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('(ThenAssign "x" "5"', result.stdout)
        self.assertIn('(Literal "./input-file_1.txt")', result.stdout)
        self.assertIn('(Literal "-l")', result.stdout)

    def test_conditional_ast(self):
        result = self.parse('if true ; then echo yes ; fi\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('(If (Single (Cmd "true" [] NoRedir))', result.stdout)
        self.assertIn('(Single (Cmd "echo" [(Literal "yes")] NoRedir))',
                      result.stdout)

    def test_rejects_incomplete_conditional(self):
        for text in ('if true ; then echo yes\n',
                     'if ; then echo yes ; fi\n',
                     'if true ; then ; fi\n',
                     'if true then echo yes fi\n'):
            with self.subTest(text=text):
                self.assertNotEqual(self.parse(text).returncode, 0)

    def test_rejects_unsupported_quotes_and_operators(self):
        for text in ('echo "hello world"\n', "echo 'hello'\n",
                     'true && echo yes\n', 'echo *.txt\n'):
            with self.subTest(text=text):
                self.assertNotEqual(self.parse(text).returncode, 0)

    def test_command_without_arguments(self):
        result = self.parse("echo\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('(Single (Cmd "echo" [] NoRedir))', result.stdout)

    def test_parses_two_stage_pipeline(self):
        result = self.parse("echo hello | wc\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Pipe (Cmd "echo" [(Literal "hello")] NoRedir) '
            '(Single (Cmd "wc" [] NoRedir)))',
            result.stdout,
        )

    def test_parses_right_recursive_pipeline(self):
        result = self.parse("echo hello | cat | wc\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Pipe (Cmd "echo" [(Literal "hello")] NoRedir) '
            '(Pipe (Cmd "cat" [] NoRedir) '
            '(Single (Cmd "wc" [] NoRedir))))',
            result.stdout,
        )

    def test_parses_input_redirection(self):
        result = self.parse("cat < input\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Single (Cmd "cat" [] (InRedir "input")))', result.stdout
        )

    def test_parses_output_redirection(self):
        result = self.parse("echo hello > output\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Single (Cmd "echo" [(Literal "hello")] (OutRedir "output")))',
            result.stdout,
        )

    def test_parses_combined_redirection(self):
        cases = [
            ("cat < input > output\n", '(InOutRedir "input" "output")'),
            ("cat > output < input\n", '(OutInRedir "output" "input")'),
        ]
        for text, redirection in cases:
            with self.subTest(text=text):
                result = self.parse(text)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(
                    f'(Single (Cmd "cat" [] {redirection}))', result.stdout
                )

    def test_attaches_redirection_to_pipeline_commands(self):
        result = self.parse("cat < input | wc > output\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Pipe (Cmd "cat" [] (InRedir "input")) '
            '(Single (Cmd "wc" [] (OutRedir "output"))))',
            result.stdout,
        )

    def test_parses_sequential_jobs(self):
        result = self.parse("echo hello ; echo world\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        first = '(Single (Cmd "echo" [(Literal "hello")] NoRedir))'
        second = '(Single (Cmd "echo" [(Literal "world")] NoRedir))'
        self.assertIn(first, result.stdout)
        self.assertIn(second, result.stdout)
        self.assertLess(result.stdout.index(first), result.stdout.index(second))

    def test_parses_final_background_job(self):
        result = self.parse("echo hello &\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Single (Cmd "echo" [(Literal "hello")] NoRedir))', result.stdout
        )

    def test_parses_background_job_before_next_job(self):
        result = self.parse("echo hello & echo world\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        first = '(Single (Cmd "echo" [(Literal "hello")] NoRedir))'
        second = '(Single (Cmd "echo" [(Literal "world")] NoRedir))'
        self.assertIn(first, result.stdout)
        self.assertIn(second, result.stdout)
        self.assertLess(result.stdout.index(first), result.stdout.index(second))

    def test_parses_variable_assignment(self):
        result = self.parse("x=hello\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('(Assign "x" "hello")', result.stdout)

    def test_distinguishes_literal_and_variable_arguments(self):
        result = self.parse("echo x $x\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Cmd "echo" [(Literal "x"), (Variable "x")] NoRedir)',
            result.stdout,
        )

    def test_parses_backquote_subcommand(self):
        result = self.parse("echo `echo hello`\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Cmd "echo" [(Subcommand '
            '(InnerSingle (InnerCmd "echo" [(InnerLiteral "hello")] '
            'NoRedir)))] NoRedir)',
            result.stdout,
        )

    def test_parses_pipeline_inside_backquotes(self):
        result = self.parse("echo `echo hello | wc`\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Cmd "echo" [(Subcommand '
            '(InnerPipe (InnerCmd "echo" [(InnerLiteral "hello")] NoRedir) '
            '(InnerSingle (InnerCmd "wc" [] NoRedir))))] NoRedir)',
            result.stdout,
        )

    def test_parses_variable_inside_backquotes(self):
        result = self.parse("echo `echo $x`\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            '(Subcommand (InnerSingle '
            '(InnerCmd "echo" [(InnerVariable "x")] NoRedir)))',
            result.stdout,
        )

    def test_rejects_nested_backquotes(self):
        result = self.parse("echo `echo `echo hello``\n")
        self.assertGreater(result.returncode, 0, result.stderr)
        self.assertIn("syntax error", (result.stdout + result.stderr).lower())

    def test_rejects_unclosed_backquote(self):
        result = self.parse("echo `echo hello\n")
        self.assertGreater(result.returncode, 0, result.stderr)
        self.assertIn("syntax error", (result.stdout + result.stderr).lower())

    def test_rejects_empty_backquotes(self):
        result = self.parse("echo ``\n")
        self.assertGreater(result.returncode, 0, result.stderr)
        self.assertIn("syntax error", (result.stdout + result.stderr).lower())

    def test_rejects_missing_assignment_name_or_value(self):
        for text in ("=hello\n", "x=\n"):
            with self.subTest(text=text):
                result = self.parse(text)
                self.assertGreater(result.returncode, 0, result.stderr)
                self.assertIn(
                    "syntax error", (result.stdout + result.stderr).lower()
                )

    def test_rejects_missing_variable_name(self):
        result = self.parse("echo $\n")
        self.assertGreater(result.returncode, 0, result.stderr)
        self.assertIn("syntax error", (result.stdout + result.stderr).lower())

    def test_rejects_missing_job(self):
        for text in (
            "; echo hello\n",
            "& echo hello\n",
            "echo hello & & echo world\n",
            "echo hello ; ; echo world\n",
        ):
            with self.subTest(text=text):
                result = self.parse(text)
                self.assertGreater(result.returncode, 0, result.stderr)
                self.assertIn(
                    "syntax error", (result.stdout + result.stderr).lower()
                )

    def test_rejects_missing_redirection_file(self):
        for text in ("cat <\n", "echo hello >\n", "cat < input >\n"):
            with self.subTest(text=text):
                result = self.parse(text)
                self.assertGreater(result.returncode, 0, result.stderr)
                self.assertIn(
                    "syntax error", (result.stdout + result.stderr).lower()
                )

    def test_rejects_repeated_redirection(self):
        for text in ("cat < input < other\n", "echo > output > other\n"):
            with self.subTest(text=text):
                result = self.parse(text)
                self.assertGreater(result.returncode, 0, result.stderr)
                self.assertIn(
                    "syntax error", (result.stdout + result.stderr).lower()
                )

    def test_rejects_missing_pipeline_stage(self):
        for text in ("| wc\n", "echo hello |\n", "echo || wc\n"):
            with self.subTest(text=text):
                result = self.parse(text)
                self.assertGreater(result.returncode, 0, result.stderr)
                self.assertIn(
                    "syntax error", (result.stdout + result.stderr).lower()
                )


if __name__ == "__main__":
    unittest.main()
