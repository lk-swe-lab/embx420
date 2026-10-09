from pathlib import Path
import json, re, subprocess, hashlib, difflib, urllib.parse, shutil
repo=Path('/home/name/Documents/420-EMBDX/A');ev=repo/'logs/commands/cp-review'; snap=Path('/home/name/.hermes/cache/scratch/cp-independent-review/normal')
allow=(repo/'.tmp/cp-allowlist.txt').read_text().splitlines()
assert len(allow)==len(set(allow))
missing=[p for p in allow if not (repo/p).is_file()];assert not missing
# Include only proposed files, never scan excluded private AI logs/wiki/session history.
patterns={'assignment_secret':r'(?i)(?:api[_-]?key|password|passwd|secret|access[_-]?token)\s*[:=]\s*[\"\'][^\"\'\s]{6,}[\"\']','private_key':r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----','provider_key':r'(?:sk-[A-Za-z0-9_-]{24,}|gh[pousr]_[A-Za-z0-9]{20,})'}
hits=[]
for p in allow:
 q=repo/'.tmp/cp-index'/p if p in ['README.md','FILES.md'] else repo/p
 text=q.read_text(errors='replace')
 for name,regex in patterns.items():
  for m in re.finditer(regex,text): hits.append({'path':p,'kind':name,'line':text[:m.start()].count('\n')+1})
links=[]
for p in ['README.md','docs/commands/cp.md','docs/commands/shell.md','docs/reports/cp-implementation.md','docs/reports/cp.md']:
 q=snap/p;text=q.read_text()
 base=subprocess.run(['git','show','HEAD:'+p],cwd=repo,capture_output=True)
 old=base.stdout.decode() if base.returncode==0 else ''
 oldlinks=set(re.findall(r'\[[^\]]*\]\(([^)]+)\)',old))
 for url in re.findall(r'\[[^\]]*\]\(([^)]+)\)',text):
  if re.match(r'[A-Za-z]+:',url) or url.startswith('#'):continue
  target=(q.parent/urllib.parse.unquote(url.split('#')[0])).resolve()
  links.append({'path':p,'url':url,'target':str(target.relative_to(snap)) if target.is_relative_to(snap) else str(target),'exists':target.exists(),'introduced':url not in oldlinks})
changes={}
for p in ['Makefile','src/registry.c','tests/test_shell.py','tests/test_shell_module.c','docs/commands/cp.md','docs/commands/shell.md']:
 a=subprocess.check_output(['git','show','HEAD:'+p],cwd=repo).decode();b=(repo/p).read_text();changes[p]=''.join(difflib.unified_diff(a.splitlines(True),b.splitlines(True),fromfile='a/'+p,tofile='b/'+p))
(ev/'integration.patch').write_text(''.join(changes.values()))
for p in ['README.md','FILES.md']:
 a=subprocess.check_output(['git','show','HEAD:'+p],cwd=repo).decode();b=(repo/'.tmp/cp-index'/p).read_text();patch=''.join(difflib.unified_diff(a.splitlines(True),b.splitlines(True),fromfile='a/'+p,tofile='b/'+p))
 assert patch==(repo/'.tmp/cp-index'/(p+'.patch')).read_text()
 (ev/(p+'.original.patch')).write_text(patch)
 pcheck=subprocess.run(['git','apply','--check','-'],cwd=snap,input=patch.encode(),capture_output=True)
 # Snapshot already contains proposed versions; reverse check proves patch yields HEAD.
 reverse=subprocess.run(['git','apply','--reverse','--check','-'],cwd=snap,input=patch.encode(),capture_output=True)
 assert reverse.returncode==0
unchanged={}
for p in ['src/traverse.c','src/cmd_spec.h','src/traverse.h','tests/test_help.c','tests/test_recursive.c','tests/test_output.h']:
 a=subprocess.check_output(['git','show','HEAD:'+p],cwd=repo);unchanged[p]=a==(snap/p).read_bytes();assert unchanged[p]
manifest=json.loads((ev/'proposed-source-hashes.json').read_text())
changed_since={}
for p,h in manifest.items():
 q=repo/'.tmp/cp-index'/p if p in ['README.md','FILES.md'] else repo/p
 if hashlib.sha256(q.read_bytes()).hexdigest()!=h: changed_since[p]=True
r={'original_allowlist_count':len(allow),'missing_paths':missing,'credential_pattern_hits':hits,'links':links,'global_format_paths_match_HEAD':unchanged,'source_changes_since_snapshot':changed_since,'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo).decode().strip(),'index_empty':not subprocess.check_output(['git','diff','--cached','--name-only'],cwd=repo).strip()}
(ev/'scope-audit.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r,indent=2))
