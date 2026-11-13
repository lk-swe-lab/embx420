#!/usr/bin/env python3
"""Run a checked demo in a temporary directory, not in the user's files."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
SHELL = ROOT / 'build/shell'
PARSER = ROOT / 'build/TestGrammar'


def run_case(title, source, expected, directory):
    print(f'\n== {title} ==', flush=True)
    print(source.rstrip(), flush=True)
    result = subprocess.run([str(SHELL)], input=source, text=True,
                            capture_output=True, cwd=directory, timeout=8)
    if result.returncode or result.stderr or result.stdout != expected:
        raise RuntimeError(f'Unexpected result: {result!r}')
    print('Output:', flush=True)
    print(result.stdout, end='', flush=True)


def main():
    if not SHELL.is_file() or not PARSER.is_file():
        raise SystemExit('Run make before this demo.')
    with tempfile.TemporaryDirectory(prefix='bnf-shell-demo-') as directory:
        Path(directory, 'input.txt').write_text('red\nblue\n')
        run_case('Threads, processes, and system pipes',
                 'hello\necho hello world | cat | wc -w\n',
                 'Hello from bnf-shell\n2\n', directory)
        run_case('Files and a foreground sequence',
                 'cat < input.txt > output.txt ; cat < output.txt\n',
                 'red\nblue\n', directory)
        assert Path(directory, 'output.txt').read_text() == 'red\nblue\n'
        run_case('Local state and exported child environment',
                 'BNF_DEMO_VALUE=5 ; echo $BNF_DEMO_VALUE\n'
                 'export BNF_DEMO_VALUE\nprintenv BNF_DEMO_VALUE\n',
                 '5\n5\n', directory)
        run_case('Our evaluator captures a backquote pipeline',
                 'echo count `cat input.txt | wc -l`\n',
                 'count 2\n', directory)
        run_case('A condition uses the command exit status',
                 'if true ; then echo yes ; fi\n'
                 'if false ; then echo skipped ; fi\n',
                 'yes\n', directory)
        run_case('A background job does not delay the next command',
                 'sleep .1 & echo foreground\n',
                 'foreground\n', directory)
        run_case('Main-thread directory and exit commands',
                 f'cd {directory}\npwd\nexit 0 ; echo skipped\n',
                 directory + '\n', directory)
        source = 'x=5 ; echo $x | wc -c\n'
        result = subprocess.run([str(PARSER)], input=source, text=True,
                                capture_output=True, timeout=5, check=True)
        print('\n== Generated AST ==\n' + source.rstrip(), flush=True)
        print(result.stdout, flush=True)
    print('Demo passed. Temporary fixtures were removed.', flush=True)


if __name__ == '__main__':
    main()
