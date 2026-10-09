#!/usr/bin/env python3
"""Reviewer-owned contract oracle; no production parser or GNU-derived bytes."""
import argparse, itertools, subprocess
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('root')
parser.add_argument('--reference', default='/usr/bin/echo')
args=parser.parse_args()
root=Path(args.root).resolve()
env={'PATH':'/usr/bin:/bin','LC_ALL':'C'}
reference_count=0
fixtures=[([],b'\n'),([b'',b''],b' \n'),
          ([b' a ',b'',b'b '],b' a   b \n'),
          ([b'\\n',b'\\x41'],b'\\n \\x41\n'),
          ([b'a\nb',b'c\rd',b'\t'],b'a\nb c\rd \t\n'),
          ([b'\xff\xfe'],b'\xff\xfe\n'),
          ([b'x'*65000],b'x'*65000+b'\n'),
          ([b'q']*2048,b'q '*2047+b'q\n')]
fixtures.extend(([b'fixture',bytes([v]),b''],b'fixture '+bytes([v])+b' \n')
                for v in range(1,256))
for operands,expected in fixtures:
    got=subprocess.run([str(Path(args.reference).resolve())]+operands,
                       env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                       timeout=5)
    assert (got.returncode,got.stdout,got.stderr)==(0,expected,b''), (
        'GNU_REFERENCE_BYTES',operands,got.returncode,got.stdout,got.stderr)
    reference_count+=1
HELP=(b'Usage: echo [-h] [TEXT]...\n\n'
      b'  -h, --help           Show help and exit.\n'
      b'  TEXT                 Literal text operands (no escapes).\n'
      b'\nPrint literal text operands separated by spaces, then a newline.\n'
      b'Use -- before option-like text. No -n, escapes or JSON.\n'
      b'Exit status: 0 success, 1 operational failure, 2 argument error.\n')
def oracle(args):
    operands=[]; help_count=0; end=False; invalid=False
    for token in args:
        if end:operands.append(token)
        elif token==b'--':end=True
        elif token in (b'-h',b'--help'):help_count+=1
        elif token.startswith(b'-') and token!=b'-':invalid=True
        else:operands.append(token)
    if invalid or help_count>1:return 2,b''
    if help_count:return 0,HELP
    return 0,b' '.join(operands)+b'\n'
count=0
for prefix in [[str(root/'build/echo')],[str(root/'build/main'),'echo']]:
    for operands,expected in fixtures:
        got=subprocess.run(prefix+operands,env=env,stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE,timeout=5)
        assert (got.returncode,got.stdout,got.stderr)==(0,expected,b''), (
            'COMMON_PROJECT_BYTES',operands,got.returncode,got.stdout)
        count+=1
TOKENS=[b'',b'a b',b'-',b'-h',b'--help',b'--',b'--bogus',b'-n']
for posix in [False,True]:
    use_env={**env,**({'POSIXLY_CORRECT':'1'} if posix else {})}
    for prefix in [[str(root/'build/echo')],[str(root/'build/main'),'echo']]:
        for size in range(4):
            for args in itertools.product(TOKENS,repeat=size):
                want=oracle(args)
                got=subprocess.run(prefix+list(args),env=use_env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
                assert (got.returncode,got.stdout)==want,(posix,args,want,got.returncode,got.stdout)
                assert bool(got.stderr)==(want[0]!=0),(args,got.stderr)
                count+=1
        for byte in range(1,256):
            args=[b'--',bytes([byte]),b'',b'\xff\n\t\r\\']
            got=subprocess.run(prefix+args,env=use_env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
            assert (got.returncode,got.stdout,got.stderr)==(0,bytes([byte])+b'  \xff\n\t\r\\\n',b'')
            count+=1
# Exact syntax corner cases follow the declared Argtable abbreviation convention.
for token in [b'--h',b'--he',b'--hel']:
    got=subprocess.run([str(root/'build/echo'),token],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
    assert (got.returncode,got.stdout,got.stderr)==(0,HELP,b'');count+=1
for token in [b'--help=',b'--help=yes',b'-hh',b'-hfoo',b'---',b'-\xff']:
    got=subprocess.run([str(root/'build/echo'),token],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
    assert got.returncode==2 and not got.stdout and got.stderr;count+=1
print('independent echo oracle:',count,'project checks and',
      reference_count,'GNU-compatible independent fixtures passed')
