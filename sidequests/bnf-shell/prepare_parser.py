"""Add Bison cleanup to the BNFC 2.9.6.1 C parser.

BNFC does not emit error-path destructors. An untyped start wrapper transfers
ownership to the caller on success. Typed nodes are freed on parser errors.
No accepted shell syntax or public parser entry point changes.
"""
from pathlib import Path
import re


def prepare(path):
    text = path.read_text()
    if '%start ShellStart' in text:
        return
    if text.count('%start Input') != 1:
        raise ValueError('Expected Input as the sole BNFC entry point')
    types = re.findall(r'^%type\s+<(\w+)>\s+(\w+)\s*$', text, re.M)
    if not types or ('input_', 'Input') not in types:
        raise ValueError('Expected typed BNFC AST categories')
    destructors = ['%destructor { free($$); } <_string>']
    destructors += [f'%destructor {{ free_{category}($$); }} <{field}>'
                    for field, category in types]
    text = text.replace('%pure_parser', '%define api.pure full', 1)
    text = text.replace('%start Input', '\n'.join(destructors) +
                        '\n\n%start ShellStart', 1)
    text = text.replace('%%',
                        '%%\n\nShellStart : Input '
                        '{ result->input_ = $1; } ;', 1)
    path.write_text(text)


if __name__ == '__main__':
    prepare(Path('build/Grammar.y'))
