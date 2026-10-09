from pathlib import Path
import subprocess, tarfile, io, shutil, json, hashlib, os, concurrent.futures
repo=Path('/home/name/Documents/420-EMBDX/A')
evidence=repo/'logs/commands/cp-review'; evidence.mkdir(parents=True,exist_ok=True)
root=Path('/home/name/.hermes/cache/scratch/cp-independent-review'); root.mkdir(exist_ok=True)
allow=(repo/'.tmp/cp-allowlist.txt').read_text().splitlines()
data=subprocess.check_output(['git','archive','HEAD'],cwd=repo)
for name in ['normal','sanitizer']:
 target=root/name; target.mkdir(exist_ok=True)
 with tarfile.open(fileobj=io.BytesIO(data)) as t: t.extractall(target,filter='data')
 for p in allow:
  q=repo/'.tmp/cp-index'/p if p in ['README.md','FILES.md'] else repo/p
  dest=target/p; dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(q,dest)
 (target/'build').mkdir(exist_ok=True)
 for dep in ['argtable3','cunit']:
  d=target/'build'/dep
  if not d.exists(): d.symlink_to(repo/'build'/dep,target_is_directory=True)
manifest={p:hashlib.sha256((root/'normal'/p).read_bytes()).hexdigest() for p in allow}
(evidence/'proposed-source-hashes.json').write_text(json.dumps(manifest,indent=2)+'\n')
commands=[]
def run(label,argv,where='normal',env=None):
 cwd=root/where
 e=os.environ.copy();e.update(env or {})
 p=subprocess.run(argv,cwd=cwd,env=e,capture_output=True,timeout=300)
 for suffix,blob in [('stdout',p.stdout),('stderr',p.stderr),('status',str(p.returncode).encode()+b'\n')]: (evidence/(label+'.'+suffix)).write_bytes(blob)
 commands.append({'label':label,'argv':argv,'cwd':str(cwd),'env':env or {},'status':p.returncode})
 print(label,p.returncode,flush=True)
 return p
flags='-std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes -g3 -O0 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie'
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as x:
 jobs=[x.submit(run,'fresh-build-regression',['make','all','test','test-cp'], 'normal'),x.submit(run,'fresh-default-sanitizer',['make','all','test','CFLAGS='+flags,'LDFLAGS=-no-pie -fsanitize=address,undefined'],'sanitizer',{'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})]
 for j in jobs: j.result()
run('targeted',['make','test-cp-impl','test-cp','test-shell'])
run('lint',['make','lint'])
run('scoped-format',['clang-format','--dry-run','--Werror','src/cmd_cp.c','src/cmd_cp.h','src/cp_main.c','src/registry.c','tests/test_cp_module.c','tests/test_shell_module.c'])
run('global-format',['make','format-check'])
run('main-help',['./build/main','help'])
run('cp-main-help',['./build/main','help','cp'])
run('cp-standalone-help',['./build/cp','--help'])
(evidence/'execution-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
print('snapshot',root,flush=True)
