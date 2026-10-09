from pathlib import Path
import json, subprocess, os, tempfile, hashlib, resource, re, shutil, difflib, urllib.parse
repo=Path('/home/name/Documents/420-EMBDX/A'); root=Path('/home/name/.hermes/cache/scratch/cp-independent-review/normal'); ev=repo/'logs/commands/cp-review'
results=[]
def run(label,argv,cwd=root,**kwargs):
 p=subprocess.run(argv,cwd=cwd,capture_output=True,timeout=60,**kwargs)
 for ext,b in [('stdout',p.stdout),('stderr',p.stderr),('status',str(p.returncode).encode()+b'\n')]: (ev/(label+'.'+ext)).write_bytes(b)
 results.append({'label':label,'argv':argv,'cwd':str(cwd),'status':p.returncode}); return p
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as t:
 f=Path(t); src=f/'source'; dst=f/'dest'; size=1048576*3+4097
 src.write_bytes((bytes(range(256))*(size//256+1))[:size])
 p=run('mechanism-strace',['strace','-yy','-o',str(ev/'mechanism.strace'),'-e','trace=openat,close,ftruncate,mmap,msync,munmap,read,write,sendfile,copy_file_range',str(root/'build/cp'),str(src),str(dst)])
 assert p.returncode==0 and src.read_bytes()==dst.read_bytes()
 trace=(ev/'mechanism.strace').read_text(); rows=[l for l in trace.splitlines() if str(src) in l or str(dst) in l or l.startswith('msync(')]
 (ev/'mechanism-filtered.txt').write_text('\n'.join(rows)+'\n')
 assert not any(re.match(r'(read|write|sendfile|copy_file_range)\(',l) and (str(src) in l or str(dst) in l) for l in trace.splitlines())
 assert sum(l.startswith('msync(') for l in trace.splitlines())==4
 # Separate independent large-memory check, not relying on existing suite.
 with src.open('wb') as out:
  out.truncate(96*1024*1024+4097)
  for pos in [0,1048575,1048576,96*1024*1024+4096]: out.seek(pos);out.write(b'X')
 def limit(): resource.setrlimit(resource.RLIMIT_AS,(24*1024*1024,24*1024*1024))
 p=run('large-24MiB-cap',[str(root/'build/cp'),str(src),str(dst)],preexec_fn=limit)
 def digest(path):
  h=hashlib.sha256()
  with path.open('rb') as f:
   for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
  return h.hexdigest()
 a,b=digest(src),digest(dst); assert p.returncode==0 and a==b and src.stat().st_size==dst.stat().st_size
 (ev/'large-observation.json').write_text(json.dumps({'size':src.stat().st_size,'RLIMIT_AS':24*1024*1024,'source_sha256':a,'dest_sha256':b},indent=2)+'\n')
 # Additional no-mutation parser probes, same-inode via destination symlink,
 # deeper missing self-descendant guard, and mixed success/error/help shell reuse.
 src.write_bytes(b'unchanged');dst.unlink();dst.symlink_to(src)
 for args in [['-f',str(src),str(dst)],['--bad','--help',str(src),str(dst)],['-r','-r',str(src),str(dst)]]:
  p=run('extra-'+str(len(results)),[str(root/'build/cp'),*args]);assert p.returncode in [1,2] and src.read_bytes()==b'unchanged' and dst.is_symlink()
 tree=f/'tree';tree.mkdir();(tree/'a').write_bytes(b'payload')
 p=run('self-deep',[str(root/'build/cp'),'-r',str(tree),str(tree/'notyet/inside')]);assert p.returncode==1 and not (tree/'notyet').exists()
 p=run('main-mixed-reuse',[str(root/'build/main')],cwd=f,input=b'cp missing copy\ncp source copy\nhelp cp\ncp --json source copy\npwd\n');assert p.returncode==0 and (f/'copy').read_bytes()==b'unchanged' and p.stdout.endswith(os.fsencode(f)+b'\n')
# Strengthen existing fixture in scratch only: exercise faults in every mapping
# position across three windows, not merely the first source/dest pair.
original=(root/'tests/test_cp_module.c').read_text()
extra=original.replace('unsigned char bytes[4097];','unsigned char bytes[2097153];').replace('assert(mapped_calls == 2);','assert(mapped_calls == 6);').replace('for (int i = 1; i <= 2; ++i)','for (int i = 1; i <= 6; ++i)')
(root/'tests/test_cp_module.c').write_text(extra)
p=run('multiwindow-fault-build',['make','build/test_cp_module']);assert p.returncode==0
p=run('multiwindow-fault-module',['./build/test_cp_module']);assert p.returncode==0
(root/'tests/test_cp_module.c').write_text(original)
(ev/'multiwindow-fault-fixture.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),extra.splitlines(True),fromfile='original fixture',tofile='scratch stronger fixture')))
# SIGBUS remains isolated; fresh rebuild of documented deterministic wrapper.
p=run('sigbus-build',['cc','-std=c17','-g3','-O0','-I'+str(repo/'build/argtable3/include'),str(repo/'logs/commands/cp-implementation/sigbus_probe.c'),str(root/'src/cmd_cp.c'),str(root/'src/cmd_lines.c'),'-Wl,--wrap=mmap','-L'+str(repo/'build/argtable3/lib'),'-largtable3','-lm','-o',str(root/'build/sigbus_probe')]);
if p.returncode==0:
 with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as t:
  p=run('sigbus-isolated',['./build/sigbus_probe'],cwd=root,env={**os.environ,'TMPDIR':t})
  assert p.returncode==-7
(ev/'extra-commands.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
