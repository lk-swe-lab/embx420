#!/usr/bin/env python3
"""Independent construction-based byte oracles, not a production parser."""
import argparse,itertools,os,random,subprocess
from pathlib import Path

def cases():
 r=random.Random(0x4205052);fixtures=[]
 for kind in 'diuoxX':
  vals=[0,1,7,8,15,16,255,(1<<63)-1]
  vals+=([-1,-42,-(1<<63)] if kind in 'di' else [(1<<63),(1<<64)-1])
  vals += [r.randrange(-(1<<63),1<<63) if kind in 'di' else r.randrange(1<<64) for _ in range(8)]
  for v in vals:
   for notation in ['decimal','octal','hex']:
    mag=abs(v); operand=(('-' if v<0 else '+')+str(mag)) if notation=='decimal' else ('-' if v<0 else '')+('0'+format(mag,'o') if notation=='octal' else '0x'+format(mag,'x'))
    digits=(str(mag) if kind in 'diu' else format(mag,kind));text=('-' if v<0 else '')+digits
    width=r.choice([1,2,9,21,65]);flag=r.choice(['','-','0','-0','0-']);pad=' '*max(0,width-len(text))
    if '-' in flag:result=text+pad
    elif '0' in flag:result=('-' if v<0 else '')+'0'*max(0,width-len(text))+digits
    else:result=pad+text
    fixtures.append((['['+'%'+flag+str(width)+kind+']',operand],('['+result+']').encode()))
 for _ in range(80):
  data=bytes(r.randrange(1,256) for _ in range(r.randrange(0,100)));precision=r.randrange(0,120);width=r.randrange(0,120);left=r.choice([True,False]);text=data[:precision];pad=b' '*max(0,width-len(text))
  fixtures.append(([('%'+('-' if left else '')+(str(width) if width else '')+'.'+str(precision)+'s').encode(),data],text+pad if left else pad+text))
 for suffix in ['8','9','x','G','%s']:
  fixtures.append(([r'\777'+suffix,'operand'] if suffix=='%s' else [r'\777'+suffix],b'\xff'+(b'operand' if suffix=='%s' else suffix.encode())))
 fixtures += [([r'\xFf0\1234\08\xAq'],b'\xff0S4\0' + b'8\nq'),(['%s:%c:%d;',b'\xff','abc','0x10',b'\xfe'],b'\xff:a:16;\xfe:\0:0;'),(['%s','%n%*s\\x41'],b'%n%*s\\x41')]
 return fixtures

def invalid_cases():
 invalid=[]
 for typ,low,high in [('d',-(1<<63),(1<<63)-1),('i',-(1<<63),(1<<63)-1),('u',0,(1<<64)-1),('o',0,(1<<64)-1),('x',0,(1<<64)-1),('X',0,(1<<64)-1)]:
  values=[str(low-1),str(high+1),'0x'+format(high+1,'x'),'0'+format(high+1,'o'),'0x','0X','08','+','-','\t1','1\n','1z','0b10','9'*1000,'-0' if low==0 else '-0x8000000000000001']
  invalid += [['already:%s%'+typ,'ok',x] for x in values]
 for fmt in ['%p','%n','%hn','%lln','%zu','%jX','%Lx','%1$d','%*d','%.*s','%+s','%#o','% 9d','%-00d','%0-0d','%--d','%4097d','%.4097s','%'+('9'*500)+'s','%','\\','\\x','\\u','\\8','%5%','%.d','%.c']:
  invalid.append(['already:'+fmt,'1','2'])
 invalid += [['%d%s','1','valid','bad'],['%4096s']+['']*4097]
 return invalid

def execute(program,argv):
 return subprocess.run(program+argv,capture_output=True,timeout=8,env={'PATH':'/usr/bin:/bin','LC_ALL':'C.UTF-8','HOME':'.','TERM':'ansi'})
def main():
 p=argparse.ArgumentParser();p.add_argument('root');p.add_argument('--reference',action='store_true');p.add_argument('--binary');p.add_argument('--smoke',action='store_true');p.add_argument('--preflight-only',action='store_true');a=p.parse_args();root=Path(a.root).resolve()
 programs=[['/usr/bin/printf']] if a.reference else [[a.binary]] if a.binary else [[str(root/'build/printf')],[str(root/'build/main'),'printf']]
 fixtures=cases();calls=0
 if a.smoke:fixtures=[(['%s','plain'],b'plain')]
 if a.preflight_only:fixtures=[]
 for program in programs:
  for argv,expected in fixtures:
   proc=execute(program,argv);calls+=1
   assert (proc.returncode,proc.stdout,proc.stderr)==(0,expected,b''),('PRINTF_REVIEW_BYTES',program,argv[:3],expected,proc.returncode,proc.stdout,proc.stderr)
  if not a.reference and not a.smoke:
   for argv in invalid_cases():
    proc=execute(program,argv);calls+=1
    assert (proc.returncode,proc.stdout)==(2,b''),('PRINTF_REVIEW_PREFLIGHT',program,argv[:3],proc.returncode,proc.stdout)
    assert proc.stderr.startswith(b'printf:'),proc.stderr
 print('Independent constructed oracle:',len(fixtures),'valid cases;',0 if a.reference or a.smoke else len(invalid_cases()),'invalid cases;',calls,'real invocations passed')
if __name__=='__main__':main()
