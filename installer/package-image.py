#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Create a private clean-install image; never reads the owner's running tablet."""
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()


def write_bundle(image, out, boot):
    assert not (out/"bundle.json").exists()
    device = json.loads((out/"device.json").read_text())
    size = image.stat().st_size
    raw_hash = hashlib.sha256()
    with image.open('rb') as source, (out/'rootfs.img.gz').open('wb') as target:
        with gzip.GzipFile(filename='', mode='wb', fileobj=target, mtime=0, compresslevel=3) as compressed:
            for chunk in iter(lambda: source.read(1024*1024), b''):
                raw_hash.update(chunk); compressed.write(chunk)
    binary = (boot/'boot.img').read_bytes()
    assert len(binary) < 33554432
    (out/'boot.padded.img').write_bytes(binary+bytes(33554432-len(binary)))

    names = ('rootfs.img.gz', 'boot.padded.img', 'recovery-tools.tar',
             'gpt-primary.new', 'gpt-secondary.new', 'device-profile.json')
    manifest = {'format': 1, 'device_serial': device['serial'],
                'root_image_bytes': size, 'root_image_sha256': raw_hash.hexdigest(),
                'files': {name: digest(out/name) for name in names},
                'hardware_tested': False}
    (out/'bundle.json').write_text(json.dumps(manifest, indent=2)+'\n')
    for name in (*names, 'bundle.json'):
        (out/name).chmod(0o600)


def main():
    root, out, boot = map(Path, sys.argv[1:])
    assert root == Path('/build/rootfs') and not (out/'bundle.json').exists()
    profile = json.loads((out/'device-profile.json').read_text())
    used = int(subprocess.check_output(['du', '-s', '--block-size=1', str(root)], text=True).split()[0])
    gib = 1024**3
    size = max(8*gib, ((used+2*gib+gib-1)//gib)*gib)
    assert size <= 16*gib, 'Root image unexpectedly large'
    image = Path('/build/rootfs.img')
    with image.open('wb') as f:
        f.truncate(size)
    subprocess.run(['mkfs.ext4', '-F', '-L', 'linux-root', '-U', profile['root_uuid'],
                    '-O', '^metadata_csum_seed,^orphan_file', '-m', '1', '-d', str(root), str(image)], check=True)
    result = subprocess.run(['e2fsck', '-fn', str(image)])
    assert result.returncode == 0, 'New filesystem failed consistency check'
    write_bundle(image, out, boot)
    image.unlink()
    print("Verified ext4 image and device-bound checksums written to", out)


if __name__ == "__main__":
    main()
