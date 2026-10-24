"""Isolated package CLI contract checks. No Python package runtime."""
import os
import fcntl
import json
import shutil
import stat
import resource
import select
import random
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

PROGRAM = str(Path(sys.argv.pop(1)).resolve())
HELP = b'''Manage local directory packages.
Usage: pkg -h | --help
       pkg list
       pkg info NAME
       pkg install [--] DIRECTORY
       pkg remove [--] NAME
Install does not build or run packages. No downloads or dependencies.
Use remove then install to update. Partial removal can leave a record.
Status: 0 success; 1 state, validation or I/O failure; 2 syntax error.
'''


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        self.env = {'HOME': str(self.home), 'LC_ALL': 'C', 'PATH': '/usr/bin:/bin'}

    def run_pkg(self, *args, status=0, out=None, **kwargs):
        result = subprocess.run([PROGRAM, *args], env=self.env,
                                capture_output=True, timeout=5, **kwargs)
        self.assertEqual(result.returncode, status, result)
        if out is not None:
            self.assertEqual(result.stdout, out)
        if status:
            self.assertTrue(result.stderr.startswith(b'pkg: '), result)
        else:
            self.assertEqual(result.stderr, b'')
        return result

    def test_json_invalid(self):
        binary = str(Path(PROGRAM).with_name('json'))
        samples = [b'', b'{}x', b'\xef\xbb\xbf{}', b'{"x":1,"x":2}',
                   b'{"x":1,"\\u0078":2}', b'01', b'1.', b'1e',
                   b'NaN', b'[1,]', b'{"x":}', b'{"x" 1}',
                   b'"\\u0000"', b'"\\uD800"', b'"\\uDC00"',
                   b'"\\uD800\\u1234"', b'"\x00"', b'"\xff"',
                   b'"\xc0\x80"', b'"\xed\xa0\x80"', b'"\xf4\x90\x80\x80"',
                   b'[' * 17 + b']' * 17, b' ' * 262145]
        for raw in samples:
            result = subprocess.run([binary], input=raw, capture_output=True)
            self.assertEqual(result.returncode, 1, raw[:80])

    def test_json_valid(self):
        binary = str(Path(PROGRAM).with_name('json'))
        for raw in (b'{}', b'[null,true,false,1,-2.5e+10,"x"]',
                    b'{"a":{"b":["\\uD834\\uDD1E", "' + 'é'.encode() + b'"]}}',
                    b'[' * 16 + b']' * 16):
            result = subprocess.run([binary], input=raw, capture_output=True)
            self.assertEqual(result.returncode, 0, raw)

    def test_empty_list(self):
        self.run_pkg('list', out=b'')
        self.assertEqual(list(self.home.iterdir()), [])
        self.run_pkg('info', 'missing', status=1, out=b'')
        self.run_pkg('remove', 'missing', status=1, out=b'')
        self.assertEqual(list(self.home.iterdir()), [])

    def test_cli_invalid(self):
        for args in ((), ('list', 'extra'), ('--help', 'bad'),
                     ('info',), ('info', '--', 'name'), ('info', 'Bad'),
                     ('remove', '../escape'), ('install', '-h-'),
                     ('install',), ('list', '--help'), ('unknown',)):
            self.run_pkg(*args, status=2, out=b'')
        self.assertEqual(list(self.home.iterdir()), [])

    def test_root_and_lock(self):
        root = self.home / '.mysh' / 'pkgs'
        root.mkdir(parents=True, mode=0o700)
        root.parent.chmod(0o700)
        self.run_pkg('list', out=b'')
        lock = root / '.lock'
        self.assertTrue(lock.is_file(), 'reader must create the persistent lock')
        inode = lock.stat().st_ino
        self.assertEqual(stat.S_IMODE(lock.stat().st_mode), 0o600)
        with lock.open('r+') as holder:
            fcntl.lockf(holder, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.run_pkg('list', status=1, out=b'')
            self.run_pkg('info', 'missing', status=1, out=b'')
        self.run_pkg('list', out=b'')
        self.assertEqual(lock.stat().st_ino, inode)
        root.chmod(0o777)
        self.run_pkg('list', status=1, out=b'')
        root.chmod(0o700)
        lock.unlink()
        lock.symlink_to(self.home / 'outside')
        self.run_pkg('list', status=1, out=b'')
        self.assertFalse((self.home / 'outside').exists())

    def test_home_invalid(self):
        for value in (None, '', 'relative'):
            if value is None:
                self.env.pop('HOME', None)
            else:
                self.env['HOME'] = value
            self.run_pkg('list', status=1, out=b'')
        self.assertEqual(list(self.home.iterdir()), [])

    def package(self, name='hello'):
        source = self.home / ('source-' + name)
        shutil.copytree(Path(__file__).resolve().parents[1] /
                        'build/legacy-packages/hello-0.1.0', source)
        if name != 'hello':
            manifest = json.loads((source / 'pkg.json').read_text())
            manifest['name'] = name
            manifest['commands'] = [name]
            manifest['docs'] = ['docs/' + name + '.md']
            manifest['files'] = ['bin/' + name, 'docs/' + name + '.md',
                                 'metadata/commands.json']
            (source / 'bin/hello').rename(source / ('bin/' + name))
            (source / 'docs/hello.md').rename(source / ('docs/' + name + '.md'))
            metadata = json.loads((source / 'metadata/commands.json').read_text())
            metadata['package'] = name
            metadata['commands'][0]['name'] = name
            (source / 'metadata/commands.json').write_text(json.dumps(metadata))
            (source / 'pkg.json').write_text(json.dumps(manifest))
        return source

    def fingerprint(self, root):
        result = {}
        for path in [root, *sorted(root.rglob('*'))]:
            st = path.lstat()
            result[str(path.relative_to(root))] = (
                st.st_dev, st.st_ino, st.st_mode, st.st_nlink, st.st_size,
                st.st_mtime_ns, st.st_ctime_ns,
                os.readlink(path) if path.is_symlink() else
                path.read_bytes() if path.is_file() else None)
        return result

    def test_source_contains_home_no_mutation(self):
        source = self.package()
        for suffix in ('', 'nested', 'nested/.mysh/pkgs'):
            with self.subTest(suffix=suffix):
                home = source / suffix
                home.mkdir(parents=True, exist_ok=True)
                self.env['HOME'] = str(home)
                before = self.fingerprint(source)
                self.run_pkg('install', str(source), status=1, out=b'')
                self.assertEqual(self.fingerprint(source), before,
                                 'overlap rejection mutated source')

    def test_source_inside_managed_root_no_mutation(self):
        source = self.package()
        for index, suffix in enumerate(('', 'candidate')):
            with self.subTest(suffix=suffix):
                home = self.home / str(index)
                root = home / '.mysh/pkgs'
                root.mkdir(parents=True, mode=0o700)
                root.parent.chmod(0o700)
                self.env['HOME'] = str(home)
                target = root / suffix
                if suffix:
                    shutil.copytree(source, target)
                else:
                    shutil.copytree(source, target, dirs_exist_ok=True)
                before = self.fingerprint(root)
                self.run_pkg('install', str(target), status=1, out=b'')
                self.assertEqual(self.fingerprint(root), before,
                                 'overlap rejection mutated root/source')

    def test_physical_overlap_and_symlink_rejection(self):
        source = self.package()
        managed = self.home / 'managed'
        root = managed / '.mysh/pkgs'
        root.mkdir(parents=True, mode=0o700)
        root.parent.chmod(0o700)
        self.env['HOME'] = str(managed)
        for target in (managed / '.mysh', root):
            before = self.fingerprint(managed)
            self.run_pkg('install', str(target), status=1, out=b'')
            self.assertEqual(self.fingerprint(managed), before)
        self.run_pkg('install', str(source))
        installed = root / 'hello-0.1.0'
        for spelling in (str(installed), str(root / '../pkgs/hello-0.1.0'),
                         './managed/.mysh/pkgs/hello-0.1.0'):
            before = self.fingerprint(managed)
            self.run_pkg('install', spelling, status=1, out=b'', cwd=self.home)
            self.assertEqual(self.fingerprint(managed), before)
        self.run_pkg('remove', 'hello')
        for link_kind in ('source', 'home', 'mysh', 'root'):
            with self.subTest(link_kind=link_kind):
                fixture = self.home / link_kind
                fixture.mkdir()
                home = fixture / 'home'
                home.mkdir()
                path = str(source)
                if link_kind == 'source':
                    (fixture / 'link').symlink_to(source, target_is_directory=True)
                    path = str(fixture / 'link') + '/.'
                elif link_kind == 'home':
                    (fixture / 'link').symlink_to(home, target_is_directory=True)
                    home = fixture / 'link'
                elif link_kind == 'mysh':
                    (home / '.mysh').symlink_to(source, target_is_directory=True)
                else:
                    (home / '.mysh').mkdir()
                    (home / '.mysh/pkgs').symlink_to(source,
                                                   target_is_directory=True)
                self.env['HOME'] = str(home)
                before = self.fingerprint(fixture)
                source_before = self.fingerprint(source)
                self.run_pkg('install', path, status=1, out=b'')
                self.assertEqual(self.fingerprint(fixture), before)
                self.assertEqual(self.fingerprint(source), source_before)

    def test_separate_name_version_alias_bounds(self):
        source = self.package('a' * 60)
        manifest_path = source / 'pkg.json'
        metadata_path = source / 'metadata/commands.json'
        manifest = json.loads(manifest_path.read_text())
        metadata = json.loads(metadata_path.read_text())
        package_name = 'p' * 63
        manifest['name'] = metadata['package'] = package_name
        manifest['version'] = metadata['version'] = '65535.65535.65535'
        metadata['commands'][0]['options'][0]['long'] = ['l' * 63]
        manifest_path.write_text(json.dumps(manifest))
        metadata_path.write_text(json.dumps(metadata))
        before = self.fingerprint(source)
        self.run_pkg('install', str(source))
        self.run_pkg('info', package_name)
        self.run_pkg('remove', package_name)
        self.assertEqual(self.fingerprint(source), before)
        for field, value in [('name', 'p' * 64), ('name', 'Upper'),
                             ('name', ' leading'), ('name', 'trailing '),
                             ('version', '01.0.0'), ('version', '65536.0.0'),
                             ('version', '1.2.3 '), ('version', '+1.2.3'),
                             ('version', '1.2.3-beta')]:
            with self.subTest(field=field, value=value):
                bad = dict(manifest, **{field: value})
                manifest_path.write_text(json.dumps(bad))
                before = self.fingerprint(source)
                self.run_pkg('install', str(source), status=1, out=b'')
                self.assertEqual(self.fingerprint(source), before)
        manifest_path.write_text(json.dumps(manifest))
        metadata['commands'][0]['options'][0]['long'] = ['l' * 64]
        metadata_path.write_text(json.dumps(metadata))
        before = self.fingerprint(source)
        self.run_pkg('install', str(source), status=1, out=b'')
        self.assertEqual(self.fingerprint(source), before)
        for n in range(61, 65):
            too_long = self.package('b' * n)
            before = self.fingerprint(too_long)
            self.run_pkg('install', str(too_long), status=1, out=b'')
            self.assertEqual(self.fingerprint(too_long), before)

    def test_openat_variadic_forwarding(self):
        binary = str(Path(PROGRAM).with_name('openat'))
        result = subprocess.run([binary, str(self.home)], env=self.env,
                                capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result)
        self.assertEqual(result.stderr, b'')
        self.assertEqual(list(self.home.iterdir()), [])

    def test_install_hello(self):
        source = self.package()
        before = {str(p.relative_to(source)): p.read_bytes()
                  for p in source.rglob('*') if p.is_file()}
        self.run_pkg('install', str(source), out=b'Installed hello 0.1.0\n')
        root = self.home / '.mysh/pkgs/hello-0.1.0'
        self.assertEqual({str(p.relative_to(root)): p.read_bytes()
                          for p in root.rglob('*') if p.is_file()}, before)
        self.assertEqual({str(p.relative_to(source)): p.read_bytes()
                          for p in source.rglob('*') if p.is_file()}, before)
        for p in root.rglob('*'):
            mode = 0o700 if p.is_dir() or p.parent.name == 'bin' else 0o600
            self.assertEqual(stat.S_IMODE(p.stat().st_mode), mode)
        executed = subprocess.run([str(root / 'bin/hello')], env=self.env,
                                  capture_output=True, timeout=5)
        self.assertEqual((executed.returncode, executed.stdout, executed.stderr),
                         (0, b'Hello, world!\n', b''))
        self.run_pkg('list', out=b'hello 0.1.0\n')
        self.run_pkg('info', 'hello', out=b'Name: hello\nVersion: 0.1.0\n'
                     b'Description: Print a greeting.\nCommands: hello\n')
        self.run_pkg('install', str(source), status=1, out=b'')

    def test_install_manifest_reject(self):
        source = self.package()
        path = source / 'pkg.json'
        original = path.read_bytes()
        manifest = json.loads(original)
        variants = []
        for key in manifest:
            bad = dict(manifest)
            del bad[key]
            variants.append(json.dumps(bad).encode())
        for key, value in [('extra', 1), ('schema_version', True),
                           ('schema_version', 1.0), ('name', 'Bad'),
                           ('version', '01.0.0'), ('version', '65536.0.0'),
                           ('description', 'bad\ntext'), ('dependencies', ['x']),
                           ('files', ['../outside']), ('commands', ['hello'] * 2),
                           ('docs', []), ('files', manifest['files'] + ['pkg.json'])]:
            bad = dict(manifest)
            bad[key] = value
            variants.append(json.dumps(bad).encode())
        variants += [original + b'x', b'\xef\xbb\xbf' + original,
                     original.replace(b'"hello"', b'"\\uD800"', 1),
                     original.replace(b'"name":', b'"name":"x", "name":', 1)]
        for raw in variants:
            path.write_bytes(raw)
            self.run_pkg('install', str(source), status=1, out=b'')
            self.assertFalse((self.home / '.mysh/pkgs/hello-0.1.0').exists())
        path.write_bytes(original)
        self.run_pkg('install', str(source), out=b'Installed hello 0.1.0\n')

    def test_install_metadata_reject(self):
        source = self.package()
        path = source / 'metadata/commands.json'
        original = path.read_bytes()
        base = json.loads(original)
        variants = []
        for key in base:
            bad = json.loads(original)
            del bad[key]
            variants.append(bad)
        for key, value in [('package', 'other'), ('version', '0.2.0'),
                           ('commands', []), ('schema_version', 1.0)]:
            bad = json.loads(original)
            bad[key] = value
            variants.append(bad)
        for key in base['commands'][0]:
            bad = json.loads(original)
            del bad['commands'][0][key]
            variants.append(bad)
        for key, value in [('summary', ''), ('usage', 'x\n'),
                           ('name', 'other'), ('long_help', '\x7f')]:
            bad = json.loads(original)
            bad['commands'][0][key] = value
            variants.append(bad)
        for key, value in [('min_count', -1), ('max_count', 0),
                           ('value', 'invalid'), ('long', ['name', 'name']),
                           ('short', ['?']), ('metavar', '')]:
            bad = json.loads(original)
            entry = bad['commands'][0]['options'][0]
            entry[key] = value
            if key == 'max_count':
                entry['min_count'] = 1
            variants.append(bad)
        for value in variants:
            path.write_text(json.dumps(value))
            self.run_pkg('install', str(source), status=1, out=b'')
        path.write_bytes(original)
        self.run_pkg('install', str(source), out=b'Installed hello 0.1.0\n')

    def test_remove_lifecycle(self):
        source = self.package()
        self.run_pkg('install', str(source), out=b'Installed hello 0.1.0\n')
        root = self.home / '.mysh/pkgs'
        lock = (root / '.lock').stat().st_ino
        self.run_pkg('remove', 'hello', out=b'Removed hello 0.1.0\n')
        self.assertEqual([p.name for p in root.iterdir()], ['.lock'])
        self.assertEqual((root / '.lock').stat().st_ino, lock)
        self.run_pkg('list', out=b'')
        self.run_pkg('info', 'hello', status=1, out=b'')
        self.run_pkg('install', str(source), out=b'Installed hello 0.1.0\n')
        self.run_pkg('remove', '--', 'hello', out=b'Removed hello 0.1.0\n')
        self.assertTrue((source / 'pkg.json').is_file())

    def test_remove_two_partial_packages(self):
        a = self.package('alpha')
        b = self.package('beta')
        root = self.home / '.mysh/pkgs'
        for order in [('alpha', 'beta'), ('beta', 'alpha')]:
            for name, source in [('alpha', a), ('beta', b)]:
                self.run_pkg('install', str(source),
                             out=('Installed ' + name + ' 0.1.0\n').encode())
            for name in ('alpha', 'beta'):
                package = root / (name + '-0.1.0')
                (package / ('bin/' + name)).unlink()
                (package / 'metadata/commands.json').unlink()
            self.run_pkg('list', status=1, out=b'')
            self.run_pkg('install', str(a), status=1, out=b'')
            for name in order:
                self.run_pkg('remove', name,
                             out=('Removed ' + name + ' 0.1.0\n').encode())
            self.run_pkg('list', out=b'')

    def test_remove_unlisted_preserves_record(self):
        source = self.package()
        self.run_pkg('install', str(source))
        package = self.home / '.mysh/pkgs/hello-0.1.0'
        extra = package / 'extra'
        extra.write_bytes(b'sentinel')
        manifest = (package / 'pkg.json').read_bytes()
        self.run_pkg('remove', 'hello', status=1, out=b'')
        self.assertEqual(extra.read_bytes(), b'sentinel')
        self.assertEqual((package / 'pkg.json').read_bytes(), manifest)
        extra.unlink()
        self.run_pkg('remove', 'hello', out=b'Removed hello 0.1.0\n')

    def test_json_bounded_memory(self):
        binary = str(Path(PROGRAM).with_name('json'))
        raw = b'[' + b'"x",' * 60000 + b'"x"]'
        def limit_memory():
            resource.setrlimit(resource.RLIMIT_AS, (128 << 20, 128 << 20))
        result = subprocess.run([binary], input=raw, capture_output=True,
                                preexec_fn=limit_memory, timeout=5)
        self.assertEqual(result.returncode, 0, 'valid 240005-byte bounded input')

    def test_source_safety(self):
        source = self.package()
        leaf = source / 'bin/hello'
        raw = leaf.read_bytes()
        outside = self.home / 'outside'
        outside.write_bytes(b'sentinel')
        outside.chmod(0o700)
        for kind in ('link', 'fifo', 'hardlink', 'noexec', 'unlisted', 'directory'):
            if kind in ('link', 'fifo', 'hardlink', 'directory'):
                leaf.unlink()
            if kind == 'link':
                leaf.symlink_to(outside)
            elif kind == 'fifo':
                os.mkfifo(leaf)
            elif kind == 'hardlink':
                os.link(outside, leaf)
            elif kind == 'directory':
                leaf.mkdir()
            elif kind == 'noexec':
                leaf.chmod(0o600)
            else:
                (source / 'extra').write_bytes(b'extra')
            self.run_pkg('install', str(source), status=1, out=b'')
            self.assertEqual(outside.read_bytes(), b'sentinel')
            if kind == 'directory':
                leaf.rmdir()
            elif kind in ('link', 'fifo', 'hardlink'):
                leaf.unlink()
            elif kind == 'unlisted':
                (source / 'extra').unlink()
            if not leaf.exists():
                leaf.write_bytes(raw)
            leaf.chmod(0o700)
        alias = self.home / 'alias'
        alias.symlink_to(source, target_is_directory=True)
        self.run_pkg('install', str(alias), status=1, out=b'')
        self.run_pkg('install', str(alias / 'bin/..'), status=1, out=b'')
        self.run_pkg('install', str(source), out=b'Installed hello 0.1.0\n')

    def test_support_file_and_sorted_state(self):
        source = self.package('zulu')
        support = source / 'support'
        support.mkdir()
        (support / 'data.txt').write_bytes(b'support\x00bytes')
        path = source / 'pkg.json'
        manifest = json.loads(path.read_text())
        manifest['files'].append('support/data.txt')
        manifest['files'].sort()
        path.write_text(json.dumps(manifest))
        self.run_pkg('install', str(source))
        alpha = self.package('alpha')
        self.run_pkg('install', str(alpha))
        self.run_pkg('list', out=b'alpha 0.1.0\nzulu 0.1.0\n')
        self.assertEqual((self.home / '.mysh/pkgs/zulu-0.1.0/support/data.txt'
                          ).read_bytes(), b'support\x00bytes')
        self.run_pkg('remove', 'zulu')
        self.run_pkg('remove', 'alpha')

    def test_reserved_collisions(self):
        for name in ('cat', 'help', 'exit', 'pkg'):
            source = self.package(name)
            self.run_pkg('install', str(source), status=1, out=b'')
        source = self.package()
        self.run_pkg('install', str(source))
        before = (self.home / '.mysh/pkgs/hello-0.1.0/pkg.json').read_bytes()
        manifest = json.loads((source / 'pkg.json').read_text())
        manifest['version'] = '0.2.0'
        (source / 'pkg.json').write_text(json.dumps(manifest))
        metadata = json.loads((source / 'metadata/commands.json').read_text())
        metadata['version'] = '0.2.0'
        (source / 'metadata/commands.json').write_text(json.dumps(metadata))
        self.run_pkg('install', str(source), status=1, out=b'')
        self.assertEqual((self.home / '.mysh/pkgs/hello-0.1.0/pkg.json').read_bytes(),
                         before)

    def test_corrupt_records_and_stale_stages(self):
        source = self.package()
        self.run_pkg('install', str(source))
        root = self.home / '.mysh/pkgs'
        package = root / 'hello-0.1.0'
        for relative in ('pkg.json', 'metadata/commands.json', 'bin/hello'):
            path = package / relative
            original = path.read_bytes()
            path.write_bytes(b'broken')
            if relative == 'bin/hello':
                path.chmod(0o600)
            self.run_pkg('list', status=1, out=b'')
            self.run_pkg('info', 'hello', status=1, out=b'')
            self.run_pkg('remove', 'hello', status=1, out=b'')
            self.run_pkg('install', str(source), status=1, out=b'')
            path.write_bytes(original)
            path.chmod(0o700 if relative == 'bin/hello' else 0o600)
        (root / '.tmp-crash').mkdir()
        (root / '.tmp-crash/sentinel').write_bytes(b'stale')
        self.run_pkg('list', out=b'hello 0.1.0\n')
        stale = self.run_pkg('remove', 'hello', status=1, out=b'')
        self.assertIn(b'stale stage', stale.stderr)
        self.run_pkg('install', str(source), status=1, out=b'')
        self.assertEqual((root / '.tmp-crash/sentinel').read_bytes(), b'stale')
        shutil.rmtree(root / '.tmp-crash')
        package.rename(root / 'wrong-0.1.0')
        self.run_pkg('list', status=1, out=b'')

    def test_fault_install_rollback_and_recovery(self):
        source = self.package()
        root = self.home / '.mysh/pkgs'
        for fault in ('open', 'write', 'zero', 'read', 'close', 'rename',
                      'mutate-source'):
            self.env['PKG_FAULT'] = fault
            self.run_pkg('install', str(source), status=1, out=b'')
            self.assertFalse((root / 'hello-0.1.0').exists(), fault)
            self.assertEqual([p.name for p in root.iterdir()], ['.lock'], fault)
        for fault in ('short', 'eintr', 'read-eintr', 'short-read'):
            self.env['PKG_FAULT'] = fault
            self.run_pkg('install', str(source), out=b'Installed hello 0.1.0\n')
            del self.env['PKG_FAULT']
            self.run_pkg('remove', 'hello', out=b'Removed hello 0.1.0\n')
        self.env['PKG_FAULT'] = 'cleanup'
        result = self.run_pkg('install', str(source), status=1, out=b'')
        self.assertIn(b'cleanup failed', result.stderr)
        self.assertFalse((root / 'hello-0.1.0').exists())
        stages = [p for p in root.iterdir() if p.name.startswith('.tmp-')]
        self.assertEqual(len(stages), 1)
        del self.env['PKG_FAULT']
        self.run_pkg('list', out=b'')
        self.run_pkg('install', str(source), status=1, out=b'')

    def test_fault_remove_restore(self):
        source = self.package()
        root = self.home / '.mysh/pkgs/hello-0.1.0'
        self.run_pkg('install', str(source))
        self.env['PKG_FAULT'] = 'unlink'
        self.run_pkg('remove', 'hello', status=1, out=b'')
        self.assertTrue((root / 'pkg.json').is_file())
        for fault, diagnostic, has_record in (
                ('rmdir', b'manifest restored', True),
                ('restore', b'manifest restoration failed', False)):
            self.env['PKG_FAULT'] = fault
            result = self.run_pkg('remove', 'hello', status=1, out=b'')
            self.assertIn(diagnostic, result.stderr)
            self.assertEqual((root / 'pkg.json').is_file(), has_record)
        del self.env['PKG_FAULT']
        self.run_pkg('list', status=1, out=b'')

    def test_output_failure_commit_boundary(self):
        source = self.package()
        root = self.home / '.mysh/pkgs/hello-0.1.0'
        with open('/dev/full', 'wb') as output:
            def fail_output(*args):
                result = subprocess.run([PROGRAM, *args], env=self.env,
                                        stdout=output, stderr=subprocess.PIPE,
                                        timeout=5)
                self.assertEqual(result.returncode, 1)
                self.assertIn(b'output failure', result.stderr)
                return result
            fail_output('--help')
            result = fail_output('install', str(source))
            self.assertIn(b'committed', result.stderr)
            self.assertTrue((root / 'pkg.json').is_file())
            fail_output('list')
            fail_output('info', 'hello')
            result = fail_output('remove', 'hello')
            self.assertIn(b'committed', result.stderr)
            self.assertFalse(root.exists())

    def test_allocation_failures(self):
        source = self.package()
        root = self.home / '.mysh/pkgs'
        for ordinal in range(1000):
            self.env['PKG_ALLOC_FAIL'] = str(ordinal)
            result = subprocess.run([PROGRAM, 'install', str(source)],
                                    env=self.env, capture_output=True, timeout=5)
            self.assertIn(result.returncode, (0, 1), (ordinal, result))
            self.env.pop('PKG_ALLOC_FAIL')
            if result.returncode == 0:
                self.run_pkg('list', out=b'hello 0.1.0\n')
                self.run_pkg('remove', 'hello')
                self.assertGreater(ordinal, 100)
                break
            else:
                self.assertTrue(result.stderr.startswith(b'pkg: '))
                self.assertFalse((root / 'hello-0.1.0').exists())
                # Allocation failures in cleanup may leave a hidden stage.
                for stage in root.glob('.tmp-*'):
                    self.assertIn(b'cleanup failed', result.stderr)
                    shutil.rmtree(stage)
        else:
            self.fail('No successful allocation ordinal below 1000')

    def test_module_isolation(self):
        source = self.package()
        module = str(Path(PROGRAM).with_name('module'))
        result = subprocess.run([module, str(source)], env=self.env,
                                capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result)

    def test_parser_constructor_failure(self):
        self.env['PKG_FAULT'] = 'parser'
        self.run_pkg('--help', status=1, out=b'')
        self.assertEqual(list(self.home.iterdir()), [])

    def test_multi_command_prefix_docs_order(self):
        source = self.package('multi')
        manifest = json.loads((source / 'pkg.json').read_text())
        metadata = json.loads((source / 'metadata/commands.json').read_text())
        command = metadata['commands'][0]
        binary = (source / 'bin/multi').read_bytes()
        (source / 'bin/multi').unlink()
        (source / 'docs/multi.md').unlink()
        manifest['commands'] = ['a', 'a-a']
        manifest['docs'] = ['docs/a-a.md', 'docs/a.md']
        manifest['files'] = sorted(['bin/a', 'bin/a-a', *manifest['docs'],
                                    'metadata/commands.json'])
        metadata['commands'] = []
        for name in manifest['commands']:
            (source / ('bin/' + name)).write_bytes(binary)
            (source / ('bin/' + name)).chmod(0o700)
            (source / ('docs/' + name + '.md')).write_bytes(b'generated doc\n')
            entry = dict(command)
            entry['name'] = name
            metadata['commands'].append(entry)
        (source / 'pkg.json').write_text(json.dumps(manifest))
        (source / 'metadata/commands.json').write_text(json.dumps(metadata))
        self.run_pkg('install', str(source), out=b'Installed multi 0.1.0\n')
        self.run_pkg('info', 'multi', out=b'Name: multi\nVersion: 0.1.0\n'
                     b'Description: Print a greeting.\nCommands: a a-a\n')
        self.run_pkg('remove', 'multi', out=b'Removed multi 0.1.0\n')

    def test_negative_zero_metadata_count(self):
        source = self.package()
        path = source / 'metadata/commands.json'
        raw = path.read_bytes().replace(b'"min_count": 0', b'"min_count": -0')
        path.write_bytes(raw)
        self.run_pkg('install', str(source), out=b'Installed hello 0.1.0\n')

    def test_json_file_and_path_limits(self):
        source = self.package()
        manifest_path = source / 'pkg.json'
        metadata_path = source / 'metadata/commands.json'
        original_manifest = manifest_path.read_bytes()
        original_metadata = metadata_path.read_bytes()
        manifest_path.write_bytes(original_manifest +
                                  b' ' * (65536 - len(original_manifest)))
        metadata_path.write_bytes(original_metadata +
                                  b' ' * (262144 - len(original_metadata)))
        self.run_pkg('install', str(source))
        self.run_pkg('remove', 'hello')
        manifest_path.write_bytes(manifest_path.read_bytes() + b' ')
        self.run_pkg('install', str(source), status=1, out=b'')
        manifest_path.write_bytes(original_manifest)
        metadata_path.write_bytes(metadata_path.read_bytes() + b' ')
        self.run_pkg('install', str(source), status=1, out=b'')
        metadata_path.write_bytes(original_metadata)
        manifest = json.loads(original_manifest)
        long_path = '/'.join(['a' * 63] * 4)
        self.assertEqual(len(long_path), 255)
        path = source / long_path
        path.parent.mkdir(parents=True)
        path.write_bytes(b'bounded')
        manifest['files'] = sorted([*manifest['files'], long_path])
        manifest_path.write_text(json.dumps(manifest))
        self.run_pkg('install', str(source))
        self.run_pkg('remove', 'hello')
        manifest['files'] = sorted([*manifest['files'], long_path + 'b'])
        manifest_path.write_text(json.dumps(manifest))
        self.run_pkg('install', str(source), status=1, out=b'')

    def test_public_text_and_file_count_limits(self):
        source = self.package()
        path = source / 'pkg.json'
        manifest = json.loads(path.read_text())
        manifest['description'] = 'é' * 128
        for i in range(61):
            relative = 'support' + str(i).zfill(2)
            (source / relative).write_bytes(b'payload')
            manifest['files'].append(relative)
        manifest['files'].sort()
        self.assertEqual(len(manifest['files']), 64)
        path.write_text(json.dumps(manifest))
        self.run_pkg('install', str(source))
        self.run_pkg('remove', 'hello')
        manifest['description'] += 'x'
        path.write_text(json.dumps(manifest))
        self.run_pkg('install', str(source), status=1, out=b'')
        manifest['description'] = 'valid'
        manifest['files'].append('support99')
        path.write_text(json.dumps(manifest))
        (source / 'support99').write_bytes(b'over')
        self.run_pkg('install', str(source), status=1, out=b'')

    def test_process_crash_and_lock_recovery(self):
        source = self.package()
        root = self.home / '.mysh/pkgs'
        for fault in ('before-rename', 'after-rename'):
            reader, writer = os.pipe()
            env = dict(self.env, PKG_FAULT=fault, PKG_READY=str(writer))
            process = subprocess.Popen([PROGRAM, 'install', str(source)],
                                       env=env, pass_fds=(writer,),
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE)
            os.close(writer)
            try:
                self.assertTrue(select.select([reader], [], [], 5)[0])
                self.assertEqual(os.read(reader, 5), b'ready')
                inode = (root / '.lock').stat().st_ino
                self.run_pkg('list', status=1, out=b'')
                self.run_pkg('info', 'hello', status=1, out=b'')
                self.run_pkg('install', str(source), status=1, out=b'')
                self.run_pkg('remove', 'hello', status=1, out=b'')
            finally:
                process.kill()
                process.communicate(timeout=5)
                os.close(reader)
            self.assertEqual((root / '.lock').stat().st_ino, inode)
            if fault == 'before-rename':
                self.assertFalse((root / 'hello-0.1.0').exists())
                self.run_pkg('list', out=b'')
                self.run_pkg('install', str(source), status=1, out=b'')
                for stage in root.glob('.tmp-*'):
                    shutil.rmtree(stage)
            else:
                self.run_pkg('list', out=b'hello 0.1.0\n')
                self.run_pkg('remove', 'hello')

    def test_json_differential_bounded_fuzz(self):
        build = Path(__file__).resolve().parents[1] / 'scripts/build_package.py'
        spec = importlib.util.spec_from_file_location('package_build_oracle', build)
        assert spec is not None and spec.loader is not None
        oracle = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(oracle)
        binary = str(Path(PROGRAM).with_name('json-sanitize'))
        rng = random.Random(420)
        values = [None, True, False, -0.0, 12, -32, 1.25,
                  'texté𝄞', 'escaped\n\t\r\b\f"\\', '', '\0']
        corpus = [json.dumps(v, ensure_ascii=ascii_mode).encode()
                  for v in values for ascii_mode in (True, False)]
        corpus += [b'{"a":1,"\\u0061":2}', b'"\\uD834\\uDD1E"',
                   b'[' * 16 + b']' * 16, b'[' * 17 + b']' * 17]
        for _ in range(200):
            tree = {str(i): [rng.choice(values) for _ in range(rng.randrange(5))]
                    for i in range(rng.randrange(5))}
            raw = json.dumps(tree, ensure_ascii=rng.choice([True, False])).encode()
            if rng.randrange(2):
                at = rng.randrange(len(raw) + 1)
                raw = raw[:at] + bytes([rng.randrange(256)]) + raw[at:]
            corpus.append(raw)
        for raw in corpus:
            try:
                oracle.json_read(raw, 262144)
                expected = 0
            except (ValueError, UnicodeError):
                expected = 1
            result = subprocess.run([binary], input=raw, capture_output=True,
                                    timeout=5)
            self.assertEqual(result.returncode, expected, (raw, result.stderr))

    def synthetic_package(self, name, commands):
        source = self.home / ('fixture-' + name)
        (source / 'bin').mkdir(parents=True)
        (source / 'docs').mkdir()
        (source / 'metadata').mkdir()
        commands = sorted(commands)
        docs = sorted('docs/' + command + '.md' for command in commands)
        files = sorted([*docs, *('bin/' + command for command in commands),
                        'metadata/commands.json'])
        for command in commands:
            (source / ('bin/' + command)).write_bytes(b'x')
            (source / ('bin/' + command)).chmod(0o700)
            (source / ('docs/' + command + '.md')).write_bytes(b'd')
        manifest = dict(schema_version=1, name=name, version='65535.65535.65535',
                        description='D', commands=commands, files=files,
                        docs=docs, dependencies=[])
        metadata = dict(schema_version=1, package=name, version=manifest['version'],
                        commands=[dict(name=command, summary='S', long_help='',
                                       usage=command, arguments=[], options=[])
                                  for command in commands])
        (source / 'pkg.json').write_text(json.dumps(manifest))
        (source / 'metadata/commands.json').write_text(json.dumps(metadata))
        return source

    def test_package_count_limit(self):
        for i in range(64):
            name = 'p' + str(i).zfill(2)
            source = self.synthetic_package(name, ['cmd' + str(i).zfill(2)])
            self.run_pkg('install', str(source))
        expected = b''.join(('p' + str(i).zfill(2) +
                             ' 65535.65535.65535\n').encode() for i in range(64))
        self.run_pkg('list', out=expected)
        source = self.synthetic_package('overflow', ['extra'])
        self.run_pkg('install', str(source), status=1, out=b'')
        self.run_pkg('list', out=expected)

    def test_command_count_limits(self):
        for i in range(16):
            commands = ['cmd' + str(i).zfill(2) + str(j).zfill(2)
                        for j in range(16)]
            source = self.synthetic_package('p' + str(i).zfill(2), commands)
            self.run_pkg('install', str(source))
        source = self.synthetic_package('overflow', ['extra'])
        self.run_pkg('install', str(source), status=1, out=b'')
        # A separate root makes the per-package bound independent of totals.
        self.env['HOME'] = str(self.home / 'other-home')
        Path(self.env['HOME']).mkdir()
        source = self.synthetic_package('too-many', ['c' + str(i).zfill(2)
                                                     for i in range(17)])
        self.run_pkg('install', str(source), status=1, out=b'')

    def test_payload_byte_limits(self):
        source = self.synthetic_package('bytes', ['bytecmd'])
        path = source / 'pkg.json'
        manifest = json.loads(path.read_text())
        overhead = sum((source / relative).stat().st_size
                       for relative in manifest['files'])
        for i in range(4):
            relative = 'support' + str(i)
            with (source / relative).open('wb') as stream:
                stream.truncate(16777216 - (overhead if i == 3 else 0))
            manifest['files'].append(relative)
        manifest['files'].sort()
        path.write_text(json.dumps(manifest))
        self.run_pkg('install', str(source))
        self.run_pkg('remove', 'bytes')
        with (source / 'support3').open('ab') as stream:
            stream.write(b'x')
        self.run_pkg('install', str(source), status=1, out=b'')
        with (source / 'support0').open('ab') as stream:
            stream.write(b'x')
        self.run_pkg('install', str(source), status=1, out=b'')

    def test_restrictive_umask(self):
        source = self.package()
        result = subprocess.run([PROGRAM, 'install', str(source)], env=self.env,
                                capture_output=True, timeout=5,
                                preexec_fn=lambda: os.umask(0o777))
        self.assertEqual(result.returncode, 0, result)
        root = self.home / '.mysh/pkgs'
        self.assertEqual(stat.S_IMODE((root / '.lock').stat().st_mode), 0o600)
        for path in (root.parent, root, root / 'hello-0.1.0',
                     root / 'hello-0.1.0/bin'):
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o700)
        self.run_pkg('list', out=b'hello 0.1.0\n')
        self.run_pkg('remove', 'hello')

    def test_effective_name_and_metadata_text_limits(self):
        command = 'c' * 60
        source = self.synthetic_package('p' * 63, [command])
        path = source / 'metadata/commands.json'
        metadata = json.loads(path.read_text())
        entry = metadata['commands'][0]
        entry['summary'] = 'é' * 128
        entry['long_help'] = 'h' * 16384
        entry['usage'] = 'u' * 16384
        entry['arguments'] = [dict(metavar='v', description='d' * 1024,
                                    min_count=0, max_count=None)
                              for _ in range(64)]
        path.write_text(json.dumps(metadata))
        self.run_pkg('install', str(source))
        self.run_pkg('remove', 'p' * 63)
        for field, suffix in [('summary', 'x'), ('long_help', 'x'), ('usage', 'x')]:
            entry[field] += suffix
            path.write_text(json.dumps(metadata))
            self.run_pkg('install', str(source), status=1, out=b'')
            entry[field] = entry[field][:-1]
        entry['arguments'][0]['description'] += 'x'
        path.write_text(json.dumps(metadata))
        self.run_pkg('install', str(source), status=1, out=b'')
        entry['arguments'][0]['description'] = 'd'
        entry['arguments'].append(dict(metavar='v', description='',
                                       min_count=0, max_count=None))
        path.write_text(json.dumps(metadata))
        self.run_pkg('install', str(source), status=1, out=b'')

    def test_normalize_special_mode_bits(self):
        source = self.package()
        (source / 'bin/hello').chmod(0o6777)
        (source / 'docs/hello.md').chmod(0o6666)
        self.run_pkg('install', str(source))
        installed = self.home / '.mysh/pkgs/hello-0.1.0'
        self.assertEqual(stat.S_IMODE((installed / 'bin/hello').stat().st_mode),
                         0o700)
        self.assertEqual(stat.S_IMODE((installed / 'docs/hello.md').stat().st_mode),
                         0o600)
        self.assertEqual(stat.S_IMODE((source / 'bin/hello').stat().st_mode), 0o6777)

    def test_operand_separator_and_posix_environment(self):
        source = self.package()
        source.rename(self.home / '-directory')
        for correct in (None, '1'):
            if correct is None:
                self.env.pop('POSIXLY_CORRECT', None)
            else:
                self.env['POSIXLY_CORRECT'] = correct
            self.run_pkg('install', '-directory', status=2, out=b'', cwd=self.home)
            self.run_pkg('install', '--', '-directory', cwd=self.home,
                         out=b'Installed hello 0.1.0\n')
            self.run_pkg('remove', '--', 'hello', out=b'Removed hello 0.1.0\n')

    def test_publication_never_replaces_final_directory(self):
        source = self.package()
        self.env['PKG_FAULT'] = 'occupied-final'
        self.run_pkg('install', str(source), status=1, out=b'')
        root = self.home / '.mysh/pkgs'
        final = root / 'hello-0.1.0'
        self.assertTrue(final.is_dir())
        self.assertEqual(list(final.iterdir()), [])
        self.assertEqual(sorted(p.name for p in root.iterdir()),
                         ['.lock', 'hello-0.1.0'])

    def test_other_package_command_collision(self):
        source = self.package()
        self.run_pkg('install', str(source))
        other = self.package('other')
        manifest = json.loads((other / 'pkg.json').read_text())
        metadata = json.loads((other / 'metadata/commands.json').read_text())
        manifest['commands'] = ['hello']
        manifest['docs'] = ['docs/hello.md']
        manifest['files'] = ['bin/hello', 'docs/hello.md',
                             'metadata/commands.json']
        metadata['commands'][0]['name'] = 'hello'
        (other / 'bin/other').rename(other / 'bin/hello')
        (other / 'docs/other.md').rename(other / 'docs/hello.md')
        (other / 'pkg.json').write_text(json.dumps(manifest))
        (other / 'metadata/commands.json').write_text(json.dumps(metadata))
        self.run_pkg('install', str(other), status=1, out=b'')
        self.run_pkg('list', out=b'hello 0.1.0\n')
        # Administrative corruption must also fail before any removal unlink.
        installed = self.home / '.mysh/pkgs/other-0.1.0'
        shutil.copytree(other, installed)
        before = (self.home / '.mysh/pkgs/hello-0.1.0/bin/hello').read_bytes()
        self.run_pkg('remove', 'hello', status=1, out=b'')
        self.assertEqual((self.home / '.mysh/pkgs/hello-0.1.0/bin/hello').read_bytes(),
                         before)

    def test_help(self):
        for flag in ('-h', '--help'):
            self.run_pkg(flag, out=HELP)
        self.assertEqual(list(self.home.iterdir()), [])


if __name__ == '__main__':
    unittest.main()
