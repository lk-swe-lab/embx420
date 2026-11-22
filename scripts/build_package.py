"""Build a local directory package. Never write installed package state."""
import argparse
import ctypes
import json
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    """Reject invalid source data without repairing it."""
    if not condition:
        raise ValueError(message)


def unique_object(pairs):
    """Reject duplicate JSON object keys."""
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result


def json_read(raw, limit):
    """Read bounded UTF-8 JSON with a lexical nesting bound."""
    require(len(raw) <= limit, 'JSON byte limit exceeded')
    text = raw.decode('utf-8')
    depth = 0
    quoted = escaped = False
    for ch in text:
        if quoted:
            if escaped:
                escaped = False
            elif ch == '\\':
                escaped = True
            elif ch == '"':
                quoted = False
        elif ch == '"':
            quoted = True
        elif ch in '[{':
            depth += 1
            require(depth <= 16, 'JSON nesting limit exceeded')
        elif ch in ']}':
            depth -= 1
    value = json.loads(text, object_pairs_hook=unique_object,
                       parse_constant=lambda s: require(False, 'invalid ' + s))
    # Reject invalid escaped Unicode and NUL in every decoded string.
    def strings(node):
        if isinstance(node, str):
            node.encode('utf-8')
            require('\x00' not in node, 'NUL in JSON text')
        elif isinstance(node, list):
            for item in node:
                strings(item)
        elif isinstance(node, dict):
            for key, item in node.items():
                strings(key)
                strings(item)
    strings(value)
    return value


def public_text(value, limit, multiline=False, nonempty=False):
    """Check public text bytes and controls."""
    require(isinstance(value, str), 'expected public text')
    require((not nonempty or value) and len(value.encode('utf-8')) <= limit,
            'public text length exceeded or empty')
    require(all(ord(c) >= 32 and ord(c) != 127 or
                multiline and c in '\n\t' for c in value),
            'control character in public text')


def name_valid(value):
    """Check exact package names or long option aliases."""
    return isinstance(value, str) and re.fullmatch('[a-z][a-z0-9-]{0,62}', value)


def command_name_valid(value):
    """Leave three bytes for the generated .md component suffix."""
    return name_valid(value) and len(value) <= 60


def path_valid(value):
    """Check exact relative payload paths without normalization."""
    return (isinstance(value, str) and len(value) <= 255 and
            all(re.fullmatch('[A-Za-z0-9_][A-Za-z0-9_.-]{0,62}', part)
                for part in value.split('/')))


def ordered_list(value, maximum, valid, nonempty=False):
    """Check ordered unique inventory arrays."""
    require(isinstance(value, list) and len(value) <= maximum and
            (not nonempty or value) and all(valid(v) for v in value),
            'invalid inventory array')
    require(value == sorted(set(value)), 'inventory must be unique and sorted')


def manifest_check(manifest):
    """Validate the authoritative schema and required generated inventory."""
    require(isinstance(manifest, dict) and set(manifest) == {
        'schema_version', 'name', 'version', 'description', 'commands',
        'files', 'docs', 'dependencies'}, 'manifest fields mismatch')
    require(type(manifest['schema_version']) is int and
            manifest['schema_version'] in (1, 2), 'unsupported schema version')
    require(name_valid(manifest['name']), 'invalid package name')
    version = manifest['version']
    require(isinstance(version, str) and re.fullmatch(
        r'(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})',
        version) and all(int(p) <= 65535 for p in version.split('.')),
        'invalid version')
    public_text(manifest['description'], 256, nonempty=True)
    ordered_list(manifest['commands'], 16, command_name_valid, True)
    ordered_list(manifest['files'], 64, path_valid, True)
    ordered_list(manifest['docs'], 64, path_valid)
    require(manifest['dependencies'] == [], 'dependencies are unsupported')
    files = set(manifest['files'])
    docs = sorted('docs/' + name + '.md' for name in manifest['commands'])
    require(manifest['docs'] == docs, 'required generated docs mismatch')
    binaries = (('lib/' + name + '.so' if manifest['schema_version'] == 2
                 else 'bin/' + name) for name in manifest['commands'])
    require({'metadata/commands.json', *docs, *binaries} <= files,
            'required generated inventory missing')
    require('pkg.json' not in files, 'manifest cannot list itself')
    require(not any('/'.join(path.split('/')[:i]) in files
                    for path in files for i in range(1, len(path.split('/')))),
            'file conflicts with parent directory')


