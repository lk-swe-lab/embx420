"""Build the command-free host and test explicit session selection."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class EmptyHost(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.program = Path(cls.build.name) / 'aishell'
        selected = os.environ.get('AISHELL_TEST_PROGRAM')
        if selected:
            cls.program = Path(selected).absolute()
            return
        sources = ['main.c', 'shell.c', 'shell_input.c', 'module_runtime.c']
        if (ROOT / 'src/host_catalog.c').exists():
            sources += ['host_catalog.c', 'pkg_store.c', 'pkg_fs.c',
                        'pkg_schema.c', 'pkg_json.c', 'module_usage.c']
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Isrc',
                        *['src/' + s for s in sources], '-ldl',
                        '-o', str(cls.program)], cwd=ROOT, check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.home = Path(self.tmp.name)
        self.env = {'HOME': str(self.home), 'PATH': '/nonexistent'}

    def run_host(self, *args, data=None):
        return subprocess.run([str(self.program), *args], input=data,
                              capture_output=True, env=self.env,
                              cwd=self.home, timeout=10)

    def test_empty_registry_without_boot_payload(self):
        result = self.run_host('--loaded-json')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout),
                         {'schema_version': 1, 'packages': []})
        result = self.run_host('pwd')
        self.assertEqual(result.returncode, 127)
        self.assertEqual(result.stdout, b'')

    def test_host_help_and_quit_preserve_status(self):
        result = self.run_host('--help')
        self.assertEqual(result.returncode, 0)
        self.assertIn(b':load NAME', result.stdout)
        result = self.run_host(data=b'unknown\n:quit\nunknown\n')
        self.assertEqual(result.returncode, 127)
        self.assertEqual(result.stdout, b'')
        self.assertEqual(result.stderr.count(b'unknown command'), 1)

    def catalog(self, packages):
        path = self.home / 'catalog.json'
        path.write_text(json.dumps({'schema_version': 1,
                                   'packages': packages}))
        return path

    def test_catalog_listing_is_data_only(self):
        entry = {'name': 'sample', 'version': '1.0.0',
                 'description': 'Sample', 'path': 'sample-1.0.0',
                 'commands': ['sample']}
        path = self.catalog([entry])
        result = self.run_host('--catalog', str(path), '--available-json')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['packages'], [entry])
        result = self.run_host('--catalog', str(path), data=(
            b':available --json\n:loaded --json\n:quit\n'))
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = result.stdout.splitlines()
        self.assertEqual(json.loads(rows[0])['packages'], [entry])
        self.assertEqual(json.loads(rows[1])['packages'], [])

    def bundle(self, name='sample', commands=('sample',), abi=1):
        bundle = self.home / (name + '-1.0.0')
        for folder in ('lib', 'docs', 'metadata'):
            (bundle / folder).mkdir(parents=True, exist_ok=True)
        for command in commands:
            source = self.home / (command + '.c')
            source.write_text('''
#include "module_abi.h"
#include <stdio.h>
#include <stdlib.h>
__attribute__((constructor)) static void marker(void) {
    const char *path = getenv("MARKER");
    FILE *out = path ? fopen(path, "a") : NULL;
    if (out) { fputs("loaded\\n", out); fclose(out); }
}
static int run(int argc, char **argv) {
    (void)argc; (void)argv; puts("ran"); return 7;
}
static void usage(FILE *out) { fputs("sample\\n", out); }
static const cmd_spec_t spec = {
    "COMMAND", "Sample", "", run, usage, NULL
};
AISHELL_EXPORT const aishell_module_v1 *
aishell_module_entry_v1(const aishell_host_v1 *host) {
    (void)host;
    static const cmd_spec_t *const specs[] = {&spec};
    static const aishell_module_v1 descriptor = {
        sizeof(aishell_module_v1), ABI, sizeof(cmd_spec_t), 1, specs, NULL
    };
    return &descriptor;
}
'''.replace('COMMAND', command).replace('ABI,', str(abi) + ','))
            subprocess.run(['cc', '-shared', '-fPIC', '-Isrc', str(source),
                            '-o', str(bundle / 'lib' / (command + '.so'))],
                           cwd=ROOT, check=True)
            (bundle / 'docs' / (command + '.md')).write_text('Sample\n')
        metadata = {'schema_version': 2, 'package': name,
                    'version': '1.0.0', 'commands': [
            {'name': c, 'summary': 'Sample', 'long_help': '',
             'usage': 'sample'} for c in commands]}
        (bundle / 'metadata/commands.json').write_text(json.dumps(metadata))
        files = sorted(['metadata/commands.json'] +
                       ['lib/' + c + '.so' for c in commands] +
                       ['docs/' + c + '.md' for c in commands])
        manifest = {'schema_version': 3, 'name': name, 'version': '1.0.0',
                    'description': 'Sample', 'commands': list(commands),
                    'files': files, 'docs': ['docs/' + c + '.md'
                                            for c in commands],
                    'dependencies': []}
        (bundle / 'pkg.json').write_text(json.dumps(manifest))
        return {'name': name, 'version': '1.0.0', 'description': 'Sample',
                'path': bundle.name, 'commands': list(commands)}

    def test_explicit_load_and_session_only(self):
        entry = self.bundle()
        path = self.catalog([entry])
        self.env['MARKER'] = str(self.home / 'marker')
        result = self.run_host('--catalog', str(path), '--available-json')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.home / 'marker').exists())
        result = self.run_host('--catalog', str(path), '--load', 'sample',
                               '--available-json')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.home / 'marker').exists())
        result = self.run_host('--catalog', str(path), '--load', 'sample',
                               'sample')
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assertEqual(result.stdout, b'ran\n')
        result = self.run_host('--catalog', str(path), '--load', 'sample',
                               '--load', 'sample', '--loaded-json')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['packages'], [
            {'name': 'sample', 'version': '1.0.0', 'commands': ['sample']}])
        result = self.run_host('--catalog', str(path), data=(
            b':load sample\nsample\n:quit\n'))
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assertEqual(result.stdout, b'ran\n')
        self.assertEqual(self.run_host('--catalog', str(path),
                                      'sample').returncode, 127)
        self.assertFalse((self.home / '.mysh').exists())

    def test_invalid_bundle_data_does_not_run_constructor(self):
        entry = self.bundle()
        path = self.catalog([entry])
        self.env['MARKER'] = str(self.home / 'marker')
        bundle = self.home / entry['path']
        (bundle / 'unlisted').write_text('bad')
        result = self.run_host('--catalog', str(path), '--load', 'sample')
        self.assertEqual(result.returncode, 1)
        self.assertFalse((self.home / 'marker').exists())
        (bundle / 'unlisted').unlink()
        os.link(bundle / 'lib/sample.so', self.home / 'hardlink')
        self.assertEqual(self.run_host('--catalog', str(path), '--load',
                                      'sample').returncode, 1)
        self.assertFalse((self.home / 'marker').exists())

    def test_invalid_abi_and_usage_leave_empty_registry(self):
        for abi in (999, 1):
            entry = self.bundle(abi=abi)
            path = self.catalog([entry])
            if abi == 1:
                meta = self.home / entry['path'] / 'metadata/commands.json'
                data = json.loads(meta.read_text())
                data['commands'][0]['usage'] = 'drift'
                meta.write_text(json.dumps(data))
            result = self.run_host('--catalog', str(path), data=(
                b':load sample\n:loaded --json\n:quit\n'))
            self.assertEqual(json.loads(result.stdout)['packages'], [])
            self.assertIn(b'cannot load bundle', result.stderr)

    def test_conflict_is_atomic(self):
        first = self.bundle('first', ('sample',))
        second = self.bundle('second', ('alpha', 'sample'))
        path = self.catalog([first, second])
        result = self.run_host('--catalog', str(path), data=(
            b':load first\n:load second\n:loaded --json\nalpha\n:quit\n'))
        self.assertEqual(result.returncode, 127, result.stderr)
        self.assertEqual(json.loads(result.stdout)['packages'], [
            {'name': 'first', 'version': '1.0.0', 'commands': ['sample']}])

    def pkg_bundle(self):
        entry = self.bundle('pkg', ('pkg',))
        source = self.home / 'pkg-wrapper.c'
        source.write_text('''
#include "module_abi.h"
#include "module_pkg.h"
static void usage(FILE *out) { fputs("sample\\n", out); }
static const cmd_spec_t spec = {
    "pkg", "Sample", "", module_pkg_run, usage, NULL
};
AISHELL_EXPORT const aishell_module_v1 *
aishell_module_entry_v1(const aishell_host_v1 *host) {
    pkg_bind(host);
    static const cmd_spec_t *const specs[] = {&spec};
    static const aishell_module_v1 descriptor = {
        sizeof(aishell_module_v1), 1, sizeof(cmd_spec_t), 1, specs, pkg_refresh
    };
    return &descriptor;
}
''')
        sources = ['module_pkg.c', 'cmd_metadata.c', 'module_usage.c',
                   'pkg_store.c', 'pkg_fs.c', 'pkg_json.c', 'pkg_schema.c']
        subprocess.run(['cc', '-shared', '-fPIC', '-fvisibility=hidden',
                        '-Isrc', '-Ibuild/module-deps/argtable3/include',
                        str(source), *['src/' + s for s in sources],
                        '-Lbuild/module-deps/argtable3/lib', '-largtable3',
                        '-lm', '-o', str(self.home / entry['path'] /
                                        'lib/pkg.so')], cwd=ROOT, check=True)
        return entry

    def test_pkg_schema3_install_loads_only_touched_target(self):
        import shutil
        pkg = self.pkg_bundle()
        foreign = self.bundle('foreign', ('foreign',))
        target = self.bundle('target', ('target',))
        store = self.home / '.mysh/pkgs'
        (store / '.active').mkdir(parents=True, mode=0o700)
        (self.home / '.mysh').chmod(0o700)
        store.chmod(0o700)
        shutil.copytree(self.home / foreign['path'], store / foreign['path'])
        for folder in (store / foreign['path']).rglob('*'):
            if folder.is_dir():
                folder.chmod(0o700)
        (store / foreign['path']).chmod(0o700)
        (store / '.active/foreign').write_text('1.0.0')
        (store / '.active/foreign').chmod(0o600)
        path = self.catalog([pkg])
        script = (':load pkg\npkg list\n:loaded --json\n'
                  'pkg install ' + str(self.home / target['path']) +
                  '\n:loaded --json\ntarget\n:quit\n').encode()
        result = self.run_host('--catalog', str(path), data=script)
        self.assertEqual(result.returncode, 7, result.stderr)
        inventories = [json.loads(line)['packages']
                       for line in result.stdout.splitlines()
                       if line.startswith(b'{')]
        self.assertEqual([p['name'] for p in inventories[0]], ['pkg'])
        self.assertEqual([p['name'] for p in inventories[1]], ['pkg', 'target'])
        self.assertEqual((store / '.active/foreign').read_text(), '1.0.0')
        script = (':load pkg\npkg use foreign 1.0.0\nforeign\n'
                  'pkg remove foreign\nforeign\npkg use foreign 1.0.0\n'
                  'foreign\n:quit\n').encode()
        result = self.run_host('--catalog', str(path), data=script)
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assertEqual(result.stdout.count(b'ran\n'), 2)
        self.assertEqual(result.stderr.count(b'unknown command'), 1)

    def test_no_automatic_neighbor_module_load(self):
        import shutil
        entry = self.bundle()
        modules = self.program.parent / 'modules'
        modules.mkdir(exist_ok=True)
        payload = modules / 'sample.so'
        shutil.copy2(self.home / entry['path'] / 'lib/sample.so', payload)
        self.addCleanup(payload.unlink)
        self.env['MARKER'] = str(self.home / 'marker')
        result = self.run_host('--loaded-json')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['packages'], [])
        self.assertFalse((self.home / 'marker').exists())

    def test_interactive_inventory_and_prompt(self):
        import pty
        master, slave = pty.openpty()
        try:
            process = subprocess.Popen([str(self.program)], stdin=slave,
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, env=self.env,
                                       cwd=self.home)
            os.write(master, b':quit\n')
            out, errors = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 0, errors)
            self.assertIn(b'Available packages:', out)
            self.assertIn(b'main> ', errors)
        finally:
            os.close(master)
            os.close(slave)

    def test_default_catalog_is_executable_relative(self):
        import shutil
        release = self.home / 'release'
        (release / 'catalog').mkdir(parents=True)
        program = release / 'aishell'
        shutil.copy2(self.program, program)
        entry = {'name': 'sample', 'version': '1.0.0',
                 'description': 'Sample', 'path': 'sample-1.0.0',
                 'commands': ['sample']}
        (release / 'catalog/catalog.json').write_text(json.dumps({
            'schema_version': 1, 'packages': [entry]}))
        result = subprocess.run([str(program), '--available-json'],
                                cwd=self.home, env=self.env,
                                capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['packages'], [entry])

    def test_catalog_and_bundle_links_rejected(self):
        entry = self.bundle()
        path = self.catalog([entry])
        alias = self.home / 'alias.json'
        alias.symlink_to(path)
        self.assertEqual(self.run_host('--catalog', str(alias),
                                      '--available-json').returncode, 1)
        alias.unlink()
        os.link(path, alias)
        self.assertEqual(self.run_host('--catalog', str(path),
                                      '--available-json').returncode, 1)
        alias.unlink()
        bundle = self.home / entry['path']
        renamed = self.home / 'real-bundle'
        bundle.rename(renamed)
        bundle.symlink_to(renamed, target_is_directory=True)
        self.env['MARKER'] = str(self.home / 'marker')
        self.assertEqual(self.run_host('--catalog', str(path), '--load',
                                      'sample').returncode, 1)
        self.assertFalse((self.home / 'marker').exists())

    def test_explicit_missing_catalog_fails(self):
        result = self.run_host('--catalog', 'absent', '--available-json')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b'')
        self.env['AISHELL_CATALOG'] = str(self.home / 'absent')
        self.assertEqual(self.run_host('--loaded-json').returncode, 1)

    def test_invalid_catalog_rejected_before_output(self):
        good = {'name': 'sample', 'version': '1.0.0',
                'description': 'Sample', 'path': 'sample-1.0.0',
                'commands': ['sample']}
        for change in ({'path': '../sample-1.0.0'}, {'path': '/absolute'},
                       {'path': 'wrong'}, {'commands': [':quit']},
                       {'version': '01.0.0'}, {'commands': ['x', 'x']}):
            with self.subTest(change=change):
                path = self.catalog([{**good, **change}])
                result = self.run_host('--catalog', str(path),
                                       '--available-json')
                self.assertEqual(result.returncode, 1)
                self.assertEqual(result.stdout, b'')
        path = self.catalog([good, good])
        self.assertEqual(self.run_host('--catalog', str(path),
                                      '--available-json').returncode, 1)
        other = {**good, 'name': 'zeta', 'path': 'zeta-1.0.0'}
        path = self.catalog([other, good])
        self.assertEqual(self.run_host('--catalog', str(path),
                                      '--available-json').returncode, 1)
        entries = [{**good, 'name': 'p' + str(i).zfill(2),
                    'path': 'p' + str(i).zfill(2) + '-1.0.0'}
                   for i in range(65)]
        path = self.catalog(entries)
        self.assertEqual(self.run_host('--catalog', str(path),
                                      '--available-json').returncode, 1)
        path = self.catalog(entries[:64])
        self.assertEqual(self.run_host('--catalog', str(path),
                                      '--available-json').returncode, 0)


if __name__ == '__main__':
    unittest.main()
