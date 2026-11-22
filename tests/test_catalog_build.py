"""Check schema 3 and trusted build-time catalog output."""
import argparse
import importlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))


class CatalogBuild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.scratch = Path(cls.temp.name)
        source = cls.scratch / 'schema.c'
        source.write_text('''#include "pkg_schema.h"
#include <stdio.h>
int pkg_name_valid(const char *s) {
 if (!s || *s < 'a' || *s > 'z') return 0;
 for (; *s; ++s) if (!(*s >= 'a' && *s <= 'z') &&
 !(*s >= '0' && *s <= '9') && *s != '-') return 0;
 return 1;
}
int main(void) {
 char data[32768]; size_t n = fread(data, 1, sizeof(data), stdin);
 pj_value *v = pj_parse(data, n);
 const pj_value *m = pj_get(v, "manifest");
 int ok = pkg_manifest_valid(m) && pkg_metadata_valid(pj_get(v, "metadata"), m);
 pj_free(v); return !ok;
}
''')
        cls.schema = cls.scratch / 'schema'
        subprocess.run(['cc', '-std=c17', '-Wall', '-Wextra', '-Werror', '-Isrc',
                        str(source), 'src/pkg_schema.c', 'src/pkg_json.c',
                        '-o', str(cls.schema)], cwd=ROOT, check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_export_abi_layout_matches_native_headers(self):
        import ctypes
        builder = importlib.import_module('build_catalog')
        source = self.scratch / 'abi.c'
        source.write_text('''#include "module_abi.h"
#include <stdio.h>
int main(void) {
 printf("%zu %zu %zu %zu %zu\\n", sizeof(aishell_host_v1),
 sizeof(aishell_module_v1), sizeof(cmd_spec_t),
 offsetof(aishell_module_v1, commands), offsetof(cmd_spec_t, print_usage));
 return 0;
}
''')
        binary = self.scratch / 'abi'
        subprocess.run(['cc', '-std=c17', '-Wall', '-Wextra', '-Werror', '-Isrc',
                        str(source), '-o', str(binary)], cwd=ROOT, check=True)
        self.assertEqual(list(map(int, subprocess.check_output([binary]).split())),
                         [ctypes.sizeof(builder.Host), ctypes.sizeof(builder.Module),
                          ctypes.sizeof(builder.Command), builder.Module.commands.offset,
                          builder.Command.print_usage.offset])

    def test_real_catalog_has_23_usage_only_bundles(self):
        # Run a valid baseline without a missing-file/import error during RED.
        try:
            builder = importlib.import_module('build_catalog')
        except ModuleNotFoundError:
            builder = None
        self.assertIsNotNone(builder, 'catalog build feature is not implemented')
        assert builder is not None
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'catalog'
            args = argparse.Namespace(output=output, version='0.3.0')
            builder.build(args)
            catalog = json.loads((output / 'catalog.json').read_bytes())
            self.assertEqual(set(catalog), {'schema_version', 'packages'})
            self.assertEqual(catalog['schema_version'], 1)
            names = [p['name'] for p in catalog['packages']]
            self.assertEqual(len(names), 23)
            self.assertEqual(names, sorted(set(names)))
            self.assertTrue({'pkg', 'help', 'cd', 'hello'} <= set(names))
            for package in catalog['packages']:
                self.assertEqual(set(package), {'name', 'version', 'description',
                                                'path', 'commands'})
                name = package['name']
                self.assertEqual(package['version'], '0.3.0')
                self.assertEqual(package['path'], name + '-0.3.0')
                bundle = output / package['path']
                manifest = json.loads((bundle / 'pkg.json').read_bytes())
                self.assertEqual(manifest['schema_version'], 3)
                self.assertEqual(manifest['commands'], [name])
                self.assertEqual(manifest['description'], package['description'])
                self.assertEqual(manifest['commands'], package['commands'])
                self.assertEqual(sorted(str(p.relative_to(bundle)) for p in
                                        bundle.rglob('*') if p.is_file()),
                                 sorted(['pkg.json', *manifest['files']]))
                metadata = json.loads((bundle / 'metadata/commands.json').read_bytes())
                self.assertEqual(metadata['schema_version'], 2)
                command, = metadata['commands']
                self.assertEqual(set(command), {'name', 'summary', 'long_help', 'usage'})
                self.assertEqual(command['name'], name)
                self.assertTrue(command['usage'])
                self.assertFalse(command['usage'].endswith('\n'))
                self.assertIn(command['usage'], (bundle / ('docs/' + name + '.md')).read_text())
                result = subprocess.run([self.schema], input=json.dumps(
                    dict(manifest=manifest, metadata=metadata)).encode())
                self.assertEqual(result.returncode, 0, name)
            hello = json.loads((output / 'hello-0.3.0/metadata/commands.json').read_bytes())
            self.assertIn('--name', hello['commands'][0]['usage'])
            def snapshot():
                return {str(p.relative_to(output)): (p.read_bytes(), p.stat().st_ino,
                         p.stat().st_mtime_ns) for p in output.rglob('*') if p.is_file()}
            before = snapshot()
            builder.build(args)
            self.assertEqual(snapshot(), before)

    def test_unlisted_input_files_are_rejected_before_native_loading(self):
        builder = importlib.import_module('build_catalog')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            shutil.copytree(ROOT / 'build/modules', root / 'build/modules')
            shutil.copytree(ROOT / 'build/example-modules', root / 'build/example-modules')
            marker = root / 'build/modules/unknown.so'
            marker.write_bytes(b'not a module')
            args = argparse.Namespace(output=root / 'output', version='0.3.0')
            with patch.object(builder, 'ROOT', root):
                with self.assertRaisesRegex(ValueError, 'unlisted'):
                    builder.build(args)
            self.assertFalse(args.output.exists())
            self.assertEqual(marker.read_bytes(), b'not a module')

    @staticmethod
    def snapshot(root):
        result = {}
        for path in [root, *root.rglob('*')]:
            info = path.lstat()
            record = (info.st_ino, info.st_mode, info.st_nlink, info.st_mtime_ns,
                      info.st_ctime_ns)
            content = path.read_bytes() if stat.S_ISREG(info.st_mode) else None
            link = os.readlink(path) if stat.S_ISLNK(info.st_mode) else None
            result[str(path.relative_to(root))] = (record, content, link)
        return result

    def test_existing_output_and_inputs_are_preserved_on_failure(self):
        builder = importlib.import_module('build_catalog')
        for change in ('bytes', 'mode', 'symlink', 'hardlink', 'fifo', 'extra-file',
                       'extra-dir', 'missing', 'changed-binary', 'new-version'):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                shutil.copytree(ROOT / 'build/modules', root / 'build/modules')
                shutil.copytree(ROOT / 'build/example-modules', root / 'build/example-modules')
                args = argparse.Namespace(output=root / 'catalog', version='0.3.0')
                with patch.object(builder, 'ROOT', root):
                    builder.build(args)
                    target = args.output / 'hello-0.3.0/docs/hello.md'
                    if change == 'bytes':
                        target.write_bytes(b'keep changed bytes')
                    elif change == 'mode':
                        target.chmod(0o600)
                    elif change in ('symlink', 'hardlink', 'fifo', 'missing'):
                        target.unlink()
                        if change == 'symlink':
                            target.symlink_to(root / 'build/example-modules/hello.so')
                        elif change == 'hardlink':
                            os.link(root / 'build/example-modules/hello.so', target)
                        elif change == 'fifo':
                            os.mkfifo(target)
                    elif change == 'extra-file':
                        (args.output / 'human-work').write_bytes(b'keep')
                    elif change == 'extra-dir':
                        (args.output / 'human-directory').mkdir()
                    elif change == 'changed-binary':
                        with (root / 'build/example-modules/hello.so').open('ab') as stream:
                            stream.write(b'new build bytes')
                    else:
                        args.version = '0.4.0'
                    before = self.snapshot(root)
                    with self.assertRaises((ValueError, OSError)):
                        builder.build(args)
                    self.assertEqual(self.snapshot(root), before)

    def test_invalid_names_versions_and_unsafe_inputs_do_not_publish(self):
        builder = importlib.import_module('build_catalog')
        for version in ('../escape', '01.0.0', '65536.0.0', '0.3', '0.3.0/other'):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary) / 'catalog'
                with self.assertRaisesRegex(ValueError, 'version'):
                    builder.build(argparse.Namespace(output=output, version=version))
                self.assertFalse(output.exists())
        for name in ('../escape', '/absolute', 'bad_name', 'x' * 61):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary) / 'catalog'
                with patch.object(builder, 'specifications', return_value=[
                        (None, None, name), (None, None, 'hello')]):
                    with self.assertRaisesRegex(ValueError, 'unsafe'):
                        builder.build(argparse.Namespace(output=output, version='0.3.0'))
                self.assertFalse(output.exists())
        for kind in ('symlink', 'hardlink', 'fifo', 'corrupt'):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                shutil.copytree(ROOT / 'build/modules', root / 'build/modules')
                shutil.copytree(ROOT / 'build/example-modules', root / 'build/example-modules')
                binary = root / 'build/example-modules/hello.so'
                binary.unlink()
                if kind == 'symlink':
                    binary.symlink_to(ROOT / 'build/example-modules/hello.so')
                elif kind == 'hardlink':
                    os.link(ROOT / 'build/example-modules/hello.so', binary)
                elif kind == 'fifo':
                    os.mkfifo(binary)
                else:
                    binary.write_bytes(b'not ELF')
                args = argparse.Namespace(output=root / 'catalog', version='0.3.0')
                with patch.object(builder, 'ROOT', root):
                    with self.assertRaises((ValueError, OSError)):
                        builder.build(args)
                self.assertFalse(args.output.exists())

    def test_output_alias_cannot_bypass_input_boundary(self):
        builder = importlib.import_module('build_catalog')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            shutil.copytree(ROOT / 'build/modules', root / 'build/modules')
            shutil.copytree(ROOT / 'build/example-modules', root / 'build/example-modules')
            output = root / 'build/modules/../modules/catalog'
            before = self.snapshot(root)
            with patch.object(builder, 'ROOT', root):
                with self.assertRaisesRegex(ValueError, 'overlap'):
                    builder.build(argparse.Namespace(output=output, version='0.3.0'))
            self.assertEqual(self.snapshot(root), before)

    def test_publication_failure_leaves_no_catalog(self):
        builder = importlib.import_module('build_catalog')
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'catalog'
            with patch.object(builder, 'publish_directory', side_effect=OSError('injected')):
                with self.assertRaisesRegex(OSError, 'injected'):
                    builder.build(argparse.Namespace(output=output, version='0.3.0'))
            self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_usage_writer_callback_and_errors(self):
        source = self.scratch / 'usage.c'
        source.write_text(r'''#define _POSIX_C_SOURCE 200809L
#include "cmd_spec.h"
#include <errno.h>
#include <stdlib.h>
#ifdef HAS_USAGE
#include "module_usage.h"
#else
static int module_usage_write(FILE *out, const cmd_spec_t *s) {
 (void)out; (void)s; return -1;
}
#endif
static void usage(FILE *out) { fputs("Usage: x \"Q\"\\\t\n\n", out); }
int main(void) {
 cmd_spec_t s = {.name="x", .summary="Quote \" \\", .print_usage=usage};
 if (module_usage_write(stdout, &s)) return 1;
 if (module_usage_write(NULL, &s) != -1 || errno != EINVAL) return 2;
 if (module_usage_write(stdout, NULL) != -1 || errno != EINVAL) return 3;
 s.print_usage = NULL;
 if (module_usage_write(stdout, &s) != -1 || errno != EINVAL) return 4;
 s.print_usage = usage;
 FILE *bad = fopen("/dev/full", "w");
 if (!bad) return 5;
 setvbuf(bad, NULL, _IONBF, 0);
 int rc = module_usage_write(bad, &s); fclose(bad);
 return rc == -1 ? 0 : 6;
}
''')
        binary = self.scratch / 'usage'
        inputs = ['-DHAS_USAGE', 'src/module_usage.c'] if (
            ROOT / 'src/module_usage.c').exists() else []
        subprocess.run(['cc', '-std=c17', '-Wall', '-Wextra', '-Werror', '-Isrc',
                        str(source), *inputs, '-o', str(binary)], cwd=ROOT, check=True)
        result = subprocess.run([binary], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), dict(
            name='x', summary='Quote " \\', long_help='', usage='Usage: x "Q"\\\t'))

    def test_schema3_usage_only_and_legacy_strictness(self):
        for schema in (1, 2, 3):
            for metadata_schema in (1, 2):
                for grammar in (False, True):
                    with self.subTest(schema=schema, metadata=metadata_schema,
                                      grammar=grammar):
                        command: dict[str, object] = dict(name='hello', summary='Greeting.',
                                       long_help='', usage='hello [NAME]')
                        if grammar:
                            command.update(arguments=[], options=[])
                        binary = 'bin/hello' if schema == 1 else 'lib/hello.so'
                        manifest = dict(schema_version=schema, name='hello',
                                        version='0.3.0', description='Greeting.',
                                        commands=['hello'], docs=['docs/hello.md'],
                                        files=sorted([binary, 'docs/hello.md',
                                                      'metadata/commands.json']),
                                        dependencies=[])
                        metadata = dict(schema_version=metadata_schema,
                                        package='hello', version='0.3.0',
                                        commands=[command])
                        result = subprocess.run([self.schema], input=json.dumps(
                            dict(manifest=manifest, metadata=metadata)).encode())
                        valid = ((schema == 3 and metadata_schema == 2 and not grammar)
                                 or (schema in (1, 2) and metadata_schema == 1 and grammar))
                        self.assertEqual(result.returncode, 0 if valid else 1)


if __name__ == '__main__':
    unittest.main()
