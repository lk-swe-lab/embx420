"""Real-main package discovery and foreground execution checks."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

PROGRAM = str(Path(sys.argv.pop(1)).resolve())
PACKAGE = Path(__file__).resolve().parents[1] / 'build/packages/hello-0.1.0'


class PackageShellTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        self.env = {'HOME': str(self.home), 'LC_ALL': 'C',
                    'PATH': '/usr/bin:/bin'}

    def cli(self, *args, data=b'', status=0):
        result = subprocess.run([PROGRAM, *args], input=data,
                                capture_output=True, env=self.env,
                                cwd=self.home, timeout=5)
        self.assertEqual(result.returncode, status, result)
        if status == 0:
            self.assertEqual(result.stderr, b'')
        return result

    def test_pkg_builtin_without_hello_builtin(self):
        self.assertEqual(self.cli('pkg', 'list').stdout, b'')
        help_text = self.cli('help', 'pkg').stdout
        self.assertIn(b'Usage: pkg', help_text)
        self.assertIn(b'Manage local packages.', self.cli('--help').stdout)
        self.cli('hello', status=127)
        self.assertEqual(list(self.home.iterdir()), [])


    def test_installed_help_without_execution(self):
        self.cli('pkg', 'install', str(PACKAGE))
        expected = subprocess.run([str(PACKAGE / 'bin/hello'), '--help'],
                                  capture_output=True, timeout=5).stdout
        self.assertEqual(self.cli('help', 'hello').stdout, expected)
        self.assertIn(b'hello', self.cli('--help').stdout)
        self.assertIn(b'hello', self.cli('help').stdout)
        self.assertIn(b'main --commands-json', self.cli('--help').stdout)


    def test_catalog_route_schema_sort_and_refresh(self):
        empty = self.cli('--commands-json').stdout
        catalog = json.loads(empty)
        self.assertEqual(set(catalog), {'schema_version', 'commands'})
        self.assertEqual(catalog['schema_version'], 1)
        names = [entry['name'] for entry in catalog['commands']]
        self.assertEqual(names, sorted(names))
        self.assertEqual(len(names), len(set(names)))
        self.assertIn('pkg', names)
        self.assertNotIn('hello', names)
        for entry in catalog['commands']:
            self.assertEqual(set(entry), {'name', 'summary', 'origin',
                                         'package', 'version'})
            self.assertIsNone(entry['package'])
            self.assertIsNone(entry['version'])
            self.assertEqual(entry['origin'], 'control' if entry['name'] in
                             ('help', 'exit') else 'builtin')
        self.assertEqual(self.cli('--commands-json').stdout, empty)
        self.cli('--commands-json', 'extra', status=2)
        self.cli('pkg', 'install', str(PACKAGE))
        entries = json.loads(self.cli('--commands-json').stdout)['commands']
        hello = [entry for entry in entries if entry['name'] == 'hello']
        self.assertEqual(hello, [{'name': 'hello', 'summary': 'Print a greeting.',
                                 'origin': 'package', 'package': 'hello',
                                 'version': '0.1.0'}])
        self.cli('pkg', 'remove', 'hello')
        self.assertEqual(self.cli('--commands-json').stdout, empty)


    def test_installed_execute_and_persistent_refresh(self):
        script = (f'pkg install "{PACKAGE}"\nhelp hello\n'
                  'hello --name "two words"\npkg remove hello\n'
                  'hello\nexit\n').encode()
        result = self.cli(data=script, status=127)
        expected_help = subprocess.run([str(PACKAGE / 'bin/hello'), '--help'],
                                      capture_output=True, timeout=5).stdout
        self.assertEqual(result.stdout, b'Installed hello 0.1.0\n' +
                         expected_help + b'Hello, two words!\n' +
                         b'Removed hello 0.1.0\n')
        self.assertEqual(result.stderr, b'main: hello: unknown command\n')
        self.assertFalse((self.home / '.mysh/pkgs/hello-0.1.0').exists())


    def custom_package(self, body):
        import shutil
        source = self.home / 'source'
        shutil.copytree(PACKAGE, source)
        binary = source / 'bin/hello'
        binary.write_bytes(body)
        binary.chmod(0o700)
        return source

    def test_child_statuses_and_no_path_fallback(self):
        source = self.custom_package(b'#!/bin/sh\nexit 23\n')
        self.cli('pkg', 'install', str(source))
        self.cli('hello', status=23)
        installed = self.home / '.mysh/pkgs/hello-0.1.0/bin/hello'
        installed.write_bytes(b'#!/bin/sh\nkill -TERM $$\n')
        self.cli('hello', status=143)
        poison = self.home / 'poison'
        poison.mkdir()
        for name in ('hello', 'unknown', 'pwd'):
            fake = poison / name
            fake.write_bytes(b'#!/bin/sh\necho PATH-POISON\n')
            fake.chmod(0o700)
        self.env['PATH'] = str(poison)
        installed.write_bytes(b'not an executable image\n')
        result = self.cli('hello', status=126)
        self.assertIn(b'cannot execute', result.stderr)
        self.assertNotIn(b'PATH-POISON', result.stdout)
        self.cli('unknown', status=127)
        self.assertEqual(self.cli('pwd').stdout, os.fsencode(self.home) + b'\n')
        installed.write_bytes(b'#!/bin/sh\nprintf "<%s>\\n" "$@"\n')
        self.assertEqual(self.cli('hello', 'two words', '', '*').stdout,
                         b'<two words>\n<>\n<*>\n')
        self.cli('help', 'hello')
        self.cli('--commands-json')

    def test_inherited_stdin_and_parent_no_readahead(self):
        source = self.custom_package(b'#!/bin/sh\nIFS= read -r x\n'
                                     b'printf "data:<%s>\\n" "$x"\n')
        self.cli('pkg', 'install', str(source))
        result = self.cli(data=b'hello\none data line\npwd\nexit\n')
        self.assertEqual(result.stdout, b'data:<one data line>\n' +
                         os.fsencode(self.home) + b'\n')

    def test_corrupt_state_keeps_direct_builtins_and_controls(self):
        self.cli('pkg', 'install', str(PACKAGE))
        manifest = self.home / '.mysh/pkgs/hello-0.1.0/pkg.json'
        manifest.write_bytes(b'bad record')
        for args in (('--commands-json',), ('--help',), ('help',),
                     ('help', 'hello'), ('hello',), ('unknown',)):
            result = self.cli(*args, status=1)
            self.assertEqual(result.stdout, b'')
            self.assertTrue(result.stderr)
        self.cli('help', 'pwd')
        self.cli('pkg', '--help')
        self.cli('exit')
        self.assertEqual(self.cli('pwd').stdout, os.fsencode(self.home) + b'\n')
        self.assertEqual(self.cli('-r', str(self.home / 'absent'),
                                  status=1).stdout, b'')

    def test_snapshot_lock_contention_and_no_child_lock(self):
        import fcntl
        self.cli('pkg', 'install', str(PACKAGE))
        lock = self.home / '.mysh/pkgs/.lock'
        with lock.open('r+') as holder:
            fcntl.lockf(holder, fcntl.LOCK_EX | fcntl.LOCK_NB)
            for args in (('--commands-json',), ('--help',), ('help', 'hello'),
                         ('hello',), ('unknown',)):
                self.assertEqual(self.cli(*args, status=1).stdout, b'')
            self.cli('help', 'pwd')
            self.cli('pwd')
        # The child can perform a package read while its parent waits.
        installed = self.home / '.mysh/pkgs/hello-0.1.0/bin/hello'
        installed.write_text('#!/bin/sh\nexec "' + PROGRAM + '" pkg list\n')
        self.assertEqual(self.cli('hello').stdout, b'hello 0.1.0\n')

    def test_external_update_visible_to_already_running_loop(self):
        import select
        proc = subprocess.Popen([PROGRAM], stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                env=self.env, cwd=self.home)
        self.addCleanup(lambda: proc.kill() if proc.poll() is None else None)
        assert proc.stdin is not None and proc.stdout is not None
        input_stream, output_stream = proc.stdin, proc.stdout
        def line(command):
            input_stream.write(command + b'\n')
            input_stream.flush()
            self.assertTrue(select.select([output_stream], [], [], 5)[0])
            return output_stream.readline()
        self.assertEqual(line(b'pwd'), os.fsencode(self.home) + b'\n')
        self.cli('pkg', 'install', str(PACKAGE))
        self.assertIn('hello', [e['name'] for e in json.loads(
            self.cli('--commands-json').stdout)['commands']])
        self.assertEqual(line(b'hello'), b'Hello, world!\n')
        self.cli('pkg', 'remove', 'hello')
        proc.stdin.write(b'hello\nexit\n')
        out, err = proc.communicate(timeout=5)
        self.assertEqual((proc.returncode, out, err),
                         (127, b'', b'main: hello: unknown command\n'))


    def test_module_faults_and_borrowed_state(self):
        self.cli('pkg', 'install', str(PACKAGE))
        module = str(Path(PROGRAM).with_name('test_package_shell_module'))
        for mode in ('fork', 'wait-eintr', 'wait-fail', 'exec-fail',
                     'ownership', 'catalog-oom'):
            result = subprocess.run([module, mode], env=self.env,
                                    cwd=self.home, capture_output=True,
                                    timeout=5)
            self.assertEqual(result.returncode, 0, (mode, result))


    def test_discovery_does_not_consume_stdin_and_reports_output_error(self):
        self.cli('pkg', 'install', str(PACKAGE))
        data = self.home / 'input'
        data.write_bytes(b'untouched input\n')
        for args in (('--commands-json',), ('--help',), ('help', 'hello'),
                     ('pkg', 'list'), ('pkg', '--help')):
            with data.open('rb') as stream:
                result = subprocess.run([PROGRAM, *args], stdin=stream,
                                        capture_output=True, env=self.env,
                                        cwd=self.home, timeout=5)
                self.assertEqual(result.returncode, 0, result)
                self.assertEqual(stream.tell(), 0, args)
            with open('/dev/full', 'wb') as full:
                result = subprocess.run([PROGRAM, *args], stdout=full,
                                        stderr=subprocess.PIPE, env=self.env,
                                        cwd=self.home, timeout=5)
                self.assertEqual(result.returncode, 1, (args, result))

    def test_recorded_path_after_cd_and_loop_child_status(self):
        self.cli('pkg', 'install', str(PACKAGE))
        other = self.home / 'different cwd'
        other.mkdir()
        result = self.cli(data=b'cd "different cwd"\nhello\npwd\nexit\n')
        self.assertEqual(result.stdout, b'Hello, world!\n' +
                         os.fsencode(other) + b'\n')
        installed = self.home / '.mysh/pkgs/hello-0.1.0/bin/hello'
        for body, status in ((b'#!/bin/sh\nexit 23\n', 23),
                             (b'#!/bin/sh\nkill -TERM $$\n', 143),
                             (b'no image\n', 126)):
            installed.write_bytes(body)
            self.cli(data=b'hello\nexit\n', status=status)

    def test_catalog_escapes_stored_strings_without_execution(self):
        source = self.custom_package(b'not an executable image\n')
        path = source / 'metadata/commands.json'
        metadata = json.loads(path.read_text())
        summary = r'Quotes " and slash \ and café'
        metadata['commands'][0]['summary'] = summary
        path.write_text(json.dumps(metadata))
        self.cli('pkg', 'install', str(source))
        raw = self.cli('--commands-json').stdout
        entries = json.loads(raw)['commands']
        self.assertEqual([e['summary'] for e in entries if e['name'] == 'hello'],
                         [summary])
        self.assertEqual(self.cli('--commands-json').stdout, raw)
        self.cli('help', 'hello')


if __name__ == '__main__':
    unittest.main(verbosity=2)
