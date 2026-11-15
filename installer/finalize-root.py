#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Finish a fresh offline root; deliberately never weakens the live update guard."""
import json
import os
from pathlib import Path
import subprocess
import sys

root, out = map(Path, sys.argv[1:])
assert root == Path('/build/rootfs') and (root/'tmp/trona-package').is_dir()
profile = json.loads((out/'device-profile.json').read_text())


def put(name, text, mode=0o644):
    path = root/name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text); path.chmod(mode)


payload = root/'tmp/trona-package/payload'
for source in payload.rglob('*'):
    os.lchown(root/source.relative_to(payload), 0, 0)
put('etc/fstab', 'UUID='+profile['root_uuid']+' / ext4 noatime,errors=remount-ro 0 1\n')
(root/'etc/resolv.conf').unlink(missing_ok=True)
(root/'etc/resolv.conf').symlink_to('/run/NetworkManager/resolv.conf')
# Block desktop startup if first-boot integrity or checked integration fails.
put('etc/systemd/system/trona-desktop.service.d/installer-guard.conf',
    '[Unit]\nRequires=trona-boot-guard.service\nAfter=trona-boot-guard.service\n')
put('var/lib/systemd/linger/trona', '')
for unit in ('trona-auto-rotate', 'trona-keyboard', 'trona-fit-windows', 'trona-sd-mount'):
    directory = root/'home/trona/.config/systemd/user/graphical-session.target.wants'
    directory.mkdir(parents=True, exist_ok=True)
    link = directory/(unit+'.service')
    target = '/usr/lib/systemd/user/'+unit+'.service'
    if link.is_symlink():
        assert os.readlink(link) == target, 'Unexpected user service override'
    else:
        link.symlink_to(target)
subprocess.run(['chroot', str(root), 'chown', '-R', 'trona:trona', '/home/trona'], check=True)
# No Wi-Fi credentials, Bluetooth bonds, host identity or private SSH key is copied.
assert not list((root/'etc/NetworkManager/system-connections').glob('*'))
assert not list((root/'var/lib/bluetooth').glob('*'))
# The default distribution passwords must both have been replaced/locked.
shadow = {line.split(':')[0]: line.split(':')[1] for line in (root/'etc/shadow').read_text().splitlines()}
assert shadow['root'].startswith(('!', '*')) and shadow['trona'].startswith(('$y$', '$6$'))
assert 'alarm' not in shadow
put('etc/trona-installation', 'Clean source-built native-p18 install; experimental installer 0.2\n')
