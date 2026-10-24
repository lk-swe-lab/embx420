"""Prove focused package safety tests reject six intentional defects."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scratch', type=Path)
    args = parser.parse_args()
    args.scratch.mkdir(parents=True, exist_ok=True)
    flags = ['-std=c17', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-Isrc', '-Ibuild/argtable3/include', '-g', '-O1']
    wrappers = ('openat,close,read,__read_chk,write,renameat2,unlinkat,malloc,'
                'calloc,strdup,arg_lit0,arg_parse')
    link = ['-Wl,' + ','.join('--wrap=' + name for name in wrappers.split(',')),
            '-Lbuild/argtable3/lib', '-largtable3', '-lm']
    original = {name: (ROOT / ('src/' + name + '.c')).read_text()
                for name in ('cmd_pkg', 'pkg_store', 'pkg_fs', 'pkg_json',
                             'pkg_schema')}
    for defect in ('follow-link', 'ignore-copy-error', 'early-record-delete',
                   'ignore-busy-lock', 'ignore-overlap', 'ignore-root-ancestor'):
        target = args.scratch.resolve() / defect
        target.mkdir(exist_ok=True)
        sources = dict(original)
        if defect == 'follow-link':
            sources['pkg_fs'] = sources['pkg_fs'].replace('O_NOFOLLOW', '0')
            sources['pkg_store'] = replace_once(sources['pkg_store'],
                '!S_ISREG(st.st_mode) || st.st_nlink != 1 ||',
                '(!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) || '
                'st.st_nlink != 1 ||')
            case = 'PackageTests.test_source_safety'
            marker = b'AssertionError: 0 != 1'
        elif defect == 'ignore-copy-error':
            sources['pkg_store'] = replace_once(sources['pkg_store'],
                'stage_copy(source_fd, stage_fd, &record) != 0',
                'stage_copy(source_fd, stage_fd, &record) == 2')
            case = 'PackageTests.test_fault_install_rollback_and_recovery'
            marker = b'AssertionError: 0 != 1'
        elif defect == 'ignore-busy-lock':
            sources['pkg_fs'] = replace_once(sources['pkg_fs'],
                'fcntl(root->lock, F_SETLK, &lock) != 0',
                'fcntl(root->lock, F_SETLK, &lock) == 2')
            case = 'PackageTests.test_root_and_lock'
            marker = b'AssertionError: 0 != 1'
        elif defect == 'ignore-overlap':
            sources['pkg_store'] = replace_once(sources['pkg_store'],
                'pkg_install_disjoint(source_fd) != 0',
                'pkg_install_disjoint(source_fd) == 2')
            case = 'PackageTests.test_source_contains_home_no_mutation'
            marker = b'overlap rejection mutated source'
        elif defect == 'ignore-root-ancestor':
            sources['pkg_fs'] = replace_once(sources['pkg_fs'],
                'ancestor_of(fd, source) == 0 ? 0 : -1',
                'ancestor_of(fd, source) >= 0 ? 0 : -1')
            case = 'PackageTests.test_source_inside_managed_root_no_mutation'
            marker = b'overlap rejection mutated root/source'
        else:
            sources['pkg_store'] = replace_once(sources['pkg_store'],
                'if (fd < 0 || payload_delete(fd, record) != 0 ||',
                'if (fd >= 0) (void)unlink_recorded(fd, "pkg.json", 0);\n'
                '    if (fd < 0 || payload_delete(fd, record) != 0 ||')
            case = 'PackageTests.test_fault_remove_restore'
            marker = b'AssertionError: False is not true'
        paths = []
        for name, text in sources.items():
            path = target / (name + '.c')
            path.write_text(text)
            paths.append(str(path))
        binary = target / 'pkg'
        command = [os.environ.get('CC', 'cc'), *flags, 'tests/pkg_driver.c',
                   'tests/pkg_faults.c', *paths, *link, '-o', str(binary)]
        compiled = subprocess.run(command, cwd=ROOT, capture_output=True)
        (target / 'build.log').write_bytes(compiled.stdout + compiled.stderr)
        assert compiled.returncode == 0, compiled.stderr.decode()
        tested = subprocess.run([sys.executable, 'tests/test_pkg.py', str(binary),
                                 case], cwd=ROOT, capture_output=True, timeout=30)
        (target / 'test.log').write_bytes(tested.stdout + tested.stderr)
        assert tested.returncode == 1 and marker in tested.stderr, tested
        print(defect + ': intended assertion rejected mutant')


if __name__ == '__main__':
    main()