def markdown(command):
    """Render public facts from structured metadata, not terminal help."""
    lines = ['# ' + command['name'], '', command['summary'], '',
             '## Usage', '', '```text', command['usage'], '```']
    if command['long_help']:
        lines += ['', command['long_help']]
    for key, heading in [('arguments', 'Arguments'), ('options', 'Options')]:
        if command[key]:
            lines += ['', '## ' + heading, '']
        for entry in command[key]:
            labels = (['-' + a for a in entry['short']] +
                      ['--' + a for a in entry['long']]) if key == 'options' \
                else [entry['metavar']]
            if key == 'options' and entry['value'] != 'none':
                labels = [s + ' ' + entry['metavar'] for s in labels]
            maximum = entry['max_count']
            count = str(entry['min_count']) + '..' + (
                'unbounded' if maximum is None else str(maximum))
            lines += ['- ' + ', '.join('`' + s + '`' for s in labels) +
                      ': ' + entry['description'] + ' Count: ' + count + '.']
    return '\n'.join(lines) + '\n'


def command_check(command):
    """Validate the exporter boundary before generating package output."""
    require(isinstance(command, dict) and set(command) == {
        'name', 'summary', 'long_help', 'usage', 'arguments', 'options'},
        'command metadata fields mismatch')
    require(command_name_valid(command['name']), 'invalid metadata command name')
    public_text(command['summary'], 256, nonempty=True)
    public_text(command['long_help'], 16384, multiline=True)
    public_text(command['usage'], 16384, multiline=True, nonempty=True)
    require(not command['usage'].endswith('\n'), 'usage has a terminal newline')
    require(isinstance(command['arguments'], list) and
            isinstance(command['options'], list) and
            len(command['arguments']) + len(command['options']) <= 64,
            'invalid metadata entries')
    aliases = set()
    for kind in ('arguments', 'options'):
        for entry in command[kind]:
            keys = {'metavar', 'description', 'min_count', 'max_count'}
            if kind == 'options':
                keys |= {'short', 'long', 'value'}
            require(isinstance(entry, dict) and set(entry) == keys,
                    'metadata entry fields mismatch')
            public_text(entry['metavar'], 1024, nonempty=kind == 'arguments')
            public_text(entry['description'], 1024, multiline=True)
            minimum, maximum = entry['min_count'], entry['max_count']
            require(type(minimum) is int and minimum >= 0 and
                    (maximum is None or type(maximum) is int and
                     maximum >= minimum), 'invalid metadata counts')
            if kind == 'options':
                require(entry['value'] in ('none', 'required', 'optional'),
                        'invalid option value rule')
                require(entry['value'] != 'none' or entry['metavar'] == '',
                        'valueless option must have an empty metavar')
                require(entry['value'] == 'none' or entry['metavar'],
                        'value option requires a metavar')
                require(isinstance(entry['short'], list) and
                        isinstance(entry['long'], list) and
                        entry['short'] + entry['long'], 'option needs aliases')
                for prefix, valid in [('short', lambda s: isinstance(s, str) and
                                      re.fullmatch('[A-Za-z0-9]', s)),
                                     ('long', name_valid)]:
                    for alias in entry[prefix]:
                        require(valid(alias) and (prefix, alias) not in aliases,
                                'invalid or duplicate option alias')
                        aliases.add((prefix, alias))


def no_links(path):
    """Reject symlinks in an input or output path and its ancestors."""
    path = path.absolute()
    for part in [*reversed(path.parents), path]:
        if part.exists() or part.is_symlink():
            require(not stat.S_ISLNK(part.lstat().st_mode), 'symlink: ' + str(part))


def regular_read(path, maximum, executable=False):
    """Read one bounded, unlinked regular file; detect observed mutation."""
    no_links(path)
    before = path.stat()
    require(stat.S_ISREG(before.st_mode) and before.st_nlink == 1,
            'expected unlinked regular file: ' + str(path))
    require(before.st_size <= maximum, 'file byte limit exceeded: ' + str(path))
    require(not executable or before.st_mode & 0o111,
            'binary is not executable: ' + str(path))
    with path.open('rb') as stream:
        data = stream.read(maximum + 1)
    after = path.stat()
    identity = ('st_dev', 'st_ino', 'st_mode', 'st_nlink', 'st_size',
                'st_mtime_ns', 'st_ctime_ns')
    require(len(data) <= maximum and len(data) == after.st_size and
            all(getattr(before, key) == getattr(after, key) for key in identity),
            'file changed or exceeded byte limit: ' + str(path))
    return data


def source_inventory(source, support):
    """Require exact support files and only their necessary source parents."""
    expected = {'pkg.json', *support}
    parents = {str(parent) for path in support for parent in Path(path).parents
               if str(parent) != '.'}
    def walk(directory):
        for path in directory.iterdir():
            relative = path.relative_to(source).as_posix()
            info = path.lstat()
            if stat.S_ISDIR(info.st_mode):
                require(relative in parents, 'unlisted directory: ' + relative)
                walk(path)
            else:
                require(relative in expected, 'unlisted source file: ' + relative)
                require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1,
                        'unsafe source file: ' + relative)
    walk(source)
    require(all((source / path).is_file() for path in expected),
            'missing source support file')


