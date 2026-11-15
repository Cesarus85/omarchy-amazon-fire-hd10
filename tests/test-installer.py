#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Destructive-install policy tests use generated files/mocks, never USB devices."""
import binascii
import importlib.util
import io
import gzip
import json
import shutil
import subprocess
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import uuid

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO/'installer'))
import layout
spec = importlib.util.spec_from_file_location('installer', REPO/'installer/trona-installer.py')
installer = importlib.util.module_from_spec(spec); spec.loader.exec_module(installer)


def fixture():
    entries = bytearray(layout.ENTRY_BYTES)
    for i, (name, start, length) in enumerate(layout.LAYOUT):
        entry = layout.DATA_TYPE + uuid.uuid4().bytes_le
        entry += struct.pack('<QQQ', start, start+length-1, 0)
        entry += name.encode('utf-16-le').ljust(72, b'\0')
        entries[i*128:(i+1)*128] = entry
    last = layout.DISK_BYTES//512-1
    guid = uuid.uuid4().bytes_le

    def header(current, other, table):
        result = bytearray(512)
        struct.pack_into('<8sIIIIQQQQ16sQIII', result, 0, b'EFI PART', 0x10000, 92, 0, 0,
                         current, other, 64, 61071326, guid, table, 128, 128, 0)
        return layout.patched_header(result, entries)
    mbr = bytearray(512); mbr[510:] = b'\x55\xaa'; mbr[450] = 0xee
    struct.pack_into('<II', mbr, 454, 1, last)
    return bytes(mbr)+header(1, last, 2)+entries, bytes(entries)+header(last, 1, last-32)


def rechecksum(primary, secondary, mutate):
    entries = bytearray(primary[1024:]); mutate(entries)
    return (primary[:512]+layout.patched_header(primary[512:1024], entries)+entries,
            bytes(entries)+layout.patched_header(secondary[-512:], entries))


