"""Build isolated C package test binaries. Never run package operations."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output.resolve()
    flags = ['-std=c17', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-Wshadow', '-Wconversion', '-Wformat=2', '-Wstrict-prototypes',
             '-g', '-O1', '-Isrc', '-Ibuild/module-deps/argtable3/include']
    sanitize = (['-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                 '-no-pie'] if args.sanitize else [])
    sources = ['src/cmd_pkg.c', 'src/pkg_store.c', 'src/pkg_fs.c',
               'src/pkg_json.c', 'src/pkg_schema.c']
    libraries = ['-Lbuild/module-deps/argtable3/lib', '-largtable3', '-lm']
    wrappers = ('openat,close,read,__read_chk,write,renameat2,unlinkat,malloc,calloc,strdup,'
                'arg_lit0,arg_parse')
    wrap = ['-Wl,' + ','.join('--wrap=' + name for name in wrappers.split(','))]
    cc = os.environ.get('CC', 'cc')
    jobs = [(['tests/pkg_driver.c', 'tests/pkg_faults.c', *sources,
              *wrap, *libraries], 'pkg', sanitize),
            (['tests/pkg_openat_driver.c', 'tests/pkg_faults.c',
              *wrap, *libraries], 'openat', sanitize),
            (['tests/test_pkg_module.c', *sources, '-Wl,--wrap=arg_parse',
              *libraries], 'module', sanitize),
            (['tests/pkg_json_driver.c', 'src/pkg_json.c'], 'json', []),
            (['tests/pkg_json_driver.c', 'src/pkg_json.c'], 'json-sanitize',
             sanitize)]
    for inputs, name, extra in jobs:
        command = [cc, *flags, *extra, *inputs, '-o', str(output / name)]
        print(' '.join(command), flush=True)
        subprocess.run(command, cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
