#!/usr/bin/env python3
"""Read Trona SCP remap and DEVAPC diagnostic status, without register writes.

Developer diagnostic only. Does not enable SCP, load firmware or grant access.
DBG words can be stale: FIFO selection/consumption is deliberately not performed.
Do not extend this to probe the secure DEVAPC AO permission registers: those
reads themselves cause access violations on the reference Fire boot chain.
"""

import json
import mmap
import os
from pathlib import Path
import struct


def read_words(fd, base, offsets):
    with mmap.mmap(fd, 4096, flags=mmap.MAP_SHARED,
                   prot=mmap.PROT_READ, offset=base) as registers:
        return {name: struct.unpack_from('<I', registers, offset)[0]
                for name, offset in offsets.items()}


def main():
    compatible = Path('/sys/firmware/devicetree/base/compatible').read_bytes().split(b'\0')
    if b'amazon,trona' not in compatible or b'mediatek,mt8183' not in compatible:
        raise SystemExit('Only amazon,trona / mediatek,mt8183 is supported.')
    fd = os.open('/dev/mem', os.O_RDONLY | os.O_SYNC)
    try:
        scp = read_words(fd, 0x105c0000, {
            'one_time_lock': 0xdc, 'secure_ctrl': 0xe0,
            'sys_ctrl': 0xe4, 'remap_cfg1': 0x120,
            'remap_cfg2': 0x124, 'remap_cfg3': 0x128,
        })
        devapc = read_words(fd, 0x10207000, {
            **{f'vio_status_{i}': 0x400 + 4 * i for i in range(12)},
            'dbg0_stale_possible': 0x900, 'dbg1_stale_possible': 0x904,
            'shift_status': 0xf10,
        })
    finally:
        os.close(fd)
    print(json.dumps({
        'scp': {name: f'0x{value:08x}' for name, value in scp.items()},
        'devapc': {name: f'0x{value:08x}' for name, value in devapc.items()},
        'vdecsys_global_con_violation_pending': bool(devapc['vio_status_7'] & (1 << 24)),
        'note': 'Pending status may predate this capture. DBG words are not a fresh FIFO read.',
    }, indent=2))


if __name__ == '__main__':
    main()
