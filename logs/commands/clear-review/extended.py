#!/usr/bin/env python3
"""Independent test sensitivity, descriptor, and reuse probes in scratch."""
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import sys
import tempfile

ROOT = Path('/home/name/Documents/420-EMBDX/A')
LOG = ROOT / 'logs/commands/clear-review'
WORK = Path('/home/name/.hermes/cache/scratch/clear-review')
TREE = WORK / 'tree'
records = []
FLAGS = ['-std=c17','-Wall','-Wextra','-Wpedantic','-Wformat=2','-Wshadow','-Wconversion','-Wstrict-prototypes','-Werror','-g3','-O0']
INC = ['-I' + str(TREE / 'src'), '-I' + str(TREE / 'build/argtable3/include')]
LIB = ['-L' + str(TREE / 'build/argtable3/lib'), '-largtable3','-lm']
WRAPS = ['-Wl,--wrap=write,--wrap=isatty,--wrap=malloc,--wrap=arg_lit0,--wrap=arg_end,--wrap=arg_parse']
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

def run(label, args, expected=0, **kw):
    result = subprocess.run(args, cwd=TREE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=30, **kw)
    (LOG / (label + '.stdout')).write_bytes(result.stdout)
    (LOG / (label + '.stderr')).write_bytes(result.stderr)
    records.append(dict(label=label, argv=args, status=result.returncode,
                        expected=expected))
    (LOG / 'extended-results.json').write_text(json.dumps(records, indent=2) + '\n')
    print(label, result.returncode, 'expected', expected, flush=True)
    assert result.returncode == expected
    return result

# Force a real parser call followed by pointer-array mutation. Installed argtable
# may internally protect argv, so a natural permutation alone is not a control.
original = (TREE / 'tests/test_clear_module.c').read_text()
seam = '''
int __real_arg_parse(int argc, char **argv, void **table);
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int result = __real_arg_parse(argc, argv, table);
    if (argc > 1) {
        char *temp = argv[0];
        argv[0] = argv[1];
        argv[1] = temp;
    }
    return result;
}
'''
fixture = TREE / 'tests/clear_review_mutating_parser.c'
fixture.write_text(original.replace('    assert(argv[0] == name && argv[1] == (option == NULL ? NULL : arg));', '    if (argv[0] != name || argv[1] != (option == NULL ? NULL : arg))\n        dprintf(report_fd, "caller argv mutated after parser seam\\n");\n    assert(argv[0] == name && argv[1] == (option == NULL ? NULL : arg));') + seam)
(LOG / 'mutating-parser-fixture.c').write_text(fixture.read_text())
module_source = (TREE / 'src/cmd_clear.c').read_text()
for name, code, expected in [
    ('parser-copy', module_source, 0),
    ('parser-copy-negative', module_source.replace('arg_parse(argc, copy, table)', 'arg_parse(argc, argv, table)'), -6),
    ('getopt-negative', module_source.replace('optind = saved_optind;', 'optind = saved_optind + 1;'), -6)]:
    path = TREE / 'src' / (name + '.c')
    path.write_text(code)
    binary = WORK / name
    run(name + '-build', ['cc', *FLAGS, *INC, str(fixture), str(path), 'src/cmd_lines.c', *WRAPS, *LIB, '-o', str(binary)])
    run(name, [str(binary)], expected)

# Real external regular-file silence for each TERM state, with stdin closed.
for index, term in enumerate([None, '', 'dumb', 'xterm', 'DUMB', 'unknown-terminal']):
    env = dict(os.environ, LC_ALL='C')
    if term is None:
        env.pop('TERM', None)
    else:
        env['TERM'] = term
    with tempfile.TemporaryFile(dir=WORK) as out:
        result = subprocess.run([str(WORK / 'strict-clear')], env=env,
                                preexec_fn=lambda: os.close(0), stdout=out,
                                stderr=subprocess.PIPE, timeout=5)
        out.seek(0)
        data = out.read()
    records.append(dict(label='regular-file-' + str(index), term=term,
                        status=result.returncode, stdout_hex=data.hex(),
                        stderr_hex=result.stderr.hex(), stdin='closed'))
    assert (result.returncode, data, result.stderr) == (0, b'', b'')

# Negative byte control: parameter 3 violates exact selected visible-screen erase.
mutant = TREE / 'src/clear_review_byte_negative.c'
assert '\\033[H\\033[2J' in module_source
mutant.write_text(module_source.replace('\\033[H\\033[2J', '\\033[H\\033[3J'))
run('byte-negative-build', ['cc', *FLAGS, *INC,'src/clear_main.c',str(mutant),'src/cmd_lines.c',*LIB,'-o',str(WORK / 'byte-negative')])
run('byte-negative', [sys.executable,'tests/test_clear.py','--program',str(WORK / 'byte-negative'),'ClearTests.test_terminal_bytes'], 1)
# Repeated allocation/error/help calls under ASan+UBSan with the mutating seam.
repeat = TREE / 'tests/clear_review_repeat.c'
repeat.write_text(original.replace('invoke(NULL, 0);\n    invoke("--bad", 2);', 'for (int i = 0; i < 1000; ++i) {\n        invoke(NULL, 0);\n        invoke("--bad", 2);\n        invoke("--help", 0);\n    }\n    invoke(NULL, 0);\n    invoke("--bad", 2);', 1) + seam)
run('reuse-sanitize-build', ['cc', *FLAGS, '-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie',*INC,str(repeat),'src/cmd_clear.c','src/cmd_lines.c',*WRAPS,*LIB,'-o',str(WORK / 'reuse-sanitize')])
run('reuse-sanitize', [str(WORK / 'reuse-sanitize')], env=dict(os.environ, LC_ALL='C', ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1'))
(LOG / 'extended-results.json').write_text(json.dumps(records, indent=2) + '\n')
worker = json.loads((LOG / 'worker-clear-code-sha256.json').read_text())
comparison = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == value for name,value in worker.items()}
(LOG / 'worker-hash-comparison.json').write_text(json.dumps(comparison, indent=2) + '\n')
assert all(comparison.values())
print('ALL EXTENDED GATES PASSED; NEGATIVE CONTROLS FAILED AS EXPECTED')
