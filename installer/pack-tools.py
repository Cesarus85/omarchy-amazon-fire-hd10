#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Pack the ARM64 filesystem tools and their libraries for recovery RAM."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile

root, output = map(Path, sys.argv[1:])
stage = Path('/build/recovery-tools')
(stage/'tools/bin').mkdir(parents=True)
(stage/'tools/lib').mkdir()
for name in ('blockdev', 'e2fsck', 'resize2fs'):
    binary = '/usr/bin/'+name
    shutil.copyfile(root/binary.lstrip('/'), stage/'tools/bin'/name)
    (stage/'tools/bin'/name).chmod(0o755)
    text = subprocess.check_output(['chroot', str(root), 'ldd', binary], text=True)
    assert 'not found' not in text
    for lib in re.findall(r'=>\s+(/[^\s]+)', text):
        shutil.copyfile(root/lib.lstrip('/'), stage/'tools/lib'/Path(lib).name)
shutil.copyfile(root/'usr/lib/ld-linux-aarch64.so.1', stage/'tools/lib/ld-linux-aarch64.so.1')
(stage/'tools/lib/ld-linux-aarch64.so.1').chmod(0o755)
for file in stage.rglob('*'):
    if file.is_file():
        data = file.read_bytes()
        assert data[:4] == b'\x7fELF' and data[18:20] == b'\xb7\x00', str(file)
with tarfile.open(output, 'w', format=tarfile.USTAR_FORMAT) as tar:
    for path in sorted(stage.rglob('*')):
        info = tar.gettarinfo(str(path), str(path.relative_to(stage)))
        info.uid = info.gid = 0; info.uname = info.gname = ''; info.mtime = 0
        if path.is_file():
            with path.open('rb') as f:
                tar.addfile(info, f)
        else:
            tar.addfile(info)
os.chmod(output, 0o600)
