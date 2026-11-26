#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Guided, device-bound clean installation. Never unlocks a bootloader."""
import argparse
import getpass
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import struct
import sys
import tempfile
import tarfile
import urllib.request
import uuid

from layout import (DISK_BYTES, LAYOUT, PREFIX_BYTES, ROOT_SECTORS, SECTOR,
                    plan, require, sha, validate)

REPO = Path(__file__).resolve().parents[1]
DISK = '/dev/block/mmcblk0'


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for data in iter(lambda: f.read(1024*1024), b''):
            h.update(data)
    return h.hexdigest()


def save(path, value):
    fd, name = tempfile.mkstemp(prefix='.'+path.name, dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as f:
            f.write(json.dumps(value, indent=2) + '\n'); f.flush(); os.fsync(f.fileno())
        os.replace(name, path)
        directory = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def checked(args, **kw):
    return subprocess.run(args, check=True, **kw)


class Device:
    def __init__(self, serial=None):
        require(shutil.which('adb'), 'Install Android SDK Platform Tools (adb) first.')
        lines = subprocess.check_output(['adb', 'devices'], text=True).splitlines()[1:]
        devices = [line.split() for line in lines if line.strip()]
        # Never silently choose one of several tablets, including unauthorized devices.
        require(len(devices) == 1, 'Connect exactly one Android device in TWRP.')
        self.serial, status = devices[0][:2]
        require(status in ('device', 'recovery'), 'ADB must be authorized in TWRP.')
        require(serial is None or self.serial == serial, 'A different device is connected.')

    def args(self, *args):
        return ['adb', '-s', self.serial, *args]

    def shell(self, command, timeout=60):
        return subprocess.check_output(self.args('shell', '-T', command), text=True,
                                       timeout=timeout).strip()

    def read(self, start, length, target):
        require(start % SECTOR == 0 and length % SECTOR == 0, 'Unaligned read')
        command = f'dd if={DISK} bs=512 skip={start//SECTOR} count={length//SECTOR} 2>/dev/null'
        with target.open('wb') as f:
            checked(self.args('exec-out', command), stdout=f, timeout=900)
        require(target.stat().st_size == length, 'Short ADB read: '+target.name)
        target.chmod(0o600)

    def inspect(self):
        require(self.shell('getprop ro.product.device') == 'trona', 'Not a Trona tablet.')
        recovery = self.shell('getprop ro.twrp.version')
        require(recovery == '3.7.0_9-0', 'This installer requires the tested TWRP 3.7.0_9-0.')
        require(self.shell('id -u') == '0', 'Recovery ADB is not root.')
        require(int(self.shell(f'blockdev --getsize64 {DISK}')) == DISK_BYTES,
                'Only the tested 32-GB eMMC is supported.')
        battery = int(self.shell('cat /sys/class/power_supply/battery/capacity'))
        require(50 <= battery <= 100, 'Charge the tablet to at least 50% first.')
        return {'serial': self.serial, 'model': 'trona', 'recovery': recovery,
                'disk_bytes': DISK_BYTES, 'battery_percent': battery}

    def push(self, source, target):
        checked(self.args('push', str(source), target), timeout=900)

    def hash(self, path):
        value = self.shell('sha256sum '+shlex.quote(path), timeout=300).split()[0]
        require(re.fullmatch('[a-f0-9]{64}', value), 'Invalid remote hash')
        return value


def doctor(_args):
    print('Host:', sys.platform, '| Python:', sys.version.split()[0])
    for name in ('adb', 'docker', 'git'):
        print(f'{name}:', shutil.which(name) or 'MISSING')
    print('Requires: unlocked Trona 32 GB, TWRP 3.7.0_9-0, >=50% battery, '
          'ARM64 Docker execution and >=25 GiB free build storage.')
    if shutil.which('adb'):
        dev = Device(); info = dev.inspect()
        print('Recovery identity and capacity accepted. Battery:', info['battery_percent'], '%')
        print('Partition geometry is checked by prepare; no blocks have been written.')


def extract_firmware(dev, dest):
    expected = json.loads((REPO/'firmware/required.json').read_text())
    dest.mkdir()
    mounts = ['/tmp/trona-installer-vendor', '/tmp/trona-installer-system']
    mounted = []
    try:
        for mount, partition in zip(mounts, (20, 19)):
            dev.shell(f'mkdir -p {mount}; mount -t ext4 -o ro,noload {DISK}p{partition} {mount}')
            mounted.append(mount)
        for name, checksum in expected.items():
            paths = dev.shell(f'find {" ".join(mounts)} -type f -name {shlex.quote(name)}').splitlines()
            paths = [p for p in paths if any(p.startswith(m+'/') for m in mounts) and dev.hash(p) == checksum]
            require(paths, 'Cannot find supported stock firmware: '+name)
            with (dest/name).open('wb') as f:
                checked(dev.args('exec-out', 'cat '+shlex.quote(paths[0])), stdout=f)
            require(digest(dest/name) == checksum, 'Unsupported firmware version: '+name)
    finally:
        for mount in reversed(mounted):
            dev.shell('umount '+mount)


def prepare(args):
    out = args.workspace
    require(not out.exists(), 'Use a new empty workspace. Existing preparations are never overwritten.')
    dev = Device(); info = dev.inspect()
    require(shutil.disk_usage(out.parent).free > 1024**3, 'Need 1 GiB free for preparation.')
    out.mkdir(mode=0o700)
    dev.read(0, 34*SECTOR, out/'gpt-primary.original')
    dev.read(DISK_BYTES-33*SECTOR, 33*SECTOR, out/'gpt-secondary.original')
    primary = (out/'gpt-primary.original').read_bytes()
    secondary = (out/'gpt-secondary.original').read_bytes()
    validate(primary, secondary)
    print('Stock 32-GB partition layout verified. Saving the protected 200-MiB area.')
    dev.read(0, PREFIX_BYTES, out/'protected-prefix.original')
    prefix = (out/'protected-prefix.original').read_bytes()
    require(prefix[:len(primary)] == primary, 'Partition table changed during capture.')
    info['protected_sha256'] = sha(prefix)
    info['partitions'] = {}
    for number, (name, start, size) in enumerate(LAYOUT[:17], 1):
        info['partitions'][str(number)] = sha(prefix[start*SECTOR:(start+size)*SECTOR])
    extract_firmware(dev, out/'firmware')
    root_uuid = str(uuid.uuid4())
    p, s = plan(primary, secondary, str(uuid.uuid4()))
    (out/'gpt-primary.new').write_bytes(p)
    (out/'gpt-secondary.new').write_bytes(s)
    save(out/'device-profile.json', {
        'root_uuid': root_uuid, 'root_bytes': ROOT_SECTORS*SECTOR,
        'boot_sha256': info['partitions']['16'],
        'recovery_sha256': info['partitions']['17'], 'sd_uuid': 'NO-SD-CARD'})
    save(out/'device.json', info)
    save(out/'preparation.json', {p.name: digest(p) for p in out.iterdir() if p.is_file()})
    print('Prepared. Android and SD card are unchanged. Private files are in', out)
    print('Next: ./install.sh build --workspace '+shlex.quote(str(out)))


def load_prepared(out):
    manifest = json.loads((out/'preparation.json').read_text())
    # Profile is intentionally updated with the new boot hash by the builder.
    for name, checksum in manifest.items():
        require(Path(name).name == name, 'Invalid preparation path')
        if name != 'device-profile.json':
            require(digest(out/name) == checksum, 'Preparation changed: '+name)
    primary = (out/'gpt-primary.original').read_bytes()
    secondary = (out/'gpt-secondary.original').read_bytes()
    validate(primary, secondary)
    return json.loads((out/'device.json').read_text())


def fetch(url, checksum, dest):
    if dest.exists():
        require(digest(dest) == checksum, 'Cached source checksum mismatch: '+str(dest))
        return
    temp = dest.with_suffix('.partial')
    print('Downloading', url, flush=True)
    with urllib.request.urlopen(url, timeout=90) as response, temp.open('wb') as f:
        shutil.copyfileobj(response, f, 1024*1024)
    require(digest(temp) == checksum,
            'Source snapshot changed. No tablet writes occurred. See docs/installer.md.')
    temp.replace(dest)


def copy_build_inputs(volume, out):
    """Stream into a Linux volume; avoid writable macOS shared-folder image I/O."""
    cmd = ['docker', 'run', '--rm', '-i', '--platform', 'linux/arm64', '-v', volume+':/build',
           'trona-installer:0.2', 'bash', '-ec',
           'mkdir -p /build/source /build/output; tar -xpf - -C /build']
    process = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    try:
        with tarfile.open(fileobj=process.stdin, mode='w|') as archive:
            def source_filter(info):
                parts = Path(info.name).parts
                if '__pycache__' in parts or info.name.endswith(('.pyc', '/.DS_Store')):
                    return None
                return info
            for path in sorted(REPO.iterdir()):
                if path.name not in ('.git', 'local', 'build', 'dist', '.DS_Store'):
                    archive.add(path, arcname='source/'+path.name, filter=source_filter)
            names = ['arch.tar.gz', 'keyring.pkg.tar.zst', 'device.json', 'device-profile.json',
                     'gpt-primary.new', 'gpt-secondary.new', 'firmware']
            if (out/'maintenance.pub').exists():
                names.append('maintenance.pub')
            for name in names:
                archive.add(out/name, arcname='output/'+name)
    finally:
        process.stdin.close()
        code = process.wait()
    require(code == 0, 'Docker input transfer failed; tablet unchanged.')


def export_bundle(container, out):
    # Export the manifest LAST. An interrupted copy is never a complete bundle.
    for name in ('rootfs.img.gz', 'boot.padded.img', 'recovery-tools.tar', 'device-profile.json',
                 'packages.txt', 'platform-check.txt', 'bundle.json'):
        temp = out/('.'+name+'.'+uuid.uuid4().hex+'.partial')
        checked(['docker', 'cp', container+':/build/output/'+name, str(temp)])
        temp.chmod(0o600); temp.replace(out/name)


def build(args):
    out = args.workspace; load_prepared(out)
    require(not (out/'bundle.json').exists(), 'Bundle already built; do not overwrite it.')
    require(shutil.which('docker'), 'Install/start Docker first.')
    require(shutil.disk_usage(out).free >= 25*1024**3,
            'Need at least 25 GiB free on the host (also in the Docker disk).')
    sources = json.loads((REPO/'installer/sources.json').read_text())
    fetch(sources['arch_url'], sources['arch_sha256'], out/'arch.tar.gz')
    fetch(sources['omarchy_keyring_url'], sources['omarchy_keyring_sha256'], out/'keyring.pkg.tar.zst')
    password = getpass.getpass('New tablet password (never printed/saved as plaintext): ')
    require(len(password) >= 8 and '\n' not in password and '\r' not in password,
            'Use at least 8 characters, without line breaks.')
    require(password == getpass.getpass('Repeat password: '), 'Passwords differ.')
    require(re.fullmatch('[a-z]{2}(?:,[a-z]{2})?', args.keyboard), 'Use a layout such as de or us.')
    if args.ssh_key:
        key = args.ssh_key.read_text().strip()
        require(re.fullmatch(r'(?:ssh-ed25519|ssh-rsa|ecdsa-sha2-nistp256) [A-Za-z0-9+/=]+(?: [^\n]+)?', key),
                'Expected a single SSH PUBLIC key.')
        # Strip the identifying comment. Private keys are never accepted.
        (out/'maintenance.pub').write_text(' '.join(key.split()[:2])+'\n')
    checked(['docker', 'build', '--platform', 'linux/arm64', '-t', 'trona-installer:0.2',
             '-f', str(REPO/'installer/Dockerfile'), str(REPO)])
    volume = 'trona-installer-'+uuid.uuid4().hex[:12]
    container = volume+'-build'
    state = {'docker_volume': volume, 'docker_container': container, 'status': 'building'}
    save(out/'build-state.json', state)
    checked(['docker', 'volume', 'create', volume], stdout=subprocess.DEVNULL)
    print('Building in an isolated Docker volume. The tablet is not accessed.', flush=True)
    # No host device mounts. Privilege is required only for chroot pseudo-filesystems.
    try:
        copy_build_inputs(volume, out)
        checked(['docker', 'create', '--name', container, '-i', '--platform', 'linux/arm64', '--privileged',
                 '-e', 'TRONA_HOST_UID='+str(os.getuid()), '-e', 'TRONA_HOST_GID='+str(os.getgid()),
                 '-v', volume+':/build', '-w', '/build', 'trona-installer:0.2',
                 'bash', '-ec', 'rmdir /src; ln -s /build/source /src; ln -s /build/output /output; '
                 'exec bash /src/installer/build.sh "$1"', 'trona-build', args.keyboard],
                stdout=subprocess.DEVNULL)
        checked(['docker', 'start', '-ai', container], input=(password+'\n').encode())
        code = subprocess.check_output(['docker', 'inspect', '-f', '{{.State.ExitCode}}', container], text=True).strip()
        require(code == '0', 'Build container failed; inspect it with docker logs '+container)
        export_bundle(container, out)
        checked(['docker', 'rm', container], stdout=subprocess.DEVNULL)
    except BaseException:
        state['status'] = 'failed'; save(out/'build-state.json', state)
        print('Build stopped; tablet unchanged. Diagnostic volume:', volume, file=sys.stderr)
        print('Inspect: docker logs '+container, file=sys.stderr)
        print('When no longer needed: docker rm -f '+container+'; docker volume rm '+volume, file=sys.stderr)
        raise
    finally:
        password = None
    state['status'] = 'complete'; save(out/'build-state.json', state)
    print('Build complete. No tablet writes. Next: ./install.sh install --workspace '+shlex.quote(str(out)))
    print('Optional build-volume cleanup: docker volume rm '+volume)


def verify_bundle(out):
    info = load_prepared(out)
    bundle = json.loads((out/'bundle.json').read_text())
    require(bundle['format'] == 1 and bundle['device_serial'] == info['serial'], 'Wrong bundle/device')
    expected = {'rootfs.img.gz', 'boot.padded.img', 'recovery-tools.tar',
                'gpt-primary.new', 'gpt-secondary.new', 'device-profile.json'}
    require(set(bundle['files']) == expected, 'Incomplete bundle')
    for name, checksum in bundle['files'].items():
        require(digest(out/name) == checksum, 'Bundle checksum mismatch: '+name)
    require((out/'boot.padded.img').stat().st_size == 33554432, 'Wrong boot size')
    primary = (out/'gpt-primary.original').read_bytes()
    secondary = (out/'gpt-secondary.original').read_bytes()
    new = (out/'gpt-primary.new').read_bytes()
    require(len(new) == len(primary), 'Invalid planned GPT size')
    part_uuid = str(uuid.UUID(bytes_le=new[1024+17*128+16:1024+17*128+32]))
    expected_primary, expected_secondary = plan(primary, secondary, part_uuid)
    require(new == expected_primary and (out/'gpt-secondary.new').read_bytes() == expected_secondary,
            'GPT plan does not preserve the protected partitions')
    profile = json.loads((out/'device-profile.json').read_text())
    require(profile['boot_sha256'] == bundle['files']['boot.padded.img'] and
            profile['recovery_sha256'] == info['partitions']['17'] and
            profile['root_bytes'] == ROOT_SECTORS*SECTOR, 'Profile and bundle disagree')
    require(4*1024**3 <= bundle['root_image_bytes'] < ROOT_SECTORS*SECTOR,
            'Root image exceeds partition')
    require(bundle['root_image_bytes'] % (1024*1024) == 0, 'Root image must be MiB aligned')
    # Verify decompression before ANY tablet write, not during the flash alone.
    h = hashlib.sha256(); count = 0; superblock = b''
    with gzip.open(out/'rootfs.img.gz', 'rb') as f:
        for chunk in iter(lambda: f.read(1024*1024), b''):
            if not superblock:
                superblock = chunk[1024:2048]
            count += len(chunk); h.update(chunk)
            require(count <= bundle['root_image_bytes'], 'Oversize decompressed image')
    require(count == bundle['root_image_bytes'] and h.hexdigest() == bundle['root_image_sha256'],
            'Root image integrity failed')
    require(len(superblock) == 1024 and superblock[56:58] == b'\x53\xef' and
            uuid.UUID(bytes=superblock[104:120]) == uuid.UUID(profile['root_uuid']),
            'Root filesystem UUID/type does not match the boot profile')
    blocks = struct.unpack_from('<I', superblock, 4)[0]
    block_shift = struct.unpack_from('<I', superblock, 24)[0]
    require(block_shift <= 6 and blocks*(1024 << block_shift) == count,
            'Root filesystem size does not match the image')
    return info, bundle


def check_current(out, info, bundle, resume):
    prefix = (out/'protected-prefix.current').read_bytes()
    tail = (out/'gpt-secondary.current').read_bytes()
    if not resume:
        require(sha(prefix) == info['protected_sha256'], 'Protected area changed. Refusing installation.')
        require(tail == (out/'gpt-secondary.original').read_bytes(), 'Backup GPT changed.')
        return
    require((out/'install-state.json').exists(), 'Resume needs the original interrupted workspace.')
    state = json.loads((out/'install-state.json').read_text())
    require(state['stage'] in ('writing-gpt', 'writing-root', 'writing-boot'),
            'Only an interrupted installation may be resumed; completed installations are not erased.')
    original = bytearray((out/'protected-prefix.original').read_bytes())
    require(prefix[:34*SECTOR] in ((out/'gpt-primary.original').read_bytes(), (out/'gpt-primary.new').read_bytes()),
            'Unknown primary GPT. Manual recovery required.')
    require(tail in ((out/'gpt-secondary.original').read_bytes(), (out/'gpt-secondary.new').read_bytes()),
            'Unknown backup GPT. Manual recovery required.')
    _, start, size = LAYOUT[15]
    boot = prefix[start*SECTOR:(start+size)*SECTOR]
    require(sha(boot) in (info['partitions']['16'], bundle['files']['boot.padded.img']),
            'Unknown/partially written boot. Manual recovery required.')
    original[:34*SECTOR] = prefix[:34*SECTOR]
    original[start*SECTOR:(start+size)*SECTOR] = boot
    require(bytes(original) == prefix, 'Protected firmware/calibration/recovery changed.')


def install(args):
    out = args.workspace
    print('Verifying complete local image (this can take several minutes).', flush=True)
    info, bundle = verify_bundle(out)
    dev = Device(info['serial']); dev.inspect()
    require(dev.shell('cat /proc/swaps').count('\n') == 0, 'Active swap in recovery')
    print('\nEXPERIMENTAL INSTALLER — not yet tested end-to-end on a second tablet.')
    print('This permanently erases Android, apps and internal user data (partitions 18–21).')
    if args.resume:
        print('RESUME restarts the root write from zero. Any partly installed Linux data is erased again.')
    else:
        require(not (out/'install-state.json').exists(), 'A prior write exists. Inspect it before using --resume.')
    print('The SD card and partitions 1–17 are preserved, except the new Linux boot in p16.')
    print('The saved 200-MiB area is NOT an Android/user-data backup.')
    print('Device:', dev.serial, '| Root:', round(ROOT_SECTORS*SECTOR/1024**3, 2), 'GiB')
    require(input('Type ERASE '+dev.serial+' to continue: ').strip() == 'ERASE '+dev.serial,
            'Cancelled; no storage writes.')
    # Re-read protected data after confirmation to catch another device/layout or drift.
    dev.inspect()
    dev.read(0, PREFIX_BYTES, out/'protected-prefix.current')
    dev.read(DISK_BYTES-33*SECTOR, 33*SECTOR, out/'gpt-secondary.current')
    check_current(out, info, bundle, args.resume)
    # Stage small trusted tools and boot in RAM, not Android userdata.
    dev.shell('mkdir -p /tmp/trona-install')
    for name in ('recovery-tools.tar', 'boot.padded.img', 'gpt-primary.new', 'gpt-secondary.new'):
        dev.push(out/name, '/tmp/trona-install/'+name)
        require(dev.hash('/tmp/trona-install/'+name) == digest(out/name), 'RAM transfer failed')
    dev.shell('cd /tmp/trona-install && tar xf recovery-tools.tar')
    loader = '/tmp/trona-install/tools/lib/ld-linux-aarch64.so.1 --library-path /tmp/trona-install/tools/lib '
    tool = loader+'/tmp/trona-install/tools/bin/'
    dev.shell(tool+'blockdev --getsize64 '+DISK)
    # Freeze the recovery UI so it cannot remount old Android paths during migration.
    pids = dev.shell('pidof recovery').split()
    require(pids and all(p.isdecimal() for p in pids), 'Cannot identify recovery process')
    for pid in pids:
        require(dev.shell('cat /proc/'+pid+'/comm') == 'recovery', 'Unexpected recovery process')
    mutated = False
    try:
        for pid in pids:
            dev.shell('kill -STOP '+pid)
        # Refuse active loops/device-mapper; never blindly format a mounted filesystem.
        mounts = dev.shell('cat /proc/mounts').splitlines()
        require(not any(line.split()[0].startswith(('/dev/block/loop', '/dev/loop', '/dev/block/dm-', '/dev/dm-'))
                        for line in mounts), 'Unmount loop/device-mapper filesystems first.')
        for line in reversed(mounts):
            source, mountpoint = line.split()[:2]
            if source.startswith('/dev/'):
                source = dev.shell('readlink -f '+shlex.quote(source))
            if source.startswith(('/dev/block/mmcblk0p', '/dev/mmcblk0p')) or mountpoint in ('/data', '/cache', '/system', '/vendor', '/sdcard'):
                require('\\' not in mountpoint, 'Unsupported escaped mountpoint')
                dev.shell('umount '+shlex.quote(mountpoint))
        require(not any('mmcblk0p' in line for line in dev.shell('cat /proc/mounts').splitlines()),
                'Internal filesystem remains mounted')
        require(dev.shell('ls /sys/class/block/mmcblk0p*/holders/* 2>/dev/null || true') == '',
                'A device-mapper/other holder still uses internal storage')
        # Persist the stage BEFORE the first disk write; there is no blind retry/resume.
        save(out/'install-state.json', {'stage': 'writing-gpt', 'automatic_resume': False})
        mutated = True
        # TWRP's standalone dd is built without conv=notrunc.
        dev.shell(f'/sbin/toybox dd if=/tmp/trona-install/gpt-secondary.new of={DISK} bs=512 seek={DISK_BYTES//512-33} conv=notrunc && sync')
        dev.shell(f'/sbin/toybox dd if=/tmp/trona-install/gpt-primary.new of={DISK} bs=512 conv=notrunc && sync')
        dev.shell(tool+'blockdev --rereadpt '+DISK)
        require(int(dev.shell(tool+'blockdev --getsize64 '+DISK+'p18')) == ROOT_SECTORS*SECTOR,
                'Kernel partition table did not update. Stop; do not reboot Android.')
        for name, offset, length in [('gpt-primary', 0, 34*SECTOR),
                                     ('gpt-secondary', DISK_BYTES-33*SECTOR, 33*SECTOR)]:
            dev.read(offset, length, out/(name+'.readback'))
            require((out/(name+'.readback')).read_bytes() == (out/(name+'.new')).read_bytes(),
                    'GPT readback failed')
        save(out/'install-state.json', {'stage': 'writing-root', 'automatic_resume': False})
        print('Writing Linux root. Keep the cable connected.', flush=True)
        with gzip.open(out/'rootfs.img.gz', 'rb') as source:
            proc = subprocess.Popen(dev.args('shell', '-T', f'dd of={DISK}p18 bs=1048576 && sync'),
                                    stdin=subprocess.PIPE)
            try:
                shutil.copyfileobj(source, proc.stdin, 1024*1024)
                proc.stdin.close()
            finally:
                if proc.stdin and not proc.stdin.closed:
                    proc.stdin.close()
                rc = proc.wait()
            require(rc == 0, 'Root transfer failed')
        result = dev.shell(f'dd if={DISK}p18 bs=1048576 count={bundle["root_image_bytes"]//1048576} 2>/dev/null | sha256sum', timeout=1800).split()[0]
        require(result == bundle['root_image_sha256'], 'Root readback mismatch; boot not written')
        dev.shell(tool+'e2fsck -pf '+DISK+'p18; rc=$?; [ "$rc" -le 1 ]', timeout=600)
        dev.shell(tool+'resize2fs '+DISK+'p18', timeout=600)
        dev.shell(tool+'e2fsck -pf '+DISK+'p18; rc=$?; [ "$rc" -le 1 ]', timeout=600)
        for number in range(1, 18):
            accepted = {info['partitions'][str(number)]}
            if args.resume and number == 16:
                accepted.add(bundle['files']['boot.padded.img'])
            require(dev.hash(DISK+'p'+str(number)) in accepted,
                    'Protected partition changed; boot not written')
        save(out/'install-state.json', {'stage': 'writing-boot', 'automatic_resume': False})
        dev.shell(f'dd if=/tmp/trona-install/boot.padded.img of={DISK}p16 bs=1048576 && sync')
        require(dev.hash(DISK+'p16') == bundle['files']['boot.padded.img'], 'Boot readback failed')
        require(dev.hash(DISK+'p17') == info['partitions']['17'], 'Recovery readback failed')
        save(out/'install-state.json', {'stage': 'written-and-verified', 'boot_tested': False})
        print('Storage and boot readbacks verified. Recovery is unchanged.')
        print('First login: trona and the password chosen during build. Run the acceptance checks.')
        if input('Type BOOT to start Linux now, or press Enter to stay in recovery: ').strip() == 'BOOT':
            checked(dev.args('reboot'))
    except BaseException:
        if mutated:
            print('STOP: installation interrupted after storage writes. Do not boot Android or rerun blindly.\n'
                  'Keep this workspace and use docs/installer.md#interrupted-installation.', file=sys.stderr)
        raise
    finally:
        # On successful reboot ADB disappears; do not mask the result with cleanup errors.
        if not mutated:
            for pid in pids:
                try:
                    dev.shell('kill -CONT '+pid, timeout=5)
                except (subprocess.SubprocessError, OSError):
                    pass


def main():
    os.umask(0o077)
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('command', nargs='?', choices=('doctor', 'prepare', 'build', 'verify', 'install', 'wizard'))
    ap.add_argument('--workspace', type=Path, default=REPO/'local/installer')
    ap.add_argument('--keyboard', default='us', help='XKB keyboard layout, e.g. de or us')
    ap.add_argument('--ssh-key', type=Path, help='Optional personal SSH PUBLIC key for cable maintenance')
    ap.add_argument('--resume', action='store_true', help='Restart an interrupted install from the SAME workspace only')
    args = ap.parse_args(); args.workspace = args.workspace.expanduser().resolve()
    require(not args.workspace.is_relative_to(REPO) or args.workspace.is_relative_to(REPO/'local'),
            'Private workspaces inside the repository must be under ignored local/.')
    if args.command is None:
        print('Trona installer — experimental, Fire HD 10 (2021) 32 GB only.\n'
              '1. Check requirements (read-only)\n2. Prepare from stock TWRP (read-only)\n'
              '3. Build private Linux system (host only)\n4. Verify built bundle (host only)\n'
              '5. Install (erases Android after explicit confirmation)\n'
              '6. Guided clean install: check, prepare, build, then confirmed installation')
        args.command = {'1':'doctor', '2':'prepare', '3':'build', '4':'verify', '5':'install', '6':'wizard'}.get(input('Step [1]: ') or '1')
        require(args.command is not None, 'Unknown step')
    if args.command in ('prepare', 'wizard'):
        args.workspace.parent.mkdir(parents=True, exist_ok=True)
    if args.command == 'wizard':
        require(not args.resume, 'Use install --resume for an interrupted write.')
        doctor(args); prepare(args); build(args); install(args)
        return
    {'doctor': doctor, 'prepare': prepare, 'build': build,
     'verify': lambda a: (verify_bundle(a.workspace), print('Bundle checks passed; no device access.')),
     'install': install}[args.command](args)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError, EOFError, KeyboardInterrupt) as e:
        print('Stopped:', str(e) or 'cancelled', file=sys.stderr)
        sys.exit(1)
