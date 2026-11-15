#!/usr/bin/env python3
"""Fast source checks; does not execute any tablet/privileged script."""
from pathlib import Path
import ast,json,re,subprocess,sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'scripts'))
from device_profile import render
profile={'ROOT_UUID':'11111111-2222-4333-8444-555555555555','ROOT_BYTES':'31058804224',
 'BOOT_SHA256':'1'*64,'RECOVERY_SHA256':'2'*64,'SD_UUID':'NO-SD-CARD'}
count=0
for p in root.rglob('*'):
 if any(x in p.relative_to(root).parts for x in ['.git','local','build','dist','__pycache__']):continue
 if p.is_symlink():
  if not p.resolve().is_relative_to(root) or not p.resolve().exists():raise SystemExit('Unsafe/dangling symlink: '+str(p))
  continue
 if not p.is_file():continue
 data=p.read_bytes()
 if b'\0' in data:raise SystemExit('Unexpected binary: '+str(p))
 text=data.decode('utf-8')
 if p.name.startswith('._') or p.name.endswith('.mod.c') or p.suffix in ['.ko','.img','.bin','.pyc']:raise SystemExit('Build/private artifact: '+str(p))
 if re.search(r'-----BEGIN (?:OPENSSH|RSA|EC) PRIVATE KEY-----',text):raise SystemExit('Private key marker: '+str(p))
 if p.suffix=='.json':json.loads(text)
 if p.suffix=='.py' or re.match(r'#![^\n]*python',text):
  ast.parse(render(text,profile) if 'platform' in p.relative_to(root).parts else text,filename=str(p))
 if re.match(r'#![^\n]*(?:bash|/sh)\b',text) or p.name=='PKGBUILD':
  subprocess.run(['bash','-n'],input=render(text,profile),text=True,check=True)
 if p.suffix=='.md':
  for link in re.findall(r'\]\(([^\s)]+)\)',text):
   if '://' in link or link.startswith('#'):continue
   if not (p.parent/link.split('#')[0]).exists():raise SystemExit('Broken local link: '+str(p)+': '+link)
 count+=1
required=json.loads((root/'kernel/modules.json').read_text())
if len(required)!=len(set(required)):raise SystemExit('Duplicate module names')
assert (root/'platform/payload/usr/share/alsa/ucm2/conf.d/Trona/Trona.conf').is_symlink()
assert '@ROOT_UUID@' in (root/'boot/init.template').read_text()
assert 'setsid sh' not in (root/'boot/init.template').read_text()
assert 'trona_suspend_path' not in (root/'kernel/linux-6.18.56-trona.patch').read_text()
print(f'PASS: {count} source files, syntax, JSON, links, symlinks and artifact exclusions')
