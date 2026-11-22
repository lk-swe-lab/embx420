"""Loader rejection, symbol boundaries, and native package negative checks."""
import fcntl
import json
import os
from pathlib import Path
import select
import shutil
import subprocess
import unittest
from test_shared_package import (SharedPackages, ROOT, PROGRAM, PACKAGE,
                                 compile_fixture)


class Loader(SharedPackages):
    def test_directory_enumeration_failure_is_not_reported_as_success(self):
        self.cli('pkg', 'install', str(PACKAGE))
        fault = self.base / 'fault.so'
        subprocess.run(['cc', '-std=c17', '-shared', '-fPIC',
                        'tests/loader_faults.c', '-ldl', '-o', str(fault)],
                       cwd=ROOT, check=True)
        preload = str(fault)
        needed = subprocess.check_output(['readelf', '-d', PROGRAM], text=True)
        if 'libasan' in needed:
            asan = subprocess.check_output(
                ['cc', '-print-file-name=libasan.so'], text=True).strip()
            preload = asan + ':' + preload
        self.env['LD_PRELOAD'] = preload
        self.env['AISHELL_FAIL_READDIR'] = '/.mysh/pkgs'
        result = self.cli('pkg', 'list', status=1)
        self.assertIn(b'inventory scan failed', result.stderr)
        self.env['AISHELL_FAIL_READDIR'] = '/.mysh/pkgs/.active'
        result = self.cli('pwd')
        self.assertIn(b'cannot enumerate active selections', result.stderr)
        self.assertEqual(result.stdout, os.fsencode(self.base) + b'\n')

    def test_install_normalizes_private_modes_under_restrictive_umask(self):
        result = subprocess.run([PROGRAM, 'pkg', 'install', str(PACKAGE)],
                                capture_output=True, env=self.env,
                                preexec_fn=lambda: os.umask(0o777), timeout=5)
        self.assertEqual(result.returncode, 0, result)
        root = self.home / '.mysh/pkgs'
        for path in (root, root / '.active', root / 'hello-0.2.0'):
            self.assertEqual(path.stat().st_mode & 0o777, 0o700)
        self.assertEqual(self.cli('hello').stdout, b'Hello, world!\n')

    def test_executable_only_package_is_explicitly_rejected(self):
        legacy = ROOT / 'build/legacy-packages/hello-0.1.0'
        result = self.cli('pkg', 'install', str(legacy), status=1)
        self.assertIn(b'executable-only schema-1 packages are unsupported',
                      result.stderr)
        self.cli('hello', status=127)

    def test_package_batch_rejection_is_atomic(self):
        for extra in ('cat', 'zzz'):
            with self.subTest(extra=extra):
                source = self.base / ('batch-' + extra)
                shutil.copytree(PACKAGE, source)
                names = sorted(['hello', extra])
                manifest_path = source / 'pkg.json'
                manifest = json.loads(manifest_path.read_bytes())
                manifest['commands'] = names
                manifest['docs'] = sorted('docs/' + n + '.md' for n in names)
                manifest['files'] = sorted([*manifest['docs'],
                    *('lib/' + n + '.so' for n in names),
                    'metadata/commands.json'])
                manifest_path.write_text(json.dumps(manifest))
                (source / ('docs/' + extra + '.md')).write_text('Fixture')
                extra_command = compile_fixture(source / ('lib/' + extra + '.so'),
                                '-DFIXTURE_NAME="' + extra + '"',
                                *(['-DFIXTURE_ABI'] if extra == 'zzz' else []))
                metadata_path = source / 'metadata/commands.json'
                metadata = json.loads(metadata_path.read_bytes())
                command = metadata['commands'][0]
                metadata['commands'] = sorted([command, extra_command],
                                              key=lambda c: c['name'])
                metadata_path.write_text(json.dumps(metadata))
                self.cli('pkg', 'install', str(source), status=1)
                self.cli('hello', status=127)
                self.assertFalse((self.home / '.mysh/pkgs/.active/hello').exists())
                self.assertFalse((self.home / '.mysh/pkgs/hello-0.2.0').exists())
                self.cli('cat', '--help')

    def test_selection_temporary_collision_preserves_unknown_bytes(self):
        proc = subprocess.Popen([PROGRAM], stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                env=self.env, cwd=self.base)
        active = self.home / '.mysh/pkgs/.active'
        active.mkdir(parents=True, mode=0o700)
        active.parent.chmod(0o700)
        active.parent.parent.chmod(0o700)
        unknown = active / f'tmp-{proc.pid}-1'
        unknown.write_bytes(b'human-owned bytes')
        try:
            out, err = proc.communicate(
                f'pkg install {PACKAGE}\nexit\n'.encode(), timeout=5)
            self.assertEqual(proc.returncode, 1, (out, err))
            self.assertTrue(unknown.exists(), 'unknown temp was deleted')
            self.assertEqual(unknown.read_bytes(), b'human-owned bytes')
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.communicate()

    def test_loader_rejects_without_partial_registry(self):
        release = self.base / 'release'
        release.mkdir()
        shutil.copy2(PROGRAM, release / 'aishell')
        shutil.copytree(PROGRAM.parent / 'modules', release / 'modules')
        target = release / 'modules/zzz-test.so'
        for mode in ('MISSING', 'ABI', 'SHORT', 'SPEC_SIZE', 'NULL_RUN',
                     'NULL', 'DUPLICATE'):
            with self.subTest(mode=mode):
                compile_fixture(target, '-DFIXTURE_' + mode,
                                '-DFIXTURE_NAME="sneak"')
                result = subprocess.run([release / 'aishell', '--commands-json'],
                                        capture_output=True, env=self.env,
                                        timeout=5)
                self.assertEqual(result.returncode, 0, result)
                names = [v['name'] for v in json.loads(result.stdout)['commands']]
                self.assertNotIn('sneak', names)
                self.assertEqual(len(names), 22)
                self.assertIn(b'module', result.stderr)

    def test_module_symbol_boundary(self):
        for module in [*sorted((ROOT / 'build/modules').glob('*.so')),
                       ROOT / 'build/example-modules/hello.so']:
            exported = subprocess.check_output(['nm', '-D', '--defined-only', module], text=True)
            self.assertEqual([line.split()[-1] for line in exported.splitlines()],
                             ['aishell_module_entry_v1'], module)
            undefined = subprocess.check_output(['nm', '-D', '--undefined-only', module], text=True)
            for forbidden in ('find_command', 'register_command', 'pthread', 'dlclose'):
                self.assertNotIn(forbidden, undefined, module)
        elf = subprocess.check_output(['readelf', '-d', PROGRAM], text=True)
        self.assertNotIn('argtable', elf)
        self.assertNotIn('RPATH', elf)
        self.assertNotIn('RUNPATH', elf)

    def test_real_single_thread_and_external_refresh(self):
        proc = subprocess.Popen([PROGRAM], stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                env=self.env, cwd=self.base)
        try:
            def line(command):
                proc.stdin.write(command + b'\n')
                proc.stdin.flush()
                self.assertTrue(select.select([proc.stdout], [], [], 5)[0])
                return proc.stdout.readline()
            self.assertEqual(line(b'pwd'), os.fsencode(self.base) + b'\n')
            self.assertEqual(len(list(Path(f'/proc/{proc.pid}/task').iterdir())), 1)
            self.cli('pkg', 'install', str(PACKAGE))
            self.assertEqual(line(b'hello'), b'Hello, world!\n')
            self.cli('pkg', 'remove', 'hello')
            proc.stdin.write(b'hello\nexit\n')
            out, err = proc.communicate(timeout=5)
            self.assertEqual((proc.returncode, out, err),
                             (127, b'', b'main: hello: unknown command\n'))
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.communicate()

    def test_native_argv_stdin_status_and_catalog_escape(self):
        source = self.changed_package('0.3.0')
        command = compile_fixture(source / 'lib/hello.so')
        path = source / 'metadata/commands.json'
        metadata = json.loads(path.read_bytes())
        metadata['commands'] = [command]
        path.write_text(json.dumps(metadata))
        self.cli('pkg', 'install', str(source))
        result = self.cli('hello', 'two words', '', '*', status=23)
        self.assertEqual(result.stdout, b'<two words>\n<>\n<*>\n')
        result = self.cli(data=b'hello --read\none data line\npwd\nexit\n')
        self.assertEqual(result.stdout, b'data:<one data line>\n' + os.fsencode(self.base) + b'\n')
        self.cli(data=b'hello\nexit\n', status=23)
        entry = next(e for e in json.loads(self.cli('--commands-json').stdout)['commands']
                     if e['name'] == 'hello')
        self.assertEqual(entry['summary'], 'Quotes " and slash \\ and UTF-8 café')
        self.assertEqual(self.cli('help', 'hello').stdout, b'Usage: fixture [ARGS...]\n')
        result = self.cli(data=b'cd /\nhello x\nexit\n', status=23)
        self.assertEqual(result.stdout, b'<x>\n')

    def test_shared_rejections_and_no_active_side_effects(self):
        for kind in ('missing-entry', 'invalid-abi', 'symlink', 'hardlink',
                     'fifo', 'unlisted', 'traversal', 'duplicate-json',
                     'manifest-module-mismatch', 'metadata-mismatch', 'schema1'):
            with self.subTest(kind=kind):
                source = self.base / kind
                shutil.copytree(PACKAGE, source)
                module = source / 'lib/hello.so'
                if kind in ('missing-entry', 'invalid-abi', 'manifest-module-mismatch'):
                    define = {'missing-entry': '-DFIXTURE_MISSING',
                              'invalid-abi': '-DFIXTURE_ABI',
                              'manifest-module-mismatch': '-DFIXTURE_NAME="other"'}[kind]
                    compile_fixture(module, define)
                elif kind in ('symlink', 'hardlink', 'fifo'):
                    module.unlink()
                    if kind == 'symlink':
                        module.symlink_to(PACKAGE / 'lib/hello.so')
                    elif kind == 'hardlink':
                        os.link(PACKAGE / 'lib/hello.so', module)
                    else:
                        os.mkfifo(module)
                elif kind == 'unlisted':
                    (source / 'hidden').write_text('unlisted')
                elif kind == 'duplicate-json':
                    raw = (source / 'pkg.json').read_bytes()
                    (source / 'pkg.json').write_bytes(raw[:-2] + b', "name":"hello"}')
                elif kind == 'metadata-mismatch':
                    p = source / 'metadata/commands.json'
                    value = json.loads(p.read_bytes())
                    value['version'] = '9.9.9'
                    p.write_text(json.dumps(value))
                else:
                    p = source / 'pkg.json'
                    value = json.loads(p.read_bytes())
                    if kind == 'schema1':
                        value['schema_version'] = 1
                    else:
                        value['files'].append('../escape')
                    p.write_text(json.dumps(value))
                self.cli('pkg', 'install', str(source), status=1)
                self.cli('hello', status=127)
                root = self.home / '.mysh/pkgs'
                self.assertFalse((root / 'hello-0.2.0').exists())
                self.assertFalse((root / '.active/hello').exists())
                if root.exists():
                    self.assertFalse(any(p.name.startswith('.tmp-') for p in root.iterdir()))
        self.cli('pwd')

    def test_state_links_and_lock_do_not_break_standard_modules(self):
        self.cli('pkg', 'install', str(PACKAGE))
        active = self.home / '.mysh/pkgs/.active/hello'
        external = self.base / 'outside'
        external.write_text('keep')
        active.unlink()
        active.symlink_to(external)
        self.cli('pkg', 'use', 'hello', '0.2.0', status=1)
        self.assertEqual(external.read_text(), 'keep')
        self.cli('pwd')
        lock = self.home / '.mysh/pkgs/.lock'
        with lock.open('r+') as holder:
            fcntl.lockf(holder, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.cli('pkg', 'list', status=1)
            self.cli('pwd')
            self.cli('help', 'pkg')

    def test_discovery_preserves_stdin_and_reports_output_error(self):
        self.cli('pkg', 'install', str(PACKAGE))
        data = self.base / 'input'
        data.write_bytes(b'untouched input\n')
        for args in (('--commands-json',), ('--help',), ('help', 'hello'),
                     ('pkg', 'list'), ('pkg', '--help')):
            with data.open('rb') as stream:
                result = subprocess.run([PROGRAM, *args], stdin=stream,
                                        capture_output=True, env=self.env, timeout=5)
                self.assertEqual(result.returncode, 0, result)
                self.assertEqual(stream.tell(), 0)
            with open('/dev/full', 'wb') as full:
                result = subprocess.run([PROGRAM, *args], stdout=full,
                                        stderr=subprocess.PIPE, env=self.env, timeout=5)
                self.assertEqual(result.returncode, 1, result)


if __name__ == '__main__':
    unittest.main()
