"""Independent package-byte and public-interface checks (stdlib only)."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import resource
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('builder', ROOT / 'scripts/build_package.py')
assert SPEC is not None and SPEC.loader is not None
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


class PackageBuild(unittest.TestCase):
    def run_fixture(self, manifest=None, source=None, exporter=None,
                    output_limit=False):
        source = source or self.source
        if manifest is not None:
            (source / 'pkg.json').write_text(json.dumps(manifest))
        return subprocess.run([
            'python3', str(ROOT / 'scripts/build_package.py'),
            '--manifest', str(source / 'pkg.json'),
            '--binary', str(ROOT / 'build/hello'),
            '--exporter', str(exporter or ROOT / 'build/export-hello'),
            '--output-root', str(self.output)], capture_output=True, timeout=15,
            preexec_fn=(lambda: resource.setrlimit(resource.RLIMIT_FSIZE,
                                                   (1024, 1024)))
            if output_limit else None)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.source = Path(self.temporary.name) / 'source'
        self.source.mkdir()
        self.output = Path(self.temporary.name) / 'output'
        self.manifest = json.loads((ROOT / 'packages/hello/pkg.json').read_bytes())
        (self.source / 'pkg.json').write_text(json.dumps(self.manifest))

    def assert_rejected(self, result):
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn(b'build-package:', result.stderr)
        self.assertEqual(result.stdout, b'')
        self.assertFalse(self.output.exists() and list(self.output.iterdir()))

    def test_reject_corrupt_manifests_before_publication(self):
        cases = [dict(self.manifest, schema_version=True),
                 dict(self.manifest, schema_version=2),
                 dict(self.manifest, unknown=1),
                 dict(self.manifest, dependencies=['other']),
                 dict(self.manifest, name='../escape'),
                 dict(self.manifest, version='01.0.0'),
                 dict(self.manifest, version='65536.0.0'),
                 dict(self.manifest, description='x' * 257),
                 dict(self.manifest, description='bad\x00text'),
                 dict(self.manifest, files=self.manifest['files'] + ['../bad']),
                 dict(self.manifest, files=self.manifest['files'] * 2),
                 dict(self.manifest, files=['bin/hello']),
                 dict(self.manifest, docs=[]),
                 dict(self.manifest, commands=['other']),
                 {k: v for k, v in self.manifest.items() if k != 'description'}]
        for manifest in cases:
            with self.subTest(manifest=manifest):
                self.assert_rejected(self.run_fixture(manifest))
        raw = json.dumps(self.manifest).encode()
        for content in [raw + b'{}', b'\xef\xbb\xbf' + raw,
                        raw[:-1] + b', "name": "hello"}',
                        raw.replace(b'Print a greeting.', b'\\ud800'),
                        raw.replace(b'Print a greeting.', b'\xff'),
                        b' ' * 65537, b'[' * 17 + b'0' + b']' * 17]:
            with self.subTest(content=content[:50]):
                (self.source / 'pkg.json').write_bytes(content)
                self.assert_rejected(self.run_fixture())

    def test_support_inventory_and_limits(self):
        manifest = dict(self.manifest, files=sorted(
            self.manifest['files'] + ['share/readme.txt']))
        (self.source / 'share').mkdir()
        support = self.source / 'share/readme.txt'
        support.write_bytes(b'Explicit regular support bytes.\n')
        result = self.run_fixture(manifest)
        self.assertEqual(result.returncode, 0, result.stderr)
        destination = self.output / 'hello-0.1.0'
        self.assertEqual((destination / 'share/readme.txt').read_bytes(),
                         support.read_bytes())
        shutil.rmtree(self.output)
        (self.source / 'unlisted').write_text('do not copy me')
        self.assert_rejected(self.run_fixture())
        (self.source / 'unlisted').unlink()
        (self.source / 'unused-directory').mkdir()
        self.assert_rejected(self.run_fixture())
        (self.source / 'unused-directory').rmdir()
        support.unlink()
        self.assert_rejected(self.run_fixture())
        support.symlink_to(ROOT / 'Makefile')
        self.assert_rejected(self.run_fixture())
        support.unlink()
        os.link(ROOT / 'Makefile', support)
        self.assert_rejected(self.run_fixture())
        support.unlink()
        os.mkfifo(support)
        self.assert_rejected(self.run_fixture())
        support.unlink()
        with support.open('wb') as stream:
            stream.truncate(16777216)
        self.assertEqual(self.run_fixture().returncode, 0)
        shutil.rmtree(self.output)
        with support.open('ab') as stream:
            stream.write(b'x')
        self.assert_rejected(self.run_fixture())

    def test_source_output_ancestry_overlap_is_rejected_before_writes(self):
        for layout in ('source-in-target', 'source-is-target',
                       'output-in-source', 'same-root'):
            with self.subTest(layout=layout), tempfile.TemporaryDirectory() as root:
                root = Path(root)
                self.output = root / 'output'
                source = self.output / 'hello-0.1.0/source'
                if layout == 'output-in-source':
                    source = root / 'source'
                    self.output = source / 'output'
                elif layout == 'source-is-target':
                    source = self.output / 'hello-0.1.0'
                elif layout == 'same-root':
                    source = self.output
                source.mkdir(parents=True)
                record = source / 'pkg.json'
                record.write_text(json.dumps(self.manifest))
                before = {p.relative_to(root): p.read_bytes()
                          for p in root.rglob('*') if p.is_file()}
                before_paths = set(root.rglob('*'))
                result = self.run_fixture(source=source)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn(b'overlap', result.stderr)
                self.assertEqual({p.relative_to(root): p.read_bytes()
                                  for p in root.rglob('*') if p.is_file()}, before)
                self.assertEqual(set(root.rglob('*')), before_paths)

    def test_prefix_command_docs_are_independently_sorted(self):
        names = ['a', 'a-a']
        docs = ['docs/a-a.md', 'docs/a.md']
        manifest = dict(self.manifest, commands=names, docs=docs,
                        files=sorted(['metadata/commands.json', *docs,
                                      *['bin/' + name for name in names]]))
        builder.manifest_check(manifest)
        manifest['docs'] = list(reversed(docs))
        with self.assertRaises(ValueError):
            builder.manifest_check(manifest)

    def test_command_name_leaves_room_for_md_suffix(self):
        command = json.loads(subprocess.check_output([ROOT / 'build/export-hello']))
        command['name'] = 'a' * 61
        with self.assertRaisesRegex(ValueError, 'command name'):
            builder.command_check(command)
        for length in (60, 61, 63, 64):
            name = 'a' * length
            docs = ['docs/' + name + '.md']
            manifest = dict(self.manifest, commands=[name], docs=docs,
                            files=sorted(['bin/' + name, *docs,
                                          'metadata/commands.json']))
            command['name'] = name
            if length == 60:
                builder.manifest_check(manifest)
                builder.command_check(command)
                self.assertTrue(builder.path_valid(docs[0]))
                exporter = Path(self.temporary.name) / 'exporter'
                exporter.write_text('#!' + sys.executable + '\nprint(' +
                                    repr(json.dumps(command)) + ')\n')
                exporter.chmod(0o755)
                self.assertEqual(self.run_fixture(manifest, exporter=exporter).returncode, 0)
            else:
                with self.assertRaisesRegex(ValueError, 'inventory'):
                    builder.manifest_check(manifest)
                with self.assertRaisesRegex(ValueError, 'command name'):
                    builder.command_check(command)
        # Package names and long option aliases retain the 63-byte bound.
        builder.manifest_check(dict(self.manifest, name='a' * 63))
        command['name'] = 'hello'
        command['options'][0]['long'] = ['a' * 63]
        builder.command_check(command)

    def test_source_manifest_path_cannot_be_linked(self):
        link = Path(self.temporary.name) / 'linked-source'
        link.symlink_to(self.source, target_is_directory=True)
        self.assert_rejected(self.run_fixture(source=link))

    def test_existing_output_is_never_replaced_or_deleted(self):
        destination = self.output / 'hello-0.1.0'
        destination.mkdir(parents=True)
        notes = destination / 'human-notes'
        notes.write_bytes(b'Keep this human work.\n')
        before_inode = destination.stat().st_ino
        result = self.run_fixture()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(notes.read_bytes(), b'Keep this human work.\n')
        self.assertEqual(destination.stat().st_ino, before_inode)
        self.assertEqual(list(destination.iterdir()), [notes])
        notes.unlink()
        destination.rmdir()
        self.assertEqual(self.run_fixture().returncode, 0)
        before_inode = destination.stat().st_ino
        self.assertEqual(self.run_fixture().returncode, 0)
        self.assertEqual(destination.stat().st_ino, before_inode)
        binary = destination / 'bin/hello'
        binary.write_bytes(b'changed human bytes')
        result = self.run_fixture()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(b'existing output differs', result.stderr)
        self.assertEqual(binary.read_bytes(), b'changed human bytes')
        self.assertEqual(destination.stat().st_ino, before_inode)
        self.assertEqual([p.name for p in self.output.iterdir()], ['hello-0.1.0'])

    def test_publication_restore_paired_fault_cannot_remove_existing_output(self):
        self.assertEqual(self.run_fixture().returncode, 0)
        destination = self.output / 'hello-0.1.0'
        before = {p.relative_to(destination): (p.read_bytes(), p.stat().st_ino)
                  for p in destination.rglob('*') if p.is_file()}
        args = argparse.Namespace(manifest=self.source / 'pkg.json',
                                  binary=ROOT / 'build/hello',
                                  exporter=ROOT / 'build/export-hello',
                                  output_root=self.output)
        actual_rename = Path.rename
        calls = []
        def fail_publish_restore(path, target):
            calls.append(path.name)
            if path.name in ('payload', 'previous'):
                raise OSError('injected ' + path.name + ' rename failure')
            return actual_rename(path, target)
        with patch.object(Path, 'rename', fail_publish_restore):
            self.assertEqual(builder.build(args), destination)
        self.assertEqual(calls, [])
        self.assertEqual({p.relative_to(destination):
                          (p.read_bytes(), p.stat().st_ino)
                          for p in destination.rglob('*') if p.is_file()}, before)
        self.assertEqual([p.name for p in self.output.iterdir()], ['hello-0.1.0'])
        # The same publication fault on an absent target must clean only stage.
        args.output_root = self.output / 'fresh'
        with patch.object(Path, 'rename', fail_publish_restore):
            with self.assertRaisesRegex(OSError, 'payload'):
                builder.build(args)
        self.assertEqual(list(args.output_root.iterdir()), [])
        self.assertEqual({p.relative_to(destination):
                          (p.read_bytes(), p.stat().st_ino)
                          for p in destination.rglob('*') if p.is_file()}, before)

    @staticmethod
    def preserved_entry(path):
        entry = path.lstat()
        stable = (entry.st_dev, entry.st_ino, entry.st_mode, entry.st_nlink,
                  entry.st_size, entry.st_mtime_ns, entry.st_ctime_ns)
        content = path.read_bytes() if stat.S_ISREG(entry.st_mode) else None
        link = os.readlink(path) if stat.S_ISLNK(entry.st_mode) else None
        return stable, content, link

    def test_preservation_oracle_ignores_read_induced_atime(self):
        path = self.source / 'pkg.json'
        os.utime(path, ns=(1, 1))
        old_atime = path.stat().st_atime_ns
        before = self.preserved_entry(path)
        self.assertGreater(path.stat().st_atime_ns, old_atime)
        self.assertEqual(self.preserved_entry(path), before)

    def test_existing_output_requires_exact_safe_inventory(self):
        for change in ('extra-file', 'extra-dir', 'missing', 'record', 'mode',
                       'symlink', 'hardlink', 'fifo'):
            # Context cleanup runs even after a subcase assertion fails.
            with self.subTest(change=change), tempfile.TemporaryDirectory() as root:
                with patch.object(self, 'output', Path(root) / 'output'):
                    self.check_unsafe_inventory(change)

    def test_inventory_subcases_cleanup_after_assertion_failure(self):
        case = PackageBuild('test_existing_output_requires_exact_safe_inventory')
        outputs = []
        changes = []
        actual_check = case.check_unsafe_inventory

        def fail_first(change):
            changes.append(change)
            outputs.append(case.output)
            actual_check(change)
            if len(changes) == 1:
                case.fail('injected first-subcase failure')

        result = unittest.TestResult()
        with patch.object(case, 'check_unsafe_inventory', fail_first):
            case.run(result)
        self.assertEqual(len(result.failures), 1)
        self.assertIn('injected first-subcase failure', result.failures[0][1])
        self.assertEqual(result.errors, [])
        self.assertEqual(changes, ['extra-file', 'extra-dir', 'missing', 'record',
                                   'mode', 'symlink', 'hardlink', 'fifo'])
        self.assertEqual(len(set(outputs)), 8)
        self.assertTrue(all(not path.parent.exists() for path in outputs))

    def check_unsafe_inventory(self, change):
        destination = self.output / 'hello-0.1.0'
        self.assertEqual(self.run_fixture().returncode, 0)
        path = destination / 'docs/hello.md'
        if change == 'extra-file':
            path = destination / 'human-notes'
            path.write_text('keep')
        elif change == 'extra-dir':
            path = destination / 'human-directory'
            path.mkdir()
        elif change == 'missing':
            path.unlink()
        elif change == 'record':
            path = destination / 'pkg.json'
            path.write_text(json.dumps(dict(self.manifest, version='0.2.0')))
        elif change == 'mode':
            path.chmod(0o600)
        else:
            path.unlink()
            if change == 'symlink':
                path.symlink_to(self.source / 'pkg.json')
            elif change == 'hardlink':
                os.link(self.source / 'pkg.json', path)
            else:
                os.mkfifo(path)
        # Make a validator read observably update atime on relatime mounts.
        if change == 'record':
            os.utime(path, ns=(1, 1))
        inode = destination.stat().st_ino
        entry = self.preserved_entry(path) if path.exists() else None
        result = self.run_fixture()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(destination.stat().st_ino, inode)
        if entry is not None:
            self.assertEqual(self.preserved_entry(path), entry)
        self.assertEqual([p.name for p in self.output.iterdir()], ['hello-0.1.0'])

    def test_binary_and_exporter_under_output_are_rejected_before_execution(self):
        for kind in ('binary', 'exporter'):
            with self.subTest(kind=kind):
                self.output.mkdir(exist_ok=True)
                marker = self.output / 'executed'
                executable = self.output / kind
                executable.write_text('#!' + sys.executable + '\nfrom pathlib import Path\n'
                                      'Path(' + repr(str(marker)) + ').touch()\n')
                executable.chmod(0o755)
                args = argparse.Namespace(manifest=self.source / 'pkg.json',
                                          binary=ROOT / 'build/hello',
                                          exporter=ROOT / 'build/export-hello',
                                          output_root=self.output)
                setattr(args, kind, executable)
                with self.assertRaisesRegex(ValueError, 'overlap'):
                    builder.build(args)
                self.assertFalse(marker.exists())
                self.assertEqual(list(self.output.iterdir()), [executable])
                executable.unlink()

    def test_repeatable_bytes_and_failed_build_preserves_prior_output(self):
        self.assertEqual(self.run_fixture().returncode, 0)
        destination = self.output / 'hello-0.1.0'
        def snapshot():
            return {p.relative_to(destination).as_posix():
                    (p.read_bytes(), p.stat().st_mode & 0o777)
                    for p in destination.rglob('*') if p.is_file()}
        before = snapshot()
        self.assertEqual(self.run_fixture().returncode, 0)
        self.assertEqual(snapshot(), before)
        manifest = dict(self.manifest, files=sorted(
            self.manifest['files'] + ['missing']))
        result = self.run_fixture(manifest)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(snapshot(), before)
        self.assertEqual([p.name for p in self.output.iterdir()], ['hello-0.1.0'])

    def test_exporter_schema_and_output_failures_do_not_publish(self):
        good = json.loads(subprocess.check_output([ROOT / 'build/export-hello']))
        cases = [dict(good, unknown=1), dict(good, summary='x' * 257),
                 dict(good, usage=''), dict(good, usage=good['usage'] + '\n'),
                 dict(good, arguments=[{}]),
                 dict(good, options=[dict(good['options'][0], min_count=-1)]),
                 dict(good, options=[dict(good['options'][0], max_count=False)]),
                 dict(good, options=[dict(good['options'][0], value='string')]),
                 dict(good, options=[dict(good['options'][0], short=['!'])]),
                 dict(good, options=good['options'] * 2),
                 dict(good, name='wrong'), dict(good, long_help='x' * 16385)]
        exporter = Path(self.temporary.name) / 'exporter'
        for command in cases:
            with self.subTest(command=command):
                exporter.write_text('#!' + sys.executable + '\nprint(' +
                                    repr(json.dumps(command)) + ')\n')
                exporter.chmod(0o755)
                self.assert_rejected(self.run_fixture(exporter=exporter))
        for body in ['print("x" * 262145)', 'raise SystemExit(1)',
                     'print("not JSON")']:
            exporter.write_text('#!' + sys.executable + '\n' + body + '\n')
            exporter.chmod(0o755)
            self.assert_rejected(self.run_fixture(exporter=exporter))
        self.output.write_text('output root is a regular file')
        result = self.run_fixture()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(self.output.read_text(), 'output root is a regular file')

    def test_shared_renderer_escapes_aliases_and_logical_counts(self):
        result = subprocess.run(['make', 'build/test_metadata'], cwd=ROOT,
                                capture_output=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        command = json.loads(subprocess.check_output(
            [ROOT / 'build/test_metadata', 'fixture']))
        self.assertEqual(command['summary'], 'Quoted "summary".')
        self.assertEqual(command['long_help'], '')
        self.assertEqual(command['arguments'], [{
            'metavar': 'ITEM', 'description': 'A "quoted" item.\nNext\tline.',
            'min_count': 0, 'max_count': None}])
        self.assertEqual(command['options'], [{
            'metavar': '', 'description': 'A flag with \\ text.',
            'min_count': 0, 'max_count': 1, 'short': ['a', 'b'],
            'long': ['alpha', 'beta'], 'value': 'none'}])
        self.assertNotIn('Not a positional', json.dumps(command))

    def test_actual_payload_write_failure_leaves_no_publication(self):
        self.assert_rejected(self.run_fixture(output_limit=True))

    def test_manifest_path_file_count_and_total_byte_boundaries(self):
        # Inclusive manifest byte bound; whitespace is legal input.
        raw = json.dumps(self.manifest).encode()
        (self.source / 'pkg.json').write_bytes(raw + b' ' * (65536 - len(raw)))
        self.assertEqual(self.run_fixture().returncode, 0)
        shutil.rmtree(self.output)
        (self.source / 'pkg.json').write_bytes(raw + b' ' * (65537 - len(raw)))
        self.assert_rejected(self.run_fixture())
        # 64 files, including one path with four 63-byte components.
        longest = '/'.join(['a' * 63] * 4)
        support = [longest] + ['support' + str(i) for i in range(60)]
        for name in support:
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'')
        manifest = dict(self.manifest, files=sorted(self.manifest['files'] + support))
        self.assertEqual(self.run_fixture(manifest).returncode, 0)
        shutil.rmtree(self.output)
        (self.source / 'extra').write_bytes(b'')
        manifest['files'] = sorted(manifest['files'] + ['extra'])
        self.assert_rejected(self.run_fixture(manifest))
        shutil.rmtree(self.source)
        self.source.mkdir()
        names = ['support' + str(i) for i in range(4)]
        manifest = dict(self.manifest, files=sorted(self.manifest['files'] + names))
        # Account for generated bytes from the independent happy-path artifact.
        artifact = ROOT / 'build/packages/hello-0.1.0'
        generated_size = sum((artifact / p).stat().st_size
                             for p in self.manifest['files'])
        sizes = [16777216] * 3 + [67108864 - 3 * 16777216 - generated_size]
        for name, size in zip(names, sizes):
            with (self.source / name).open('wb') as stream:
                stream.truncate(size)
        self.assertEqual(self.run_fixture(manifest).returncode, 0)
        shutil.rmtree(self.output)
        with (self.source / names[-1]).open('ab') as stream:
            stream.write(b'x')
        self.assert_rejected(self.run_fixture())

    def test_generated_package_agrees_with_shared_definitions(self):
        result = subprocess.run(['make', 'package-hello'], cwd=ROOT,
                                capture_output=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        package = ROOT / 'build/packages/hello-0.1.0'
        manifest = json.loads((package / 'pkg.json').read_bytes())
        self.assertEqual((package / 'pkg.json').read_bytes(),
                         (ROOT / 'packages/hello/pkg.json').read_bytes())
        self.assertEqual(manifest['name'], 'hello')
        self.assertEqual(manifest['version'], '0.1.0')
        self.assertEqual(manifest['commands'], ['hello'])
        for file in ['pkg.json', *manifest['files']]:
            self.assertEqual((package / file).stat().st_mode & 0o777,
                             0o755 if file == 'bin/hello' else 0o644)
        for directory in [package, *[p for p in package.rglob('*') if p.is_dir()]]:
            self.assertEqual(directory.stat().st_mode & 0o777, 0o755)
        self.assertEqual(sorted(p.relative_to(package).as_posix()
                                for p in package.rglob('*') if p.is_file()),
                         sorted(['pkg.json'] + manifest['files']))
        raw = (package / 'metadata/commands.json').read_bytes()
        metadata = json.loads(raw)
        self.assertEqual(list(metadata),
                         ['schema_version', 'package', 'version', 'commands'])
        self.assertEqual(metadata['schema_version'], 1)
        self.assertEqual(metadata['package'], 'hello')
        self.assertEqual(metadata['version'], '0.1.0')
        command, = metadata['commands']
        self.assertEqual(command, {
            'name': 'hello', 'summary': 'Print a greeting.',
            'long_help': 'Print a greeting. Default name: world.',
            'usage': 'hello [-h] [--name=NAME]',
            'arguments': [], 'options': [
                {'metavar': 'NAME', 'description': 'Set the greeting name.',
                 'min_count': 0, 'max_count': 1, 'short': [],
                 'long': ['name'], 'value': 'required'},
                {'metavar': '', 'description': 'Show help and exit.',
                 'min_count': 0, 'max_count': 1, 'short': ['h'],
                 'long': ['help'], 'value': 'none'}]})
        self.assertEqual(raw, (json.dumps(metadata, indent=2,
                                         ensure_ascii=False) + '\n').encode())
        help_run = subprocess.run([package / 'bin/hello', '--help'],
                                  capture_output=True, check=True)
        help_text = help_run.stdout.decode()
        self.assertEqual(help_text.splitlines()[0], 'Usage: ' + command['usage'])
        docs = (package / 'docs/hello.md').read_text()
        for text in [command['summary'], command['long_help'], command['usage'],
                     '--name', '--help', '-h', 'Set the greeting name.',
                     'Show help and exit.']:
            self.assertIn(text, docs)
        self.assertEqual(subprocess.check_output([package / 'bin/hello']),
                         b'Hello, world!\n')


if __name__ == '__main__':
    unittest.main()
