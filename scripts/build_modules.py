"""Build hidden command modules from their authoritative cmd_spec definitions."""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import stat

from build_package import no_links

ROOT = Path(__file__).resolve().parents[1]


def specifications():
    """Discover spec owners; dependency exceptions do not duplicate CLI metadata."""
    for source in sorted((ROOT / 'src').glob('cmd_*.c')):
        text = source.read_text()
        match = re.search(r'cmd_spec_t\s+(\w+)\s*=\s*\{\s*'
                          r'(?:\.name\s*=\s*)?"([a-z0-9-]+)"', text)
        if match:
            yield source, match.group(1), match.group(2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hello', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/modules')
    args = parser.parse_args()
    specs = [(source, spec, name) for source, spec, name in specifications()
             if (name == 'hello') == args.hello]
    expected = {name + '.so' for _, _, name in specs}
    no_links(args.output)
    args.output.mkdir(parents=True, exist_ok=True)
    # Refuse stale/renamed commands and unsafe outputs before invoking cc.
    # Unknown files belong to their owner; never prune them automatically.
    for path in args.output.iterdir():
        info = path.lstat()
        if (path.name not in expected | {'.stamp'} or
                not stat.S_ISREG(info.st_mode) or info.st_nlink != 1):
            raise ValueError('unexpected module output: ' + str(path))
    cc = shlex.split(os.environ.get('CC', 'cc'))
    cpp = shlex.split(os.environ.get('CPPFLAGS', ''))
    include = shlex.split(os.environ.get('ARGTABLE3_CPPFLAGS', '-Ibuild/module-deps/argtable3/include'))
    libs = shlex.split(os.environ.get('ARGTABLE3_LDLIBS', '-Lbuild/module-deps/argtable3/lib -largtable3 -lm'))
    flags = shlex.split(os.environ.get('CFLAGS', '-std=c17 -Wall -Wextra -g -O0'))
    linker = shlex.split(os.environ.get('LDFLAGS', ''))
    for source, spec, name in specs:
        inputs = [source, ROOT / 'src/cmd_lines.c']
        defines = []
        if name in ('help', 'exit', 'pkg'):
            defines += ['-DMODULE_BIND=' + name + '_bind']
        if name == 'pkg':
            inputs = [source, ROOT / 'src/module_pkg.c', ROOT / 'src/cmd_metadata.c', *[ROOT / ('src/' + p + '.c')
                      for p in ('pkg_store', 'pkg_fs', 'pkg_json', 'pkg_schema')]]
            defines += ['-DMODULE_REFRESH=pkg_refresh', '-DAISHELL_NATIVE_PACKAGE']
        if name == 'traverse':
            inputs = [source, ROOT / 'src/traverse.c']
        command = [*cc, *cpp, *include, *flags, '-fPIC', '-fvisibility=hidden',
                   '-ffunction-sections', '-fdata-sections', '-shared',
                   '-Wl,-z,defs,--exclude-libs,ALL,--gc-sections', *linker,
                   '-DMODULE_HEADER="' + source.with_suffix('.h').name + '"',
                   '-DMODULE_SPEC=' + spec, *defines, '-Isrc',
                   str(ROOT / 'src/module_entry.c'), *map(str, inputs),
                   *libs, '-o', str(args.output / (name + '.so'))]
        print(' '.join(command), flush=True)
        subprocess.run(command, cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
