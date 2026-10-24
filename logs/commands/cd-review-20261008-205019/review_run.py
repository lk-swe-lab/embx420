import os, json, shutil, subprocess, hashlib
from pathlib import Path
R=Path('/home/name/Documents/420-EMBDX/A')
S=Path('/home/name/.hermes/cache/scratch/cd-review-20261008-205019')
D=R/'logs/commands/cd-review-20261008-205019'
S.mkdir(parents=True,exist_ok=True); D.mkdir(parents=True,exist_ok=True)
T=S/'tree'; T.mkdir(exist_ok=True)
for name in ['src','tests']:
    (T/name).mkdir(exist_ok=True)
for p in (R/'src').glob('*'):
    if p.suffix in ['.c','.h']: shutil.copy2(p,T/'src'/p.name)
for name in ['test_cd.py','test_cd_module.c']:
    shutil.copy2(R/'tests'/name,T/'tests'/name)
for name in ['Makefile','.clang-format']:
    if (R/name).exists(): shutil.copy2(R/name,T/name)
(T/'build').mkdir(exist_ok=True)
for name in ['argtable3','cunit']:
    (T/'build'/name).symlink_to(R/'build'/name,target_is_directory=True)
E={'session':'20261008_205019_2a746d','euid':os.geteuid(),'driver_cwd_before':os.getcwd(),'records':[]}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
E['original_hashes']={str(p.relative_to(T)):sha(p) for p in T.rglob('*') if p.is_file() and 'build' not in p.parts}
E['dependency_hash']=sha(R/'build/argtable3/lib/libargtable3.a')
E['repository_cd_hashes']={p:sha(R/p) for p in ['src/cmd_cd.c','src/cmd_cd.h','tests/test_cd.py','tests/test_cd_module.c','docs/commands/cd.md','docs/reports/cd.md']}
env={**os.environ,'TMPDIR':str(S),'LC_ALL':'C','PYTHONPYCACHEPREFIX':str(S/'pycache')}
def run(label,args,cwd=T,input=None,extra=None):
    result=subprocess.run(args,cwd=cwd,env={**env,**(extra or {})},input=input,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
    (D/(label+'.stdout')).write_bytes(result.stdout); (D/(label+'.stderr')).write_bytes(result.stderr)
    item={'label':label,'argv':list(map(str,args)),'cwd':str(cwd),'status':result.returncode,'environment_overrides':extra or {}}
    if input is not None: item['stdin']=input.decode(errors='backslashreplace')
    E['records'].append(item); (D/'evidence.json').write_text(json.dumps(E,indent=2)+'\n')
    print(label,result.returncode,result.stdout.decode(errors='replace')[-1400:],result.stderr.decode(errors='replace')[-1400:])
    return result
patch=Path('/home/name/.hermes/cache/scratch/cd-resume-20261008-203032/integration.patch')
shutil.copy2(patch,D/'integration.patch')
run('patch-check',['git','apply','--check',str(patch)],cwd=R)
assert run('scratch-patch',['git','apply',str(patch)]).returncode==0
assert run('test-cd',['make','test-cd']).returncode==0
assert run('lint',['make','lint','C_SOURCES=src/cmd_cd.c tests/test_cd_module.c']).returncode==0
assert run('format',['make','format-check','C_FILES=src/cmd_cd.c src/cmd_cd.h tests/test_cd_module.c']).returncode==0
run('internal-oom',[str(T/'build/test_cd_module'),'--internal-oom'])
run('python-syntax',['python3','-m','py_compile','tests/test_cd.py'])
E['driver_cwd_after']=os.getcwd(); assert E['driver_cwd_before']==E['driver_cwd_after']
E['reviewed_hashes']={str(p.relative_to(T)):sha(p) for p in T.rglob('*') if p.is_file() and 'build' not in p.parts}
(D/'evidence.json').write_text(json.dumps(E,indent=2)+'\n')
shutil.copytree(T/'src',D/'snapshot/src',dirs_exist_ok=True)
shutil.copytree(T/'tests',D/'snapshot/tests',dirs_exist_ok=True)
shutil.copy2(T/'Makefile',D/'snapshot/Makefile')
shutil.copy2(T/'.clang-format',D/'snapshot/.clang-format')
print('EVIDENCE',D)
