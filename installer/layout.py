# SPDX-License-Identifier: GPL-2.0-only
"""Strict, pure GPT planning. No device I/O; used by installer and file tests."""
import binascii
import hashlib
import struct
import uuid

SECTOR = 512
DISK_BYTES = 31268536320
ROOT_START = 409600
ROOT_SECTORS = 60661727
PREFIX_BYTES = ROOT_START * SECTOR
ENTRY_BYTES = 128 * 128
LAYOUT = [
    ('kb', 64, 2048), ('dkb', 2112, 2048), ('keys', 4160, 16384),
    ('misc', 20544, 2048), ('lk', 22592, 2048), ('tee1', 24640, 10240),
    ('tee2', 34880, 10240), ('metadata', 45120, 65536),
    ('boot_para', 110656, 2048), ('nvcfg', 112704, 16384),
    ('spmfw', 129088, 2048), ('sspm_1', 131136, 2048),
    ('cam_vpu1', 133184, 30720), ('cam_vpu2', 163904, 65536),
    ('cam_vpu3', 229440, 30720), ('boot', 260160, 65536),
    ('recovery', 325696, 83904), ('cache', 409600, 1114112),
    ('system', 1523712, 7438336), ('vendor', 8962048, 819200),
    ('userdata', 9781248, 51290079),
]
LINUX_TYPE = uuid.UUID('0fc63daf-8483-4772-8e79-3d69d8477de4').bytes_le
DATA_TYPE = uuid.UUID('ebd0a0a2-b9e5-4433-87c0-68b6b72699c7').bytes_le


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def header(data, current, alternate, entries_lba):
    require(len(data) == SECTOR and data[:8] == b'EFI PART', 'Not a GPT header')
    revision, size, checksum, reserved = struct.unpack_from('<IIII', data, 8)
    require((revision, size, reserved) == (0x10000, 92, 0), 'Unsupported GPT header')
    check = bytearray(data[:size]); check[16:20] = bytes(4)
    require(binascii.crc32(check) == checksum, 'GPT header CRC mismatch')
    cur, alt, first, last = struct.unpack_from('<QQQQ', data, 24)
    table, count, width, crc = struct.unpack_from('<QIII', data, 72)
    require((cur, alt, first, last, table, count, width) ==
            (current, alternate, 64, 61071326, entries_lba, 128, 128),
            'Unsupported GPT geometry')
    return data[56:72], crc


def validate(primary, secondary, disk_bytes=DISK_BYTES):
    """Accept only the exact stock 32-GB geometry; GUIDs remain per-device."""
    require(disk_bytes == DISK_BYTES, 'Only the tested 32-GB eMMC is supported')
    require(len(primary) == 34 * SECTOR and len(secondary) == 33 * SECTOR,
            'Incomplete GPT capture')
    require(primary[510:512] == b'\x55\xaa', 'Missing protective MBR')
    entries_mbr = [primary[i:i+16] for i in range(446, 510, 16)]
    require(entries_mbr[0][4] == 0xee and all(e == bytes(16) for e in entries_mbr[1:]),
            'Hybrid or unknown MBR is not supported')
    last = disk_bytes // SECTOR - 1
    guid, crc = header(primary[SECTOR:2*SECTOR], 1, last, 2)
    guid2, crc2 = header(secondary[-SECTOR:], last, 1, last-32)
    entries = primary[2*SECTOR:]
    require(entries == secondary[:-SECTOR] and guid == guid2 and crc == crc2,
            'Primary/backup GPT disagree')
    require(binascii.crc32(entries) == crc, 'GPT entries CRC mismatch')
    for i, (name, start, length) in enumerate(LAYOUT):
        entry = entries[i*128:(i+1)*128]
        a, b, attributes = struct.unpack_from('<QQQ', entry, 32)
        actual_name = entry[56:128].decode('utf-16-le').rstrip('\0')
        require((actual_name, a, b-a+1, attributes) == (name, start, length, 0),
                f'Unsupported partition {i+1}: {actual_name}')
        require(entry[:16] == DATA_TYPE and entry[16:32] != bytes(16),
                f'Unexpected type/GUID on partition {i+1}')
    require(entries[len(LAYOUT)*128:] == bytes(ENTRY_BYTES-len(LAYOUT)*128),
            'Unexpected extra GPT partitions')
    return entries


def patched_header(original, entries):
    result = bytearray(original)
    struct.pack_into('<I', result, 88, binascii.crc32(entries))
    result[16:20] = bytes(4)
    struct.pack_into('<I', result, 16, binascii.crc32(result[:92]))
    return bytes(result)


def plan(primary, secondary, partition_uuid):
    old = validate(primary, secondary)
    entries = bytearray(old[:17*128] + bytes(ENTRY_BYTES-17*128))
    entry = (LINUX_TYPE + uuid.UUID(partition_uuid).bytes_le +
             struct.pack('<QQQ', ROOT_START, ROOT_START+ROOT_SECTORS-1, 0) +
             'linux-root'.encode('utf-16-le').ljust(72, b'\0'))
    entries[17*128:18*128] = entry
    new_primary = primary[:SECTOR] + patched_header(primary[SECTOR:2*SECTOR], entries) + entries
    new_secondary = entries + patched_header(secondary[-SECTOR:], entries)
    return bytes(new_primary), bytes(new_secondary)
