#!/usr/bin/python3
"""Overlay optional video files into an already rendered LOCAL platform stage."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ap=argparse.ArgumentParser()
for name in ('platform','modules','firmware','bridge','sample'):ap.add_argument('--'+name,type=Path,required=True)
a=ap.parse_args();here=Path(__file__).resolve().parent;p=a.platform/'payload'
assert (p/'usr/share/trona-platform/files.json').is_file()
assert hashlib.sha256(a.firmware.read_bytes()).hexdigest()=='058d707c53ba0bc2c66ffd67b36f1a0fcbad3ab29e7489ad681af640f9d89616'
names=['mtk_scp_ipi','mtk_rpmsg','mtk_scp','trona_vdec_proxy','v4l2-h264','v4l2-vp9',
       'v4l2-mem2mem','mtk-vcodec-common','mtk-vcodec-dbgfs','mtk-vcodec-dec']
for name in names:
 f=a.modules/(name+'.ko')
 assert subprocess.check_output(['modinfo','-F','vermagic',str(f)],text=True).split()[0]=='6.18.56-trona-arch14'
assert 'trona_vdec_proxy' in subprocess.check_output(['modinfo','-F','depends',str(a.modules/'mtk_scp.ko')],text=True).strip().split(',')
b=a.bridge.read_bytes();assert b[:4]==b'\x7fELF' and b[18:20]==b'\xb7\0'
meta=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0',
 '-show_entries','stream=codec_name,width,height,nb_frames','-of','json',str(a.sample)],text=True))['streams'][0]
assert (meta['codec_name'],meta['width'],meta['height'],int(meta['nb_frames']))==('h264',640,480,60)
reference=subprocess.check_output(['ffmpeg','-nostdin','-v','error','-i',str(a.sample),
 '-vf','format=nv12','-an','-f','hash','-hash','sha256','-'],text=True,timeout=15).strip()
assert reference.startswith('SHA256=') and len(reference)==71
shutil.copytree(here/'payload',p,dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('__pycache__','*.pyc','._*','.DS_Store'))
for name in ['usr/local/libexec/trona-chrome-video','usr/local/libexec/trona-video-check','usr/local/sbin/trona-video-browser']:(p/name).chmod(0o755)
mods=p/'usr/lib/modules/6.18.56-trona-arch14/extra'
for name in names:shutil.copy2(a.modules/(name+'.ko'),mods/(name+'.ko'))
(mods.parent/'modules.order').write_text(''.join('extra/'+f.name+'\n' for f in sorted(mods.glob('*.ko'))))
for source,name in [(a.firmware,'usr/lib/firmware/mediatek/mt8183/scp-trona-h264.img'),
                    (a.bridge,'usr/lib/trona-video/v4l2stateless_drv_video.so'),
                    (a.sample,'usr/share/trona-video/selftest.mp4')]:
 dest=p/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
(p/'usr/share/trona-video/selftest.sha256').write_text(reference.split('=',1)[1]+'\n')
pkg=a.platform/'PKGBUILD';text=pkg.read_text();anchor="depends=('yay'"
assert text.count(anchor)==1
pkg.write_text(text.replace(anchor,"depends=('ffmpeg' 'libva' 'libdrm' 'yay'",1))
manifest={}
for f in sorted(p.rglob('*')):
 rel=str(f.relative_to(p))
 if rel=='usr/share/trona-platform/files.json':continue
 if f.is_symlink():manifest[rel]={'link':os.readlink(f)}
 elif f.is_file():manifest[rel]={'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
(p/'usr/share/trona-platform/files.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Optional video payload staged. No tablet, service or boot partition changed.')
