#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Prepare a local package directory. Never installs, enables services, or flashes."""
from pathlib import Path
import argparse,hashlib,json,os,shutil
from device_profile import load_profile,render
ap=argparse.ArgumentParser()
ap.add_argument('--profile',required=True);ap.add_argument('--artifacts',type=Path,required=True)
ap.add_argument('--firmware-dir',type=Path,required=True);ap.add_argument('--keyboard',type=Path,required=True)
ap.add_argument('--output',type=Path,required=True)
args=ap.parse_args();profile=load_profile(args.profile)
repo=Path(__file__).resolve().parents[1];out=args.output.resolve()
if out.exists():raise SystemExit('Refusing to overwrite an existing output')
fwmeta=json.loads((repo/'firmware/required.json').read_text())
for name,expected in fwmeta.items():
 f=args.firmware_dir/name
 if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=expected:
  raise SystemExit('Missing or different stock firmware: '+name)
for f in [args.keyboard,*list((args.artifacts/'modules').glob('*.ko'))]:
 b=f.read_bytes()
 if b[:4]!=b'\x7fELF' or b[18:20]!=b'\xb7\x00':raise SystemExit('Expected AArch64 ELF: '+str(f))
required=json.loads((repo/'kernel/modules.json').read_text())
if any(not (args.artifacts/'modules'/n).is_file() for n in required):raise SystemExit('Required kernel module missing')
shutil.copytree(repo/'platform',out,symlinks=True,ignore=shutil.ignore_patterns('__pycache__','*.pyc','._*','.DS_Store'))
p=out/'payload'
for f in p.rglob('*'):
 if f.is_file() and not f.is_symlink():
  f.chmod(f.stat().st_mode|0o200);f.write_text(render(f.read_text(),profile))
firm=p/'usr/lib/firmware';firm.mkdir(parents=True)
for name in fwmeta:shutil.copy2(args.firmware_dir/name,firm/name)
mods=p/'usr/lib/modules/6.18.56-trona-arch14';(mods/'extra').mkdir(parents=True)
for name in required:shutil.copy2(args.artifacts/'modules'/name,mods/'extra'/name)
for name in ['modules.builtin','modules.builtin.modinfo']:
 shutil.copy2(args.artifacts/name,mods/name)
(mods/'modules.order').write_text(''.join('extra/'+n+'\n' for n in required))
shutil.copy2(args.keyboard,p/'usr/local/bin/wvkbd-mobintl');(p/'usr/local/bin/wvkbd-mobintl').chmod(0o755)
(p/'etc/sudoers.d/00-trona-admin').chmod(0o440);(p/'etc/sudoers.d').chmod(0o750)
manifest={}
for f in sorted(p.rglob('*')):
 rel=str(f.relative_to(p))
 if f.is_symlink():manifest[rel]={'link':os.readlink(f)}
 elif f.is_file():manifest[rel]={'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
(p/'usr/share/trona-platform/files.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Prepared',out,'— review, then run makepkg on AArch64 Arch Linux.')
