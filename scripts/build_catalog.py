"""Build immutable schema-3 bundles. Load trusted native code only at build time.

Native constructors and print_usage callbacks run with this process's rights.
Catalog readers must read data only. They must not use this builder to read it.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import re
import stat
import sys
import tempfile

from build_modules import specifications
from build_package import (command_name_valid, no_links, path_valid,
                           public_text, publish_directory, regular_read, require)

ROOT = Path(__file__).resolve().parents[1]


class Command(C.Structure):
    """Borrow the cmd_spec_t ABI. Do not call run or visit_metadata."""
    _fields_ = [('name', C.c_char_p), ('summary', C.c_char_p),
                ('long_help', C.c_char_p), ('run', C.c_void_p),
                ('print_usage', C.c_void_p), ('visit_metadata', C.c_void_p)]


class Host(C.Structure):
    """Supply ABI identity; no command needs host services to print usage."""
    _fields_ = [('size', C.c_uint32), ('version', C.c_uint32),
                ('context', C.c_void_p), ('find', C.c_void_p),
                ('each', C.c_void_p), ('request_exit', C.c_void_p),
                ('previous_status', C.c_void_p), ('activate', C.c_void_p),
                ('hide_package', C.c_void_p), ('identity', C.c_void_p),
                ('inspect_command', C.c_void_p), ('explicit_only', C.c_void_p)]


class Module(C.Structure):
    """Borrow the aishell_module_v1 descriptor."""
    _fields_ = [('size', C.c_uint32), ('version', C.c_uint32),
                ('spec_size', C.c_uint32), ('command_count', C.c_size_t),
                ('commands', C.POINTER(C.POINTER(Command))),
                ('refresh', C.c_void_p)]


def command_check(command):
    """Check the usage-only boundary. Do not infer a grammar."""
    require(set(command) == {'name', 'summary', 'long_help', 'usage'},
            'usage metadata fields mismatch')
    require(command_name_valid(command['name']), 'invalid command name')
    public_text(command['summary'], 256, nonempty=True)
    public_text(command['long_help'], 16384, multiline=True)
    public_text(command['usage'], 16384, multiline=True, nonempty=True)
    require(not command['usage'].endswith('\n'), 'terminal LF in usage')


def read_command(path, name):
    """Run one trusted module's real callback with a libc FILE stream."""
    no_links(path)
    library = C.CDLL(str(path.absolute()), mode=0)
    entry = getattr(library, 'aishell_module_entry_v1')
    entry.argtypes = [C.POINTER(Host)]
    entry.restype = C.POINTER(Module)
    host = Host(size=C.sizeof(Host), version=1)
    pointer = entry(C.byref(host))
    require(bool(pointer), 'module rejected export host: ' + name)
    module = pointer.contents
    require(module.size >= C.sizeof(Module) and module.version == 1 and
            module.spec_size == C.sizeof(Command) and module.command_count == 1
            and bool(module.commands) and bool(module.commands[0]),
            'module ABI mismatch: ' + name)
    spec = module.commands[0].contents
    require(spec.name is not None and spec.name.decode('utf-8') == name and
            spec.summary is not None and spec.print_usage,
            'module command mismatch: ' + name)
    libc = C.CDLL(None, use_errno=True)
    libc.open_memstream.argtypes = [C.POINTER(C.c_void_p), C.POINTER(C.c_size_t)]
    libc.open_memstream.restype = C.c_void_p
    libc.ferror.argtypes = [C.c_void_p]
    libc.ferror.restype = C.c_int
    libc.fclose.argtypes = [C.c_void_p]
    libc.fclose.restype = C.c_int
    libc.free.argtypes = [C.c_void_p]
    data, length = C.c_void_p(), C.c_size_t()
    stream = libc.open_memstream(C.byref(data), C.byref(length))
    if not stream:
        raise OSError(C.get_errno(), 'open_memstream failed')
    try:
        C.CFUNCTYPE(None, C.c_void_p)(spec.print_usage)(stream)
        failed = libc.ferror(stream)
        closed = libc.fclose(stream)
        stream = None
        require(not failed and not closed, 'usage callback stream error: ' + name)
        require(length.value <= 16384, 'usage callback byte limit exceeded')
        usage = C.string_at(data, length.value).decode('utf-8').rstrip('\n')
    finally:
        if stream:
            libc.fclose(stream)
        libc.free(data)
    command = dict(name=name, summary=(spec.summary or b'').decode('utf-8'),
                   long_help=(spec.long_help or b'').decode('utf-8'), usage=usage)
    command_check(command)
    return command


def json_bytes(value):
    """Use one deterministic UTF-8 representation."""
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8')


