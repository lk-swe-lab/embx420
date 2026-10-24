#!/usr/bin/env python3
"""Capture actual native-module lifecycle output, without rebuilding artifacts."""

import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HERE = Path(__file__).resolve().parent
TIMEOUT = 10


def require(condition, message):
    """Keep checks active under python -O too."""
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree_hashes(directory):
    return {str(p.relative_to(directory)): digest(p)
            for p in sorted(directory.rglob('*')) if p.is_file()}


def main():
    record = {"schema_version": 2, "status": "FAIL", "events": [], "checks": []}
    lines = ["Native .so lifecycle demo (real execution; no rebuild)"]
    fixture = None

    def check(condition, message):
        require(condition, message)
        record["checks"].append(message)
        lines.append("CHECK: " + message)

    def run(program, args, env, cwd, expected=0, commands=None, stderr=""):
        argv = [str(program), *map(str, args)]
        text = '' if commands is None else '\n'.join(commands) + '\n'
        event = {"argv": argv, "cwd": str(cwd), "env": dict(env),
                 "stdin": text, "commands": commands or [],
                 "timeout_seconds": TIMEOUT, "expected_status": expected}
        record["events"].append(event)
        lines.append("\n$ " + shlex.join(argv))
        if commands:
            lines.extend(["stdin (one running shell):", text.rstrip('\n')])
        try:
            result = subprocess.run(argv, input=text.encode(), capture_output=True,
                                    env=env, cwd=cwd, timeout=TIMEOUT, check=False)
        except subprocess.TimeoutExpired as error:
            event.update(status=None, timed_out=True,
                         stdout=(error.stdout or b'').decode('utf-8', 'replace'),
                         stderr=(error.stderr or b'').decode('utf-8', 'replace'))
            lines.extend([event['stdout'], event['stderr'], '[TIMEOUT]'])
            raise
        event.update(status=result.returncode, stdout=result.stdout.decode(),
                     stderr=result.stderr.decode())
        for key in ('stdout', 'stderr'):
            if event[key]:
                lines.extend([key + ':', event[key].rstrip('\n')])
        lines.append(f"[process status={result.returncode}; expected={expected}]")
        require(result.returncode == expected, 'unexpected process status')
        require(event['stderr'] == stderr, 'unexpected stderr')
        return event['stdout']

    def catalog(text, installed):
        value = json.loads(text)
        require(set(value) == {'schema_version', 'commands'}
                and type(value['schema_version']) is int
                and value['schema_version'] == 1, 'catalog schema mismatch')
        entries = value['commands']
        names = [item['name'] for item in entries]
        require(names == sorted(set(names)) and 'pkg' in names,
                'catalog names not sorted/unique or pkg missing')
        for item in entries:
            require(set(item) == {'name', 'summary', 'origin', 'package', 'version'}
                    and isinstance(item['summary'], str) and item['summary'],
                    'catalog fields mismatch')
            if item['name'] == 'hello':
                require(installed and item == {
                    'name': 'hello', 'summary': 'Print a greeting.',
                    'origin': 'package', 'package': 'hello', 'version': version},
                    'hello catalog identity mismatch')
            else:
                # Compatibility labels are not the physical linkage model.
                require(item['origin'] == ('control' if item['name'] in
                        ('help', 'exit') else 'builtin')
                        and item['package'] is None and item['version'] is None,
                        'standard-module catalog identity mismatch')
        check(('hello' in names) == installed, 'catalog hello presence=' + str(installed))
        check(len(names) == len(modules) + int(installed), 'catalog count matches module inventory')
        return value

    try:
        tmpbase = os.environ.get('TMPDIR')
        require(tmpbase and Path(tmpbase).is_absolute() and Path(tmpbase).is_dir(),
                'set TMPDIR to an existing absolute user-owned scratch directory')
        source = ROOT / 'build/aishell'
        identity = json.loads((ROOT / 'packages/hello-so/pkg.json').read_text())
        version = identity['version']
        require(identity['schema_version'] == 2 and identity['name'] == 'hello'
                and identity['commands'] == ['hello'], 'expected native hello package')
        bundle = ROOT / 'build/native-candidates' / ('hello-' + version)
        release_id = json.loads((ROOT / 'packages/aishell/release.json').read_text())
        release = ROOT / 'build/so-reviewed-releases' / (release_id['name'] + '-' + release_id['version'])
        modules = sorted((ROOT / 'build/modules').glob('*.so'))
        require(source.is_file() and os.access(source, os.X_OK), 'missing build/aishell')
        require((ROOT / 'build/main').resolve() == source, 'main alias mismatch')
        require(len(modules) == 22, 'expected 22 standard .so modules')
        require(json.loads((bundle / 'pkg.json').read_text()) == identity,
                'generated bundle identity differs from source')
        require((release / 'bin/aishell').is_file(), 'missing full-shell release')
        inputs = {str(source): digest(source), **{str(p): digest(p) for p in modules}}
        record['inputs'] = {'sha256': inputs, 'package': str(bundle),
                            'package_sha256': tree_hashes(bundle),
                            'release': str(release), 'release_sha256': tree_hashes(release)}
        record['runner_cwd'] = str(Path.cwd())
        record['input_file_count'] = (len(inputs)
                                      + len(record['inputs']['package_sha256'])
                                      + len(record['inputs']['release_sha256']))
        require(digest(release / 'bin/aishell') == digest(source)
                and tree_hashes(release / 'bin/modules') ==
                {p.name: digest(p) for p in modules}
                and tree_hashes(release / 'examples' / ('hello-' + version)) ==
                tree_hashes(bundle), 'release differs from current host/modules/package')
        lines.append('Runner cwd: ' + record['runner_cwd'])
        lines.append('Input SHA-256 entries: ' + str(record['input_file_count']))
        lines.append('Host SHA-256: ' + digest(source))
        lines.append('Hello module SHA-256: ' + digest(bundle / 'lib/hello.so'))
        fixture = tempfile.TemporaryDirectory(prefix='packaging-demo-', dir=tmpbase)
        space = Path(fixture.name)
        home, private_tmp = space / 'home', space / 'tmp'
        home.mkdir(mode=0o700)
        private_tmp.mkdir(mode=0o700)
        env = {'HOME': str(home), 'TMPDIR': str(private_tmp),
               'LC_ALL': 'C', 'PATH': '/nonexistent'}
        record['namespace'] = {'root': str(space), 'env': env}
        lines.append('Namespace: ' + str(space))
        lines.append('Child environment: ' + json.dumps(env, sort_keys=True))
        check(run(source, ['pkg', 'list'], env, space) == ''
              and not (home / '.mysh').exists(), 'initial package state empty')
        baseline = catalog(run(source, ['--commands-json'], env, space), False)
        unknown = 'main: hello: unknown command\n'
        commands = ['hello', 'pkg install ' + shlex.quote(str(bundle)),
                    'pkg list', 'pkg info hello', 'help hello',
                    'hello --name "Week 4"', 'help --json', 'pkg remove hello',
                    'pkg list', 'help --json', 'hello', 'pkg use hello ' + version,
                    'hello --name restored', 'pkg remove hello', 'hello', 'exit']
        out = run(source, [], env, space, expected=127, commands=commands,
                  stderr=unknown * 3)
        check(out.startswith(f'Installed hello {version}\nhello {version} active\n'),
              'same running shell installs and lists native hello')
        check('Description: ' + identity['description'] + '\nCommands: hello\n' in out,
              'info includes manifest description and commands')
        check('Usage: hello' in out and '--name' in out and 'Hello, Week 4!\n' in out,
              'same running shell discovers help and executes multiword greeting')
        catalogs = [line for line in out.splitlines() if line.startswith('{"schema_version":')]
        require(len(catalogs) == 2, 'missing loop catalog observations')
        catalog(catalogs[0], True)
        check(catalog(catalogs[1], False) == baseline, 'remove restores baseline catalog in same shell')
        removed = 'Removed hello (deactivated; immutable versions retained)\n'
        check(out.count(removed) == 2 and f'hello {version} retained\n' in out,
              'remove hides commands while retaining installed version')
        check(f'Selected hello {version}\nHello, restored!\n' in out,
              'use reactivates the retained same-version module without restart')
        check(record['events'][-1]['status'] == 127,
              'exit preserves final unknown-command status in same shell')
        installed = home / '.mysh/pkgs' / ('hello-' + version)
        active = home / '.mysh/pkgs/.active/hello'
        check(installed.is_dir() and not active.exists(), 'deactivated payload retained; active selection absent')
        check(tree_hashes(installed) == tree_hashes(bundle), 'installed manifest and payload bytes match bundle')
        check(run(source, ['pkg', 'list'], env, space) == f'hello {version} retained\n',
              'fresh process sees retained inactive version')
        check(run(source, ['hello'], env, space, expected=127, stderr=unknown) == '',
              'fresh process cannot execute removed command; no PATH fallback')
        check(run(source, ['pkg', 'use', 'hello', version], env, space) == f'Selected hello {version}\n'
              and active.read_text() == version, 'use persists exact active version')
        check(run(source, ['hello', '--name', 'fresh'], env, space) == 'Hello, fresh!\n',
              'fresh process loads selected retained version')
        check(run(source, ['pkg', 'remove', 'hello'], env, space) == removed,
              'final source-shell selection deactivated')

        relocated = space / 'relocated-release'
        shutil.copytree(release, relocated)
        binary = relocated / 'bin/aishell'
        check(tree_hashes(relocated) == tree_hashes(release), 'complete release copied byte-for-byte outside repository')
        release_home = space / 'release-home'
        release_home.mkdir(mode=0o700)
        release_env = dict(env, HOME=str(release_home))
        record['relocated_release'] = {'source': str(release), 'binary': str(binary),
                                       'sha256': digest(binary)}
        check(run(binary, ['pwd'], release_env, space) == str(space) + '\n',
              'relocated shell resolves its modules from unrelated cwd')
        check('pkg use NAME VERSION' in run(binary, ['help', 'pkg'], release_env, space),
              'relocated pkg help exposes version selection')
        example = relocated / 'examples' / ('hello-' + version)
        moved_commands = ['hello', 'pkg install ' + shlex.quote(str(example)),
                          'hello --name relocated', 'pkg remove hello', 'hello', 'exit']
        out = run(binary, [], release_env, space, expected=127,
                  commands=moved_commands, stderr=unknown * 2)
        check(out == f'Installed hello {version}\nHello, relocated!\n' + removed,
              'relocated release installs/runs/hides included example in one shell')
        check(tree_hashes(release_home / '.mysh/pkgs' / ('hello-' + version)) == tree_hashes(example)
              and not (release_home / '.mysh/pkgs/.active/hello').exists(),
              'relocated example retained intact with no active selection')
        check(all(digest(Path(path)) == value for path, value in inputs.items())
              and tree_hashes(bundle) == record['inputs']['package_sha256']
              and tree_hashes(release) == record['inputs']['release_sha256'],
              'host, standard modules, package and release hashes unchanged')
        record['status'] = 'PASS'
    except (OSError, ValueError, KeyError, TypeError, RuntimeError,
            subprocess.SubprocessError) as error:
        record['error'] = str(error)
        lines.append('ERROR: ' + str(error))
    finally:
        if fixture is not None:
            fixture_root = Path(fixture.name)
            fixture.cleanup()
            record['cleanup_verified'] = not fixture_root.exists()
            if not record['cleanup_verified']:
                record['status'] = 'FAIL'
            lines.append('Fixture removed: ' + str(record['cleanup_verified']))
        record['process_count'] = len(record['events'])
        record['loop_input_count'] = sum(len(e['commands']) for e in record['events'])
        record['check_count'] = len(record['checks'])
        lines.append('RESULT: ' + record['status'])
        (HERE / 'packaging-demo.json').write_text(json.dumps(record, indent=2) + '\n')
        (HERE / 'packaging-demo.log').write_text('\n'.join(lines) + '\n')
    print(f"{record['status']}: {record['process_count']} processes; "
          f"{record['loop_input_count']} loop inputs; {record['check_count']} checks; "
          'see demo/packaging-demo.log and .json')
    return 0 if record['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
