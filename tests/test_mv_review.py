import os,sys,socket,stat,subprocess,tempfile,pathlib,json,itertools
T=pathlib.Path(sys.argv[1]).resolve(); count=0

def entry(root,name,kind):
 p=root/name
 if kind=='absent': return
 if kind in ['dir','tree']: p.mkdir(); (p/'child').write_bytes(b'child') if kind=='tree' else None
 elif kind=='file': p.write_bytes(b'payload:'+name.encode()); p.chmod(0o640)
 elif kind=='fifo': os.mkfifo(p,0o620)
 elif kind=='socket':
  s=socket.socket(socket.AF_UNIX); s.bind(str(p)); s.close()
 elif kind=='dangling': p.symlink_to('missing')
 elif kind=='cycle': p.symlink_to(name)
 elif kind=='badlink': p.symlink_to('referent/child')
 elif kind=='filelink': p.symlink_to('referent')
 elif kind=='dirlink': p.symlink_to('container',target_is_directory=True)

def snapshot(root):
 out={}
 def visit(parent):
  for p in sorted(parent.iterdir()):
   s=p.lstat(); k=str(p.relative_to(root)); out[k]=(s.st_ino,s.st_dev,s.st_mode,s.st_uid,s.st_gid,s.st_size if not stat.S_ISDIR(s.st_mode) else None, s.st_mtime_ns if not stat.S_ISDIR(s.st_mode) else None,os.readlink(p) if p.is_symlink() else p.read_bytes() if p.is_file() else None)
   if stat.S_ISDIR(s.st_mode): visit(p)
 visit(root); return out

def invoke(root,mode,args):
 global count
 cmd=[str(T/'build'/('mv' if mode=='standalone' else 'main'))]+([] if mode=='standalone' else ['mv'])+args
 p=subprocess.run(cmd,cwd=root,capture_output=True,timeout=5); count+=1
 return p

kinds=['absent','file','dir','tree','fifo','socket','dangling','cycle','badlink','filelink','dirlink']
for mode,src,dst,force in itertools.product(['standalone','main'],kinds[1:],kinds,[False,True]):
 with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
  root=pathlib.Path(tmp); (root/'referent').write_bytes(b'unmodified'); (root/'container').mkdir()
  entry(root,'s',src); entry(root,'d',dst)
  target='d/s' if dst in ['dir','tree'] else 'container/s' if dst=='dirlink' else 'd'
  before=snapshot(root); expected=0
  # Independent policy: directory operands contain an absent basename; otherwise
  # reject existing target unless force, then enforce kernel entry type rules.
  exists=target in before
  if exists and not force: expected=1
  if exists and force and ((src in ['dir','tree']) != (dst in ['dir','tree'])): expected=1
  p=invoke(root,mode,(['-f'] if force else [])+['s','d'])
  assert p.returncode==expected,(mode,src,dst,force,p.returncode,p.stderr)
  after=snapshot(root)
  if expected: assert before==after,(mode,src,dst,force,'mutated failure')
  else:
   assert 's' not in after and after[target]==before['s'],(mode,src,dst,force,'identity')
   for path,meta in before.items():
    if path=='s' or path.startswith('s/') or path==target: continue
    assert path in after and after[path]==meta,(mode,src,dst,force,path)
   if src=='tree': assert after[target+'/child']==before['s/child']
# Effective existing target below real or symlink directory; same inode target,
# source directory over effective link, source-link over directories and cycles.
for mode,source,target,force,alias in itertools.product(['standalone','main'],['file','dir','dangling','cycle'],kinds,[False,True],[False,True]):
 with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
  root=pathlib.Path(tmp); (root/'referent').write_bytes(b'keep'); (root/'container').mkdir(); (root/'out').mkdir(); (root/'alias').symlink_to('out')
  entry(root,'s',source); entry(root,'out/s',target)
  # cycle link created by helper points to out/s relative to out: dangling is
  # still a link entry; the operation must not resolve its referent.
  before=snapshot(root); expected=0 if target=='absent' else 1
  if force and target!='absent':
   expected=1 if ((source=='dir') != (target in ['dir','tree'])) or target=='tree' else 0
  p=invoke(root,mode,(['-f'] if force else [])+['s','alias' if alias else 'out'])
  assert p.returncode==expected,(mode,source,target,force,alias,p.stderr)
  after=snapshot(root)
  if expected: assert before==after,('effective target failure mutation',source,target)
  else: assert 's' not in after and after['out/s']==before['s']
for mode,byte,jsonmode in itertools.product(['standalone','main'],range(1,256),[False,True]):
 if byte==47: continue
 with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
  root=pathlib.Path(tmp); name=b's'+bytes([byte]); fd=os.open(os.fsencode(root)+b'/'+name,os.O_WRONLY|os.O_CREAT,0o600); os.write(fd,b'keep'); os.close(fd)
  p=invoke(root,mode,(['--json'] if jsonmode else [])+['--',name,b'd'])
  valid=byte<128
  assert p.returncode==(2 if jsonmode and not valid else 0),(mode,byte,jsonmode,p.stderr)
  if jsonmode and valid: assert json.loads(p.stdout)==dict(source=name.decode(),destination='d',status='ok')
  if jsonmode and not valid: assert os.path.lexists(os.fsencode(root)+b'/'+name) and not (root/'d').exists()
  else: assert (root/'d').read_bytes()==b'keep'
print('Independent matrix + all single-byte path validation:',count,'invocations passed')
