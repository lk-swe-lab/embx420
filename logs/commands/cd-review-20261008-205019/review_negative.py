import json,os,shutil,subprocess,hashlib
from pathlib import Path
R=Path('/home/name/Documents/420-EMBDX/A'); S=Path('/home/name/.hermes/cache/scratch/cd-review-20261008-205019'); T=S/'tree'; D=R/'logs/commands/cd-review-20261008-205019'
E=json.loads((D/'evidence.json').read_text())
def run(label,args):
 p=subprocess.run(args,cwd=T,env={**os.environ,'TMPDIR':str(S),'LC_ALL':'C'},stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=60)
 for suffix,data in [('stdout',p.stdout),('stderr',p.stderr)]: (D/(label+'.'+suffix)).write_bytes(data)
 E['records'].append({'label':label,'argv':list(map(str,args)),'cwd':str(T),'status':p.returncode})
 print(label,p.returncode,p.stdout.decode(errors='replace'),p.stderr.decode(errors='replace')); return p
mutant=(T/'src/cmd_cd.c').read_text().replace('arg_parse(argc, copy, table)','arg_parse(argc, argv, table)'); assert mutant!=(T/'src/cmd_cd.c').read_text()
(S/'borrowed_argv.c').write_text(mutant)
args=['cc','-std=c17','-Wall','-Wextra','-Wpedantic','-g3','-O0','-Isrc','-Ibuild/argtable3/include','tests/test_cd_module.c',str(S/'borrowed_argv.c'),'-Wl,--wrap=arg_lit0,--wrap=arg_file0,--wrap=arg_end,--wrap=arg_parse,--wrap=malloc,--wrap=free,--wrap=chdir','-Lbuild/argtable3/lib','-largtable3','-lm','-o',str(S/'module-borrowed-argv')]
assert run('negative-argv-build',args).returncode==0
assert run('negative-argv',[str(S/'module-borrowed-argv')]).returncode==-6
shutil.copy2(S/'borrowed_argv.c',D/'borrowed_argv-mutant.c'); shutil.copy2(S/'no_chdir.c',D/'no_chdir-mutant.c')
old=Path('/home/name/.hermes/cache/scratch/cd-resume-20261008-203032'); prior=D/'prior-worker'; prior.mkdir(exist_ok=True)
selected=['evidence.json','research.json','persistent-demo.json']+[label+'.'+suffix for label in ['final-80-tests','final-80-lint','final-80-format-check','final-80-san-main','final-80-san-module','delivery-proof-internal-oom','borrowed-sensitive-red','borrowed-sensitive-green','slice1-red-resumed'] for suffix in ['json','stdout','stderr']]
E['preserved_prior_evidence']=[]
for name in selected:
 if (old/name).is_file():
  shutil.copy2(old/name,prior/name); E['preserved_prior_evidence'].append(name)
E['owned_sources_unchanged']={p:hashlib.sha256((R/p).read_bytes()).hexdigest()==h for p,h in E['repository_cd_hashes'].items()}
assert all(E['owned_sources_unchanged'].values())
E['review_verdict']={'passed':False,'security_concerns':[],'logic_errors':['arg_parse overwrites caller exported getopt globals; existing module tests do not detect this'],'suggestions':['Add sentinel preservation checks and agreed exported-global restoration policy; do not promise private getopt state or thread safety.'],'summary':'Selected cd behavior passes, but full host-global ownership acceptance requires correction or an explicit limitation decision.'}
(D/'evidence.json').write_text(json.dumps(E,indent=2)+'\n')
shutil.copy2('/home/name/.hermes/cache/scratch/cd-review-negative.py',D/'review_negative.py')
print('preserved prior records',len(E['preserved_prior_evidence']))
