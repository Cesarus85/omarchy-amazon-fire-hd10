#!/usr/bin/env python3
"""Trona-only WMT firmware service using the shipped driver's ioctl ABI.
No Android daemon dependency. Firmware is read from this tablet's vendor backup.
"""
import fcntl
import os
from pathlib import Path
import select
import socket
import struct
import sys

FW = Path('/usr/lib/firmware')
PATCHES = ['ROMv4_be_patch_1_0_hdr.bin', 'ROMv4_be_patch_1_1_hdr.bin']

def ioctl(fd, direction, magic, number, size, argument):
    return fcntl.ioctl(fd, (direction << 30) | (size << 16) | (magic << 8) | number, argument)

def initialize():
    assert b'amazon,trona\0' in Path('/sys/firmware/devicetree/base/compatible').read_bytes()
    boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
    marker = Path('/run/trona-wmt-platform.ready')
    if marker.exists() and marker.read_text().strip() == boot and Path('/dev/stpwmt').exists():
        print('WMT platform already initialized in this boot', flush=True)
        return
    if Path('/dev/stpwmt').exists():
        raise RuntimeError('Untracked WMT state: reboot before initializing again')
    with open('/dev/wmtdetect', 'r+b', buffering=0) as detect:
        chip = ioctl(detect, 2, ord('w'), 3, 4, 0)
        if chip != 0x6771: raise RuntimeError(f'Unexpected vendor platform ID: {chip:#x}')
        ioctl(detect, 1, ord('w'), 1, 4, chip)
        result = ioctl(detect, 2, ord('w'), 4, 4, 0)
        if result: raise RuntimeError(f'WMT initialization failed: {result}')
    marker.write_text(boot + '\n')
    print('WMT platform initialized', flush=True)

def supply_patches(fd):
    info = [ioctl(fd, 2, 0xa0, 12, 4, i) for i in range(3)]
    print('Chip/hardware/firmware:', [hex(x) for x in info], flush=True)
    records = []
    for name in PATCHES:
        data = (FW / name).read_bytes()
        if len(data) < 28 or data[16:20] != b'ALPS':
            raise ValueError(f'Invalid firmware header: {name}')
        fw_version = int.from_bytes(data[22:24], 'big')
        if (fw_version ^ info[2]) & 0xff:
            raise ValueError(f'Firmware revision mismatch: {name}')
        count, sequence = data[24] >> 4, data[24] & 15
        if count != 2 or sequence not in (1, 2):
            raise ValueError(f'Unexpected patch sequence: {name}')
        address = b'\0' + data[25:28]
        records.append((sequence, address, name))
    if {record[0] for record in records} != {1, 2}:
        raise ValueError('Incomplete patch set')
    ioctl(fd, 1, 0xa0, 14, 4, len(records))
    for sequence, address, name in sorted(records):
        record = struct.pack('<I4s256s', sequence, address, name.encode())
        ioctl(fd, 1, 0xa0, 15, 8, record)
        print(f'Patch {sequence}: {name}, address bytes {address.hex()}', flush=True)

if __name__ == '__main__':
    if '--init' in sys.argv:
        initialize()
        sys.exit(0)
    Path('/run/trona-wmt-ready').unlink(missing_ok=True)
    fd = os.open('/dev/stpwmt', os.O_RDWR | os.O_CLOEXEC)
    ioctl(fd, 1, 0xa0, 5, 4, 0x23)  # BTIF transport, FM common interface
    poll = select.poll()
    poll.register(fd, select.POLLIN | select.POLLERR | select.POLLHUP)
    Path('/run/trona-wmt-ready').write_text(str(os.getpid()) + '\n')
    endpoint = os.environ.get('NOTIFY_SOCKET')
    if endpoint:
        with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as notifier:
            notifier.connect(('\0' + endpoint[1:]) if endpoint.startswith('@') else endpoint)
            notifier.sendall(b'READY=1\nSTATUS=BTIF firmware transport ready')
    print('WMT BTIF firmware service ready', flush=True)
    while True:
        for _, events in poll.poll():
            if events & (select.POLLERR | select.POLLHUP):
                raise OSError('WMT device disconnected')
            command = os.read(fd, 255).rstrip(b'\0').decode('ascii')
            print('WMT request:', repr(command), flush=True)
            try:
                if command != 'srh_patch':
                    raise ValueError(f'Unsupported command: {command}')
                supply_patches(fd)
            except Exception as error:
                print('WMT request failed:', error, flush=True)
                os.write(fd, b'fail')
            else:
                os.write(fd, b'ok')
