#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
from pathlib import Path
import hashlib,struct,argparse
from elftools.elf.elffile import ELFFile
parser=argparse.ArgumentParser(description="Build a local experimental copy; never flashes a tablet.")
parser.add_argument("firmware",type=Path)
parser.add_argument("--build-dir",type=Path,required=True)
parser.add_argument("--output",type=Path,required=True)
args=parser.parse_args();p=args.build_dir
b=bytearray(args.firmware.read_bytes())
def require(condition):
 if not condition:raise SystemExit("Input/build does not match the pinned experiment; refusing to patch.")
require(hashlib.sha256(b).hexdigest()=='401e9c027b0e105efa9eb9b31683924a8d5f5ab3b1654f0915b174effe2df33d')
require(struct.unpack_from('<H',b,44)[0]==4)
require(b[0x10000+0x2df70:0x10000+0x2df74]==bytes.fromhex('4ff02643'))
require(struct.unpack_from('<2I',b,0x10000+12)==(0xb49,0xb49))
hook=(p/'hook.bin').read_bytes();require(len(hook)==4)
b[0x10000+0x2df70:0x10000+0x2df74]=hook
with (p/"proxy.elf").open("rb") as f:
 handler=ELFFile(f).get_section_by_name(".symtab").get_symbol_by_name("proxy_exception")[0]["st_value"]|1
require(0x50000 <= (handler & ~1) < 0x56000)
struct.pack_into("<2I",b,0x10000+12,handler,handler)
code=(p/'proxy.bin').read_bytes();require(0<len(code)<0x6000)
segment=code+bytes(0x7100-len(code))
off=(len(b)+4095)&~4095;b.extend(bytes(off-len(b)));b.extend(segment)
struct.pack_into('<8I',b,52+4*32,1,off,0x50000,0x50000,len(segment),len(segment),7,0x1000)
struct.pack_into('<H',b,44,5)
if args.output.resolve()==args.firmware.resolve():raise SystemExit("Output must differ from the original firmware.")
args.output.write_bytes(b)
print('Pinned firmware patch prepared:',len(b),'bytes; temporary use only.')
