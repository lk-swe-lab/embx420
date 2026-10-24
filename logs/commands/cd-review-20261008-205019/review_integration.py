import os,json,subprocess,shutil,hashlib
from pathlib import Path
R=Path('/home/name/Documents/420-EMBDX/A'); S=Path('/home/name/.hermes/cache/scratch/cd-review-20261008-205019'); T=S/'tree'; D=R/'logs/commands/cd-review-20261008-205019'
E=json.loads((D/'evidence.json').read_text())
def run(label,args,cwd=T,extra=None,input=None):
 p=subprocess.run(args,cwd=cwd,env={**os.environ,'TMPDIR':str(S),'LC_ALL':'C',**(extra or {})},input=input,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
 for suffix,data in [('stdout',p.stdout),('stderr',p.stderr)]: (D/(label+'.'+suffix)).write_bytes(data)
 rec={'label':label,'argv':list(map(str,args)),'cwd':str(cwd),'status':p.returncode}
 if input: rec['stdin']=input.decode()
 E['records'].append(rec); (D/'evidence.json').write_text(json.dumps(E,indent=2)+'\n')
 print(label,p.returncode,p.stdout.decode(errors='replace')[-400:],p.stderr.decode(errors='replace')[-2500:]); return p
# Extra default-HOME failure invariants; unset truly removes the inherited entry.
for label,home in [('unset',None),('empty',''),('missing',str(S/'not-a-real-home')),('file',str(D/'evidence.json'))]:
 childenv=dict(os.environ)
 if home is None: childenv.pop('HOME',None)
 else: childenv['HOME']=home
 # subprocess env is complete here (not merged with an inherited HOME).
 p=subprocess.run([str(S/'main-normal')],cwd=S,env={**childenv,'LC_ALL':'C'},input=b'cd\npwd\n',stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
 assert p.returncode==0 and p.stdout==os.fsencode(S)+b'\n' and p.stderr
 (D/('home-'+label+'.stdout')).write_bytes(p.stdout); (D/('home-'+label+'.stderr')).write_bytes(p.stderr)
 E['records'].append({'label':'home-'+label,'argv':[str(S/'main-normal')],'cwd':str(S),'status':p.returncode,'stdin':'cd\npwd\n','HOME':home,'home_unset':home is None})
for label in ['shell-regression-added-cd','baseline-shell-regression']:
 for rec in E['records']:
  if rec['label']==label:
   rec['label']=label+'-missing-fixture-setup'
 for suffix in ['stdout','stderr']:
  old=D/(label+'.'+suffix)
  if old.exists(): old.rename(D/(label+'-missing-fixture-setup.'+suffix))
shutil.copytree(R/'tests/fixtures',T/'tests/fixtures',dirs_exist_ok=True)
shutil.copytree(R/'tests/fixtures',D/'snapshot/tests/fixtures',dirs_exist_ok=True)
for name in ['test_shell.py','test_shell_module.c']:
 shutil.copy2(R/'tests'/name,T/'tests'/name); shutil.copy2(R/'tests'/name,D/'snapshot/tests'/name)
assert run('shell-build',['make','-B','all','build/test_shell_module']).returncode==0
assert run('shell-regression-added-cd',['python3','tests/test_shell.py','--program','./build/main','--standalone-dir','./build']).returncode==1
assert run('shell-module-added-cd',[str(T/'build/test_shell_module')]).returncode==-6
# Baseline identity check: use the original registry, without the cd addition.
assert hashlib.sha256((R/'src/registry.c').read_bytes()).hexdigest()==E['original_hashes']['src/registry.c']
shutil.copy2(R/'src/registry.c',S/'baseline_registry.c'); shutil.copy2(R/'src/registry.c',D/'baseline_registry.c')
flags=['-std=c17','-Wall','-Wextra','-Wpedantic','-g3','-O0','-Isrc','-Ibuild/argtable3/include']
sources=['src/shell.c',str(S/'baseline_registry.c'),'src/cmd_cat.c','src/cmd_head.c','src/cmd_tail.c','src/cmd_pwd.c','src/cmd_stat.c','src/cmd_ls.c','src/cmd_mkdir.c','src/cmd_touch.c','src/cmd_lines.c']
libs=['-Lbuild/argtable3/lib','-largtable3','-lm']
assert run('baseline-main-build',['cc',*flags,'src/main.c','src/traverse.c',*sources,*libs,'-o',str(S/'baseline-main')]).returncode==0
assert run('baseline-shell-regression',['python3','tests/test_shell.py','--program',str(S/'baseline-main'),'--standalone-dir','./build']).returncode==0
assert run('baseline-shell-module-build',['cc',*flags,'tests/test_shell_module.c',*sources,'-Wl,--wrap=malloc,--wrap=realloc,--wrap=read',*libs,'-o',str(S/'baseline-shell-module')]).returncode==0
assert run('baseline-shell-module',[str(S/'baseline-shell-module')]).returncode==0
E['integration_test_blocker']='Existing Python exact shell help expects eight commands (3 failures); C registry count asserts eight (SIGABRT). Fresh pre-cd baseline passes both.'
E['owned_sources_unchanged']={p:hashlib.sha256((R/p).read_bytes()).hexdigest()==h for p,h in E['repository_cd_hashes'].items()}; assert all(E['owned_sources_unchanged'].values())
(D/'evidence.json').write_text(json.dumps(E,indent=2)+'\n'); shutil.copy2('/home/name/.hermes/cache/scratch/cd-review-integration.py',D/'review_integration.py')
