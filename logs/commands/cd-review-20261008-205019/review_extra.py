import os,json,subprocess,shutil,hashlib,tempfile,random
from pathlib import Path
R=Path('/home/name/Documents/420-EMBDX/A'); S=Path('/home/name/.hermes/cache/scratch/cd-review-20261008-205019'); T=S/'tree'; D=R/'logs/commands/cd-review-20261008-205019'
E=json.loads((D/'evidence.json').read_text()); before=os.getcwd()
env={**os.environ,'TMPDIR':str(S),'LC_ALL':'C','PYTHONPYCACHEPREFIX':str(S/'pycache')}
def run(label,args,cwd=T,input=None,extra=None):
 p=subprocess.run(args,cwd=cwd,env={**env,**(extra or {})},input=input,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
 for suffix,data in [('stdout',p.stdout),('stderr',p.stderr)]: (D/(label+'.'+suffix)).write_bytes(data)
 rec={'label':label,'argv':list(map(str,args)),'cwd':str(cwd),'status':p.returncode,'environment_overrides':extra or {}}
 if input is not None: rec['stdin']=input.decode(errors='backslashreplace')
 E['records'].append(rec); (D/'evidence.json').write_text(json.dumps(E,indent=2)+'\n')
 print(label,p.returncode,p.stdout.decode(errors='replace')[-800:],p.stderr.decode(errors='replace')[-800:]); return p
flags=['-std=c17','-Wall','-Wextra','-Wpedantic','-Wformat=2','-Wshadow','-Wconversion','-Wstrict-prototypes','-g3','-O0']
assert run('global-probe-build',['cc',*flags,'-Isrc','-Ibuild/argtable3/include',str(S/'global_probe.c'),'src/cmd_cd.c','-Lbuild/argtable3/lib','-largtable3','-lm','-o',str(S/'global_probe')]).returncode==0
p=run('global-probe',[str(S/'global_probe')]); E['global_state_probe_failed']=p.returncode!=0
shutil.copy2(S/'global_probe.c',D/'global_probe.c')
shutil.copy2(T/'build/main',S/'main-normal'); shutil.copy2(T/'build/test_cd_module',S/'module-normal')
with tempfile.TemporaryDirectory(prefix='independent-',dir=S) as name:
 root=Path(name); (root/'real/nested').mkdir(parents=True); (root/'alias').symlink_to('real/nested'); (root/'blocked').mkdir(); (root/'blocked').chmod(0); (root/'file').write_text('data'); (root/'loop').symlink_to('loop')
 script='pwd\ncd alias\npwd\ncd ..\npwd\ncd ../blocked\npwd\ncd ../file\npwd\ncd ../loop\npwd\ncd ../missing\npwd\ncd\npwd\n'
 expected=[root,root/'real/nested',root/'real',root/'real',root/'real',root/'real',root/'real',root]
 p=run('independent-persistent',[str(S/'main-normal')],cwd=root,input=script.encode(),extra={'HOME':str(root),'PWD':'stale','OLDPWD':'old','CDPATH':'/ignored'})
 assert p.returncode==0 and p.stdout==b''.join(os.fsencode(x)+b'\n' for x in expected)
 assert b'Permission denied' in p.stderr and b'Not a directory' in p.stderr and b'Too many levels' in p.stderr and b'No such file' in p.stderr
 (root/'blocked').chmod(0o700)
 # Bounded deterministic state-path property check in one real shell process.
 current=root; expected=[]; lines=[]; rng=random.Random(420)
 for i in range(100):
  dest=rng.choice(['real/nested','real','absent','file','loop'])
  lines.extend(['cd '+str(root/dest),'pwd'])
  if dest in ['real/nested','real']: current=root/dest
  expected.append(current)
 p=run('persistent-100-transitions',[str(S/'main-normal')],cwd=root,input=('\n'.join(lines)+'\n').encode())
 assert p.returncode==0 and p.stdout==b''.join(os.fsencode(x)+b'\n' for x in expected) and p.stderr
 E['persistent_property_transitions']=100
# Existing assertions must reject a module without real chdir.
mutant=(T/'src/cmd_cd.c').read_text().replace('if (chdir(path) != 0)', 'if (0 != 0)')
(S/'no_chdir.c').write_text(mutant)
main_sources=['src/main.c','src/traverse.c',str(S/'no_chdir.c'),'src/shell.c','src/registry.c','src/cmd_cat.c','src/cmd_head.c','src/cmd_tail.c','src/cmd_pwd.c','src/cmd_stat.c','src/cmd_ls.c','src/cmd_mkdir.c','src/cmd_touch.c','src/cmd_lines.c']
assert run('negative-no-chdir-build',['cc',*flags,'-Isrc','-Ibuild/argtable3/include',*main_sources,'-Lbuild/argtable3/lib','-largtable3','-lm','-o',str(S/'main-no-chdir')]).returncode==0
assert run('negative-no-chdir',['python3','tests/test_cd.py',str(S/'main-no-chdir'),'--test','test_absolute_relative_persistence']).returncode==1
# ASan/UBSan: instrument command, module harness and scratch main; library remains prebuilt.
cf=' '.join(flags+['-fsanitize=address,undefined','-fno-omit-frame-pointer'])
assert run('sanitizer-build',['make','-B','build/main','build/test_cd_module','CFLAGS='+cf]).returncode==0
san={'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
assert run('sanitizer-module',[str(T/'build/test_cd_module')],extra=san).returncode==0
assert run('sanitizer-main',['python3','tests/test_cd.py',str(T/'build/main')],extra=san).returncode==0
E['extra_driver_cwd_before']=before; E['extra_driver_cwd_after']=os.getcwd(); assert before==os.getcwd()
E['owned_sources_unchanged']={p:hashlib.sha256((R/p).read_bytes()).hexdigest()==h for p,h in E['repository_cd_hashes'].items()}
assert all(E['owned_sources_unchanged'].values())
(D/'evidence.json').write_text(json.dumps(E,indent=2)+'\n')
shutil.copy2('/home/name/.hermes/cache/scratch/cd-review-run.py',D/'review_run.py'); shutil.copy2('/home/name/.hermes/cache/scratch/cd-review-extra.py',D/'review_extra.py')
