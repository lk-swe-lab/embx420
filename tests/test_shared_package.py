"""Native package and version-selection integration, no worker threads."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('builder', ROOT / 'scripts/build_package.py')
assert SPEC is not None and SPEC.loader is not None
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)
PROGRAM = Path(os.environ.get('AISHELL_TEST_PROGRAM', ROOT / 'build/aishell'))
PACKAGE = ROOT / 'build/native-candidates/hello-0.2.0'


def compile_fixture(target, *defines):
    flags = ['cc', '-std=c17', '-fvisibility=hidden', '-Isrc',
             '-Ibuild/module-deps/argtable3/include', *defines]
    libraries = ['-Lbuild/module-deps/argtable3/lib', '-largtable3', '-lm']
    subprocess.run([*flags, '-fPIC', '-shared', 'tests/module_fixture.c',
                    *libraries, '-o', str(target)], cwd=ROOT, check=True,
                   capture_output=True)
    exporter = target.parent / 'export-fixture'
    try:
        subprocess.run([*flags, 'tests/export_module_fixture.c',
                        'src/cmd_metadata.c', *libraries, '-o', str(exporter)],
                       cwd=ROOT, check=True, capture_output=True)
        return json.loads(subprocess.check_output([exporter]))
    finally:
        exporter.unlink(missing_ok=True)


class SharedPackages(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.home = self.base / 'home'
        self.home.mkdir()
        self.env = {'HOME': str(self.home), 'PATH': '/nonexistent', 'LC_ALL': 'C'}

    def cli(self, *args, data=None, status=0):
        result = subprocess.run([str(PROGRAM), *args], input=data,
                                capture_output=True, cwd=self.base,
                                env=self.env, timeout=10)
        self.assertEqual(result.returncode, status, result)
        return result

    def test_shared_manifest_contract(self):
        manifest = {'schema_version': 2, 'name': 'hello', 'version': '0.2.0',
                    'description': 'Print a greeting.', 'commands': ['hello'],
                    'files': ['docs/hello.md', 'lib/hello.so', 'metadata/commands.json'],
                    'docs': ['docs/hello.md'], 'dependencies': []}
        builder.manifest_check(manifest)

    def test_info_inventory_from_manifest(self):
        self.cli('pkg', 'install', str(PACKAGE))
        info = self.cli('pkg', 'info', 'hello').stdout
        self.assertIn(b'hello 0.2.0 active\n', info)
        self.assertIn(b'Description: Print a greeting.\nCommands: hello\n', info)

    def test_running_install_help_remove(self):
        script = (f'hello\npkg install {PACKAGE}\nhelp hello\nhello --name live\n'
                  'help --json\npkg remove hello\nhello\nexit\n').encode()
        result = self.cli(data=script, status=127)
        self.assertIn(b'Installed hello 0.2.0\n', result.stdout)
        self.assertIn(b'Usage: hello [-h] [--name=NAME]\n', result.stdout)
        self.assertIn(b'Hello, live!\n', result.stdout)
        self.assertIn(b'"origin":"package","package":"hello","version":"0.2.0"', result.stdout)
        self.assertEqual(result.stderr, b'main: hello: unknown command\n' * 2)
        self.cli('hello', status=127)
        self.assertTrue((self.home / '.mysh/pkgs/hello-0.2.0/lib/hello.so').is_file())

    def test_generated_module_package_matches_authoritative_spec(self):
        manifest = json.loads((PACKAGE / 'pkg.json').read_bytes())
        self.assertEqual(manifest, json.loads(
            (ROOT / 'packages/hello-so/pkg.json').read_bytes()))
        self.assertEqual(manifest['schema_version'], 2)
        self.assertEqual(sorted(str(p.relative_to(PACKAGE))
                                for p in PACKAGE.rglob('*') if p.is_file()),
                         sorted(['pkg.json', *manifest['files']]))
        self.assertEqual((PACKAGE / 'lib/hello.so').read_bytes(),
                         (ROOT / 'build/example-modules/hello.so').read_bytes())
        metadata = json.loads((PACKAGE / 'metadata/commands.json').read_bytes())
        command = json.loads(subprocess.check_output([ROOT / 'build/export-hello']))
        self.assertEqual(metadata['commands'], [command])
        page = (PACKAGE / 'docs/hello.md').read_text()
        for text in (command['name'], command['summary'], command['usage'], '--name'):
            self.assertIn(text, page)
        for relative in ['pkg.json', *manifest['files']]:
            self.assertEqual((PACKAGE / relative).stat().st_mode & 0o777, 0o644)

    def changed_package(self, version):
        result = self.base / ('hello-' + version)
        shutil.copytree(PACKAGE, result)
        for file in ('pkg.json', 'metadata/commands.json'):
            value = json.loads((result / file).read_bytes())
            value['version'] = version
            (result / file).write_text(json.dumps(value))
        return result

    def test_versions_restart_and_fresh_rollback(self):
        newer = self.changed_package('0.3.0')
        command = compile_fixture(newer / 'lib/hello.so',
                                  '-DFIXTURE_GREETING', '-DFIXTURE_STATUS=0')
        path = newer / 'metadata/commands.json'
        metadata = json.loads(path.read_bytes())
        metadata['commands'] = [command]
        path.write_text(json.dumps(metadata))
        self.cli('pkg', 'install', str(PACKAGE))
        result = self.cli(data=(f'hello\npkg install {newer}\nhello\n'
                                'help --json\nexit\n').encode())
        self.assertIn(b'restart required', result.stderr)
        self.assertEqual(result.stdout.count(b'Hello, world!\n'), 2)
        self.assertIn(b'"version":"0.2.0"', result.stdout)
        catalog = json.loads(self.cli('--commands-json').stdout)
        self.assertEqual(next(c for c in catalog['commands'] if c['name'] == 'hello')['version'], '0.3.0')
        self.assertEqual(self.cli('hello').stdout, b'Hello, newer!\n')
        rollback = self.cli(data=b'hello\npkg use hello 0.2.0\nhello\nexit\n')
        self.assertEqual(rollback.stdout.count(b'Hello, newer!\n'), 2)
        self.assertIn(b'restart required', rollback.stderr)
        catalog = json.loads(self.cli('--commands-json').stdout)
        self.assertEqual(next(c for c in catalog['commands'] if c['name'] == 'hello')['version'], '0.2.0')
        self.assertEqual(self.cli('hello').stdout, b'Hello, world!\n')
        listed = self.cli('pkg', 'list').stdout.splitlines()
        self.assertEqual(set(listed), {b'hello 0.2.0 active', b'hello 0.3.0 retained'})
        self.cli('pkg', 'install', str(PACKAGE), status=1)
        self.cli('pkg', 'use', 'hello', '../bad', status=2)
        self.cli('pkg', 'use', 'hello', '9.9.9', status=1)

    def assert_metadata_drift_rejected(self, field):
        self.cli('pkg', 'install', str(PACKAGE))
        newer = self.changed_package('0.3.0')
        path = newer / 'metadata/commands.json'
        metadata = json.loads(path.read_bytes())
        command = metadata['commands'][0]
        if field == 'summary':
            command['summary'] = 'Not the loaded command summary.'
        else:
            command['options'].append({
                'metavar': 'VALUE', 'description': 'Nonexistent option',
                'min_count': 1, 'max_count': 1, 'short': [],
                'long': ['nonexistent'], 'value': 'required'})
        path.write_text(json.dumps(metadata))
        self.cli('pkg', 'install', str(newer), status=1)
        root = self.home / '.mysh/pkgs'
        self.assertEqual((root / '.active/hello').read_text(), '0.2.0')
        self.assertFalse((root / 'hello-0.3.0').exists())
        self.assertFalse(list(root.glob('.tmp-*')))
        self.assertEqual(self.cli('hello').stdout, b'Hello, world!\n')
        # A retained payload with drift must also fail explicit selection.
        retained = root / 'hello-0.3.0'
        shutil.copytree(newer, retained)
        retained.chmod(0o700)
        self.cli('pkg', 'use', 'hello', '0.3.0', status=1)
        self.assertEqual((root / '.active/hello').read_text(), '0.2.0')

    def test_summary_drift_rejected(self):
        self.assert_metadata_drift_rejected('summary')

    def test_option_grammar_drift_rejected(self):
        self.assert_metadata_drift_rejected('grammar')

    def negative_zero_package(self):
        package = self.changed_package('0.3.0')
        path = package / 'metadata/commands.json'
        original = path.read_text()
        equivalent = original.replace('"min_count": 0', '"min_count": -0')
        self.assertNotEqual(original, equivalent)
        self.assertEqual(json.loads(original), json.loads(equivalent))
        path.write_text(equivalent)
        return package

    def test_install_accepts_equivalent_negative_zero_counts(self):
        package = self.negative_zero_package()
        self.cli('pkg', 'install', str(package))
        self.assertEqual(self.cli('hello').stdout, b'Hello, world!\n')
        active = self.home / '.mysh/pkgs/.active/hello'
        self.assertEqual(active.read_text(), '0.3.0')

    def test_use_accepts_equivalent_negative_zero_counts(self):
        self.cli('pkg', 'install', str(PACKAGE))
        package = self.negative_zero_package()
        root = self.home / '.mysh/pkgs'
        retained = root / 'hello-0.3.0'
        shutil.copytree(package, retained)
        retained.chmod(0o700)
        self.cli('pkg', 'use', 'hello', '0.3.0')
        self.assertEqual((root / '.active/hello').read_text(), '0.3.0')
        self.assertEqual(self.cli('hello').stdout, b'Hello, world!\n')

    def test_unrelated_corruption_keeps_core(self):
        root = self.home / '.mysh/pkgs/.active'
        root.mkdir(parents=True, mode=0o700)
        root.parent.chmod(0o700)
        root.parent.parent.chmod(0o700)
        (root / 'broken').write_text('../escape')
        result = self.cli('pwd')
        self.assertEqual(result.stdout, os.fsencode(self.base) + b'\n')
        self.assertIn(b'corrupt', result.stderr)


if __name__ == '__main__':
    unittest.main()