def markdown(command):
    """Render only supplied public facts."""
    lines = ['# ' + command['name'], '', command['summary'], '',
             '## Usage', '', '```text', command['usage'], '```']
    if command['long_help']:
        lines += ['', command['long_help']]
    return ('\n'.join(lines) + '\n').encode('utf-8')


def payload(version):
    """Read and validate every source before publication."""
    require(isinstance(version, str) and re.fullmatch(
        r'(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})',
        version) and all(int(p) <= 65535 for p in version.split('.')),
        'invalid version')
    specs = list(specifications())
    names = [name for _, _, name in specs]
    require(len(names) == len(set(names)) and 'hello' in names,
            'duplicate or missing command specifications')
    require(all(command_name_valid(name) and path_valid(name + '-' + version)
                for name in names), 'unsafe command or package filename')
    paths = {name: ROOT / ('build/example-modules/hello.so' if name == 'hello'
                           else 'build/modules/' + name + '.so') for name in names}
    # Check both input inventories. The build stamp is not package content.
    for directory, expected in (
            (ROOT / 'build/modules', {name + '.so' for name in names if name != 'hello'}),
            (ROOT / 'build/example-modules', {'hello.so'})):
        no_links(directory)
        for path in directory.iterdir():
            require(path.name in expected | {'.stamp'},
                    'unlisted module input: ' + str(path))
            regular_read(path, 16777216)
    # Validate all input files before executing any native code.
    binaries = {name: regular_read(path, 16777216) for name, path in paths.items()}
    files, packages = {}, []
    for name in sorted(names):
        command = read_command(paths[name], name)
        require(regular_read(paths[name], 16777216) == binaries[name],
                'module changed during export: ' + name)
        leaf = name + '-' + version
        docs = 'docs/' + name + '.md'
        binary = 'lib/' + name + '.so'
        manifest = dict(schema_version=3, name=name, version=version,
                        description=command['summary'], commands=[name],
                        files=sorted([docs, binary, 'metadata/commands.json']),
                        docs=[docs], dependencies=[])
        metadata = dict(schema_version=2, package=name, version=version,
                        commands=[command])
        contents = {'pkg.json': json_bytes(manifest), docs: markdown(command),
                    binary: binaries[name], 'metadata/commands.json': json_bytes(metadata)}
        require(len(contents['metadata/commands.json']) <= 262144 and
                sum(map(len, contents.values())) <= 67108864, 'payload byte limit exceeded')
        files.update({leaf + '/' + path: data for path, data in contents.items()})
        packages.append(dict(name=name, version=version, description=command['summary'],
                             path=leaf, commands=[name]))
    files['catalog.json'] = json_bytes(dict(schema_version=1, packages=packages))
    return files


def check_existing(output, files):
    """Require exact safe bytes, modes, and inventory. Never modify output."""
    parents = {str(p) for path in files for p in Path(path).parents if str(p) != '.'}
    require(output.is_dir(), 'existing output is not a directory')
    found = set()
    def walk(directory):
        for path in directory.iterdir():
            relative = path.relative_to(output).as_posix()
            info = path.lstat()
            if stat.S_ISDIR(info.st_mode):
                require(relative in parents and stat.S_IMODE(info.st_mode) == 0o755,
                        'unlisted or changed directory: ' + relative)
                walk(path)
            else:
                require(relative in files, 'unlisted output file: ' + relative)
                require(stat.S_IMODE(info.st_mode) == 0o644 and
                        regular_read(path, 16777216) == files[relative],
                        'existing output differs: ' + relative)
                found.add(relative)
    require(stat.S_IMODE(output.stat().st_mode) == 0o755, 'changed output directory mode')
    walk(output)
    require(found == set(files), 'missing output file')
    require(all((output / p).is_dir() for p in parents), 'missing output directory')


def build(args):
    """Publish the complete catalog once. Identical output is a no-op."""
    output = args.output.absolute()
    no_links(output)
    # Do not publish over inputs or inside either input directory.
    for source in (ROOT / 'build/modules', ROOT / 'build/example-modules', ROOT / 'src'):
        source = source.absolute()
        require(output != source and output not in source.parents and
                source not in output.parents, 'input/output ancestry overlap')
    files = payload(args.version)
    if output.exists():
        check_existing(output, files)
        return output
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.catalog-', dir=output.parent) as temporary:
        stage = Path(temporary) / 'payload'
        stage.mkdir(mode=0o755)
        for name, data in files.items():
            path = stage / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(0o644)
        for directory in [stage, *[p for p in stage.rglob('*') if p.is_dir()]]:
            directory.chmod(0o755)
        publish_directory(stage, output)
    return output


def main():
    """Parse paths and report one build diagnostic."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/catalog-0.3.1')
    parser.add_argument('--version', default='0.3.1')
    args = parser.parse_args()
    try:
        print(build(args))
    except (OSError, ValueError, AttributeError) as error:
        print('build-catalog: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