def publish_directory(stage, destination):
    """Publish once; never replace even a concurrently created empty target."""
    libc = ctypes.CDLL(None, use_errno=True)
    rename = libc.renameat2
    rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int,
                       ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    if rename(-100, bytes(stage), -100, bytes(destination), 1) != 0:
        raise OSError(ctypes.get_errno(), 'package publication failed')


def build(args):
    """Render all content privately, then publish the complete directory."""
    # Reject overlap before running the exporter or creating any output.
    output = args.output_root.resolve()
    source = args.manifest.parent.resolve()
    require(output != source and output not in source.parents and
            source not in output.parents, 'source/output ancestry overlap')
    for path in (args.binary, args.exporter):
        resolved = path.resolve()
        require(output != resolved and output not in resolved.parents,
                'input/output ancestry overlap: ' + str(path))
    raw = regular_read(args.manifest, 65536)
    manifest = json_read(raw, 65536)
    manifest_check(manifest)
    no_links(args.exporter)
    with tempfile.TemporaryFile() as exported:
        subprocess.run([args.exporter.absolute()], stdout=exported,
                       check=True, timeout=10)
        exported.seek(0)
        command = json_read(exported.read(262145), 262144)
    command_check(command)
    require([command['name']] == manifest['commands'],
            'exported commands do not match manifest')
    metadata = {'schema_version': 1, 'package': manifest['name'],
                'version': manifest['version'], 'commands': [command]}
    binary_path = ('lib/' + command['name'] + '.so'
                   if manifest['schema_version'] == 2
                   else 'bin/' + command['name'])
    generated = {binary_path, 'metadata/commands.json',
                 'docs/' + command['name'] + '.md'}
    source_inventory(args.manifest.parent, set(manifest['files']) - generated)
    no_links(args.output_root)
    args.output_root.mkdir(parents=True, exist_ok=True)
    destination = args.output_root / (manifest['name'] + '-' + manifest['version'])
    no_links(destination)
    require(not destination.exists() or destination.is_dir(),
            'output destination is not a directory')
    with tempfile.TemporaryDirectory(prefix='.package-',
                                     dir=args.output_root) as temporary:
        stage = Path(temporary) / 'payload'
        stage.mkdir()
        (stage / 'pkg.json').write_bytes(raw)
        (stage / 'pkg.json').chmod(0o644)
        total = 0
        for name in manifest['files']:
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if name == binary_path:
                data = regular_read(args.binary, 16777216,
                                    executable=manifest['schema_version'] == 1)
            elif name == 'metadata/commands.json':
                data = (json.dumps(metadata, ensure_ascii=False,
                                   indent=2) + '\n').encode('utf-8')
                require(len(data) <= 262144, 'metadata byte limit exceeded')
            elif name == 'docs/' + command['name'] + '.md':
                data = markdown(command).encode('utf-8')
            else:
                data = regular_read(args.manifest.parent / name, 16777216)
            total += len(data)
            require(len(data) <= 16777216 and total <= 67108864,
                    'payload byte limit exceeded')
            target.write_bytes(data)
            target.chmod(0o755 if name == binary_path and
                         manifest['schema_version'] == 1 else 0o644)
        for directory in [stage, *[p for p in stage.rglob('*') if p.is_dir()]]:
            directory.chmod(0o755)
        if destination.exists():
            # Never move or delete an existing target. Accept only an exact,
            # safe artifact, including the record, inventory, bytes and modes.
            source_inventory(destination, set(manifest['files']))
            for path in [destination, *destination.rglob('*')]:
                relative = path.relative_to(destination)
                expected = stage / relative
                require(path.stat().st_mode == expected.stat().st_mode,
                        'existing output differs (mode): ' + str(path))
                if path.is_file():
                    require(regular_read(path, 16777216) == expected.read_bytes(),
                            'existing output differs (bytes): ' + str(path))
        else:
            publish_directory(stage, destination)
    return destination


def main():
    """Parse build inputs; report one diagnostic on build failure."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path,
                        default=ROOT / 'packages/hello-so/pkg.json')
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/example-modules/hello.so')
    parser.add_argument('--exporter', type=Path, default=ROOT / 'build/export-hello')
    parser.add_argument('--output-root', type=Path, default=ROOT / 'build/native-candidates')
    args = parser.parse_args()
    try:
        print(build(args))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print('build-package: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