class GPTTests(unittest.TestCase):
    def setUp(self):
        self.primary, self.secondary = fixture()

    def test_stock_and_preservation(self):
        entries = layout.validate(self.primary, self.secondary)
        first, last = layout.plan(self.primary, self.secondary, str(uuid.uuid4()))
        self.assertEqual(first[1024:1024+17*128], entries[:17*128])
        self.assertEqual(first[:512], self.primary[:512])
        self.assertEqual(first[568:584], self.primary[568:584])
        self.assertEqual(first[1024:], last[:-512])
        header = layout.header(first[512:1024], 1, layout.DISK_BYTES//512-1, 2)
        self.assertEqual(header[1], binascii.crc32(first[1024:]))
        entry = first[1024+17*128:1024+18*128]
        start, end = struct.unpack_from('<QQ', entry, 32)
        self.assertEqual((start, end-start+1), (layout.ROOT_START, layout.ROOT_SECTORS))
        self.assertEqual(first[1024+18*128:], bytes((128-18)*128))
        # Native Linux is deliberately not treated as a fresh stock installation.
        with self.assertRaises(ValueError): layout.validate(first, last)

    def test_single_byte_corruption(self):
        for position in (512, 520, 528, 536, 568, 584, 600, 1024, 1800, len(self.primary)-1):
            with self.subTest(position=position):
                damaged = bytearray(self.primary); damaged[position] ^= 1
                with self.assertRaises(ValueError): layout.validate(damaged, self.secondary)

    def test_valid_crc_does_not_authorize_unknown_layout(self):
        for index in range(21):
            with self.subTest(partition=index+1):
                def mutate(entries):
                    offset = index*128+32
                    struct.pack_into('<Q', entries, offset, struct.unpack_from('<Q', entries, offset)[0]+1)
                p, s = rechecksum(self.primary, self.secondary, mutate)
                with self.assertRaises(ValueError): layout.validate(p, s)

    def test_extra_partition(self):
        p, s = rechecksum(self.primary, self.secondary, lambda e: e.__setitem__(slice(21*128,22*128), e[:128]))
        with self.assertRaises(ValueError): layout.validate(p, s)

    def test_capacity_and_short_reads(self):
        for size in (0, 64000000000, layout.DISK_BYTES+512):
            with self.assertRaises(ValueError): layout.validate(self.primary, self.secondary, size)
        with self.assertRaises(ValueError): layout.validate(self.primary[:-1], self.secondary)
        with self.assertRaises(ValueError): layout.validate(self.primary, self.secondary[:-1])

    def test_hybrid_mbr(self):
        p = bytearray(self.primary); p[466] = 0x83
        with self.assertRaises(ValueError): layout.validate(p, self.secondary)

    @unittest.skipUnless(shutil.which('sfdisk'), 'Independent GPT check needs Linux sfdisk')
    def test_sparse_disk_with_independent_parser(self):
        p, s = layout.plan(self.primary, self.secondary, str(uuid.uuid4()))
        with tempfile.TemporaryDirectory() as t:
            path = Path(t)/'generated-disk'
            with path.open('wb') as f:
                f.write(p); f.seek(layout.DISK_BYTES-len(s)); f.write(s)
            subprocess.run(['sfdisk', '--verify', str(path)], check=True, capture_output=True)
            result = json.loads(subprocess.check_output(['sfdisk', '--json', str(path)], text=True))
            entries = result['partitiontable']['partitions']
            self.assertEqual(len(entries), 18)
            self.assertEqual(entries[-1]['start'], layout.ROOT_START)
            self.assertEqual(entries[-1]['size'], layout.ROOT_SECTORS)
            self.assertEqual(entries[-1]['name'], 'linux-root')


class DeviceTests(unittest.TestCase):
    def test_never_auto_select_multiple_or_unauthorized(self):
        for listing in ('List of devices attached\n',
                        'List of devices attached\na\tdevice\nb\tdevice\n',
                        'List of devices attached\na\tunauthorized\n'):
            with patch('shutil.which', return_value='/test/adb'), \
                 patch('subprocess.check_output', return_value=listing):
                with self.assertRaises(ValueError): installer.Device()

    def test_serial_binding(self):
        with patch('shutil.which', return_value='/test/adb'), \
             patch('subprocess.check_output', return_value='List of devices attached\nnew\trecovery\n'):
            with self.assertRaises(ValueError): installer.Device('old')
            self.assertEqual(installer.Device('new').args('devices'), ['adb','-s','new','devices'])

    def test_no_backup_overwrite(self):
        with tempfile.TemporaryDirectory() as t, patch.object(installer, 'Device') as device:
            args = type('Args', (), {'workspace': Path(t)})()
            with self.assertRaises(ValueError): installer.prepare(args)
            device.assert_not_called()

    def test_battery_wrong_model_and_recovery(self):
        for wrong in ('model', 'recovery', 'capacity', 'battery'):
            device = object.__new__(installer.Device); device.serial = 'test'
            answers = {'getprop ro.product.device': 'trona', 'getprop ro.twrp.version': '3.7.0_9-0',
                       'id -u': '0', 'blockdev --getsize64 /dev/block/mmcblk0': str(layout.DISK_BYTES),
                       'cat /sys/class/power_supply/battery/capacity': '80'}
            key, value = {'model':('getprop ro.product.device','suez'),
                          'recovery':('getprop ro.twrp.version',''),
                          'capacity':('blockdev --getsize64 /dev/block/mmcblk0','64000000000'),
                          'battery':('cat /sys/class/power_supply/battery/capacity','49')}[wrong]
            answers[key] = value
            with patch.object(device, 'shell', side_effect=lambda cmd: answers[cmd]):
                with self.assertRaises(ValueError): device.inspect()


class WritePolicyTests(unittest.TestCase):
    def test_failed_bundle_never_opens_adb(self):
        with tempfile.TemporaryDirectory() as t, patch.object(installer, 'Device') as device:
            args = type('Args', (), {'workspace': Path(t)})()
            with self.assertRaises(FileNotFoundError): installer.install(args)
            device.assert_not_called()

    def test_cancel_never_writes_or_reads_block_data(self):
        with tempfile.TemporaryDirectory() as t, \
             patch.object(installer, 'verify_bundle', return_value=({'serial':'test'}, {})), \
             patch.object(installer, 'Device') as device, patch('builtins.input', return_value='no'):
            device.return_value.serial = 'test'
            device.return_value.shell.return_value = 'Filename Type Size Used Priority'
            args = type('Args', (), {'workspace': Path(t), 'resume': False})()
            with self.assertRaises(ValueError): installer.install(args)
            device.return_value.push.assert_not_called()
            device.return_value.read.assert_not_called()
            self.assertEqual(device.return_value.shell.call_args_list[0].args, ('cat /proc/swaps',))

    def test_resume_refuses_completed_installation(self):
        with tempfile.TemporaryDirectory() as t:
            out = Path(t)
            for name in ('protected-prefix.current', 'gpt-secondary.current'):
                (out/name).write_bytes(b'')
            (out/'install-state.json').write_text(json.dumps({'stage':'written-and-verified'}))
            with self.assertRaisesRegex(ValueError, 'completed installations'):
                installer.check_current(out, {}, {}, True)

    def simulate_install(self, fault=None):
        """Exercise orchestration and failures without executing a shell or opening USB."""
        commands = []
        with tempfile.TemporaryDirectory() as t:
            out = Path(t)
            data = bytes(1024*1024)
            with gzip.open(out/'rootfs.img.gz', 'wb') as f: f.write(data)
            for name in ('boot.padded.img', 'recovery-tools.tar', 'gpt-primary.new',
                         'gpt-secondary.new', 'gpt-secondary.original'):
                (out/name).write_bytes(name.encode())
            info = {'serial':'fixture', 'protected_sha256':layout.sha(b'prefix'),
                    'partitions':{str(n):layout.sha(str(n).encode()) for n in range(1,18)}}
            bundle = {'root_image_bytes':len(data), 'root_image_sha256':layout.sha(data),
                      'files':{'boot.padded.img':installer.digest(out/'boot.padded.img')}}

            class FakeDevice:
                serial = 'fixture'
                boot_written = False
                def inspect(self): return {}
                def args(self, *args): return ['FAKE-NEVER-EXECUTE', *args]
                def push(self, *_): pass
                def read(self, start, length, target):
                    if target.name == 'protected-prefix.current': value = b'prefix'
                    elif target.name == 'gpt-secondary.current': value = (out/'gpt-secondary.original').read_bytes()
                    else: value = (out/target.name.replace('.readback','.new')).read_bytes()
                    target.write_bytes(value)
                def hash(self, path):
                    if path.startswith('/tmp/'):
                        return installer.digest(out/Path(path).name)
                    number = path.split('p')[-1]
                    if number == '16' and self.boot_written: return bundle['files']['boot.padded.img']
                    if fault == 'protected' and number == '5': return '0'*64
                    return info['partitions'][number]
                def shell(self, cmd, **_):
                    commands.append(cmd)
                    if cmd == 'cat /proc/swaps': return 'Filename Type Size Used Priority'
                    if cmd == 'cat /proc/mounts': return 'tmpfs /tmp tmpfs rw 0 0'
                    if cmd == 'pidof recovery': return '123'
                    if cmd == 'cat /proc/123/comm': return 'recovery'
                    if '--getsize64 /dev/block/mmcblk0p18' in cmd: return str(layout.ROOT_SECTORS*512)
                    if 'count=1 ' in cmd and '| sha256sum' in cmd:
                        return ('0'*64 if fault == 'root' else bundle['root_image_sha256'])+'  -'
                    if 'of=/dev/block/mmcblk0p16' in cmd: self.boot_written = True
                    if fault == 'disconnect' and 'gpt-secondary.new of=' in cmd:
                        raise subprocess.CalledProcessError(1, 'FAKE disconnect')
                    return ''

            class FakeProcess:
                def __init__(self, args, **_):
                    commands.append(args[-1]); self.stdin = io.BytesIO()
                def wait(self): return 0

            args = type('Args', (), {'workspace':out, 'resume':False})()
            with patch.object(installer, 'verify_bundle', return_value=(info,bundle)), \
                 patch.object(installer, 'Device', return_value=FakeDevice()), \
                 patch('subprocess.Popen', FakeProcess), \
                 patch('builtins.input', side_effect=['ERASE fixture', '']), \
                 patch('sys.stdout', new=io.StringIO()), patch('sys.stderr', new=io.StringIO()):
                if fault:
                    with self.assertRaises((ValueError, subprocess.CalledProcessError)):
                        installer.install(args)
                else:
                    installer.install(args)
                    self.assertEqual(json.loads((out/'install-state.json').read_text())['stage'], 'written-and-verified')
        return commands

    def test_root_or_protected_mismatch_never_flashes_boot(self):
        for fault in ('root', 'protected', 'disconnect'):
            with self.subTest(fault=fault):
                commands = self.simulate_install(fault)
                self.assertFalse(any('of=/dev/block/mmcblk0p16' in c for c in commands))
                self.assertFalse(any('kill -CONT' in c for c in commands))

    def test_success_writes_boot_last(self):
        commands = self.simulate_install()
        writes = [c for c in commands if 'of=/dev/block/' in c]
        self.assertEqual(len(writes), 4)
        self.assertIn('gpt-secondary.new', writes[0])
        self.assertIn('gpt-primary.new', writes[1])
        self.assertIn('of=/dev/block/mmcblk0p18', writes[2])
        self.assertIn('of=/dev/block/mmcblk0p16', writes[3])


if __name__ == '__main__':
    unittest.main()
