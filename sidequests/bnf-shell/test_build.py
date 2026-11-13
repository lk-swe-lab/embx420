"""Check the parser adapter and learning diagram against generated sources."""
from pathlib import Path
import re
import tempfile
import unittest

from prepare_parser import prepare

ROOT = Path(__file__).resolve().parent


class BuildContractTests(unittest.TestCase):
    def test_parser_adapter_is_idempotent(self):
        source = (ROOT / 'build/Grammar.y').read_text()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, 'Grammar.y')
            path.write_text(source)
            prepare(path)
            self.assertEqual(path.read_text(), source)
        self.assertIn('%destructor { free_Input($$); } <input_>', source)
        self.assertIn('%start ShellStart', source)

    def test_parser_adapter_rejects_unknown_entry_point(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, 'Grammar.y')
            path.write_text('%start Unknown\n%%\n')
            with self.assertRaises(ValueError):
                prepare(path)

    def test_diagram_names_every_actual_constructor(self):
        grammar = (ROOT / 'Grammar.cf').read_text()
        diagram = (ROOT / 'GRAMMAR.md').read_text()
        constructors = re.findall(r'^(\w+)\.\s+\w+\s*::=', grammar, re.M)
        self.assertTrue(constructors)
        for name in constructors:
            with self.subTest(constructor=name):
                self.assertRegex(diagram, rf'\b{name}\b')
        categories = re.findall(r'^\w+\.\s+(\w+)\s*::=', grammar, re.M)
        for name in set(categories):
            self.assertIn(name, diagram)
        self.assertIn('entrypoints Input ;', grammar)
        self.assertEqual(diagram.count('```mermaid'), 3)

    def test_handwritten_c_line_width(self):
        for number, line in enumerate((ROOT / 'shell.c').read_text().splitlines(),
                                      1):
            self.assertLessEqual(len(line), 80, f'shell.c:{number}')
