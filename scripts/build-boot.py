#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build a Trona boot image from explicit local inputs. No flashing."""
import argparse,gzip,hashlib,json,lzma,stat,struct,subprocess,sys
from pathlib import Path
from device_profile import load_profile,render
ap=argparse.ArgumentParser()
for name in ('profile','kernel','dtb','busybox','rootfs','output'):ap.add_argument('--'+name,required=True,type=Path)
a=ap.parse_args();p=load_profile(a.profile);repo=Path(__file__).resolve().parents[1]
if a.output.exists():raise SystemExit('Output directory already exists')
kernel=a.kernel.read_bytes();raw=gzip.decompress(kernel);dtb=a.dtb.read_bytes()
if raw[56:60]!=b'ARM\x64' or struct.unpack_from('<Q',raw,8)[0]!=0:raise SystemExit('Wrong ARM64 load-offset/header')
if len(raw)>=0x55000000-0x40200000:raise SystemExit('Kernel overlaps ramdisk')
if dtb[:4]!=b'\xd0\x0d\xfe\xed' or b'amazon,trona\0' not in dtb:raise SystemExit('Not a Trona DTB')
def arm_elf(data):
 if data[:4]!=b'\x7fELF' or data[18:20]!=b'\xb7\x00':raise ValueError('Expected AArch64 ELF')
busy=a.busybox.read_bytes();arm_elf(busy)
# A static busybox must not contain a PT_INTERP program header.
phoff=struct.unpack_from('<Q',busy,32)[0];entsize,phnum=struct.unpack_from('<HH',busy,54)
if any(struct.unpack_from('<I',busy,phoff+i*entsize)[0]==3 for i in range(phnum)):
 raise SystemExit('BusyBox must be statically linked')
entries={}
def add(name,data=b'',mode=stat.S_IFREG|0o755,major=0,minor=0):entries[name]=(data,mode,major,minor)
for d in ['bin','dev','proc','sys','run','tmp','root','etc','fsck','fsck/bin','fsck/lib']:add(d,mode=stat.S_IFDIR|0o755)
add('bin/busybox',busy)
add('init',render((repo/'boot/init.template').read_text(),p).encode())
add('shutdown',(repo/'boot/shutdown').read_bytes())
add('dev/console',mode=stat.S_IFCHR|0o600,major=5,minor=1)
add('dev/null',mode=stat.S_IFCHR|0o666,major=1,minor=3)
for name in ('blkid','e2fsck'):
 data=(a.rootfs/'usr/bin'/name).read_bytes();arm_elf(data);add('fsck/bin/'+name,data)
for lib in ['libext2fs.so.2','libcom_err.so.2','libblkid.so.1','libuuid.so.1','libe2p.so.2','libc.so.6','ld-linux-aarch64.so.1']:
 data=(a.rootfs/'usr/lib'/lib).read_bytes();arm_elf(data);add('fsck/lib/'+lib,data)
# newc is deterministic, root-owned and has no host metadata.
add('TRAILER!!!',mode=0);cpio=bytearray()
for ino,(name,(data,mode,major,minor)) in enumerate(entries.items(),1):
 nb=name.encode()+b'\0'
 fields=[ino,mode,0,0,1,0,len(data),0,0,major,minor,len(nb),0]
 cpio+=b'070701'+''.join(f'{v:08x}' for v in fields).encode()+nb
 cpio+=bytes(-len(cpio)%4);cpio+=data;cpio+=bytes(-len(cpio)%4)
a.output.mkdir(parents=True)
ram=lzma.compress(cpio,format=lzma.FORMAT_XZ,preset=6,check=lzma.CHECK_CRC32)
(a.output/'ramdisk.cpio.xz').write_bytes(ram);(a.output/'Image.gz-dtb').write_bytes(kernel+dtb)
cmd='bootopt=64S3,32N2,64N2 rdinit=/init console=ttyS0,921600 earlycon consoleblank=0 clk_ignore_unused regulator_ignore_unused cpuidle.off=1 maxcpus=8 loglevel=7 panic=5 sysctl.kernel.panic_on_rcu_stall=1'
subprocess.run([sys.executable,str(repo/'tools/mkbootimg.py'),'--kernel',str(a.output/'Image.gz-dtb'),'--ramdisk',str(a.output/'ramdisk.cpio.xz'),'--base','0','--kernel_offset','0x40200000','--ramdisk_offset','0x55000000','--second_offset','0','--tags_offset','0x54000000','--pagesize','2048','--header_version','1','--cmdline',cmd,'--output',str(a.output/'boot.img')],check=True)
image=(a.output/'boot.img').read_bytes()
if len(image)>=33554432:raise SystemExit('Boot image exceeds the tested partition')
sha=lambda b:hashlib.sha256(b).hexdigest()
manifest={'image_sha256':sha(image),'padded_boot_sha256':sha(image+bytes(33554432-len(image))),
 'kernel_sha256':sha(kernel),'dtb_sha256':sha(dtb),'ramdisk_sha256':sha(ram),'image_size':len(image),
 'root_uuid':p['ROOT_UUID'],'root_bytes':int(p['ROOT_BYTES']),'booted':False}
(a.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Built',a.output/'boot.img','— not flashed or boot-tested.')
