#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run the compiled fault handler in ARM emulation; no hardware access."""
from pathlib import Path
import struct
import sys
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR

build = Path(sys.argv[1])
with (build / 'proxy.elf').open('rb') as stream:
    entry = ELFFile(stream).get_section_by_name('.symtab').get_symbol_by_name(
        'proxy_fault')[0]['st_value'] | 1


def run(ins, *, address=0xA6020000, cfsr=0x82, ready=1, xpsr=0x01000000, host_error=0):
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    cpu.mem_map(0, 0x80000)
    cpu.mem_map(0xE000E000, 0x2000)
    cpu.mem_write(0x50000, (build / 'proxy.bin').read_bytes())
    cpu.mem_write(0x1000, ins + b'\0' * 4)
    frame = [0x11, 0x22, 0x33, 0xA6020000, 0x44, 0x55, 0x1000, xpsr]
    high = list(range(0x60, 0x68))
    cpu.mem_write(0x18000, struct.pack('<8I', *frame))
    cpu.mem_write(0x18100, struct.pack('<8I', *high))
    mailbox = [0] * 15
    mailbox[0], mailbox[14] = 0x50525859, ready
    cpu.mem_write(0x57000, struct.pack('<15I', *mailbox))
    cpu.mem_write(0xE000ED28, struct.pack('<I', cfsr))
    cpu.mem_write(0xE000ED34, struct.pack('<I', address))
    requests = []

    def service(uc, access, addr, size, value, user):
        request = struct.unpack('<4I', bytes(uc.mem_read(0x5700C, 16)))
        requests.append(request)
        if not request[2]:
            uc.mem_write(0x57010, struct.pack('<I', 0x12345678))
        uc.mem_write(0x5701C, struct.pack('<I', host_error))
        uc.mem_write(0x57008, struct.pack('<I', value))

    cpu.hook_add(UC_HOOK_MEM_WRITE, service, begin=0x57004, end=0x57007)
    for register, value in [(UC_ARM_REG_R0, 0x18000), (UC_ARM_REG_R1, 0x18100),
                            (UC_ARM_REG_SP, 0x40000), (UC_ARM_REG_LR, 0x20001)]:
        cpu.reg_write(register, value)
    cpu.emu_start(entry, 0x20000, count=10000)
    out = list(struct.unpack('<8I', bytes(cpu.mem_read(0x18000, 32))))
    high_out = list(struct.unpack('<8I', bytes(cpu.mem_read(0x18100, 32))))
    return cpu.reg_read(UC_ARM_REG_R0), out, high_out, requests, frame, high


def reg(frame, high, number):
    return frame[number] if number < 4 else high[number - 4] if number < 12 else frame[4]


cases = [
    ('str16', struct.pack('<H', 0x601A), True, 2, False),
    ('ldr16', struct.pack('<H', 0x68AC), False, 4, False),
    ('strreg16', struct.pack('<H', 0x509A), True, 2, False),
    ('ldr32', struct.pack('<HH', 0xF8D3, 0xC004), False, 12, False),
    ('str32', struct.pack('<HH', 0xF8C3, 0x4004), True, 4, False),
    ('ldrpost32', struct.pack('<HH', 0xF853, 0x4B04), False, 4, True),
]
for name, instruction, write, target, writeback in cases:
    result, out, high_out, requests, original, high = run(instruction)
    assert result == 1 and out[6] == 0x1000 + len(instruction), name
    assert len(requests) == 1, name
    address, value, is_write, width = requests[0]
    assert (address, is_write, width) == (0x16020000, write, 4), name
    if write:
        assert value == reg(original, high, target), name
    else:
        assert reg(out, high_out, target) == 0x12345678, name
    if writeback:
        assert out[3] == original[3] + 4, name
    print('PASS', name)

word = struct.pack('<H', 0x601A)
rejects = [
    ('outside_allowlist', word, {'address': 0xA6010000}),
    ('permission_controller', word, {'address': 0xA001C30C}),
    ('unaligned', word, {'address': 0xA6020001}),
    ('not_initialized', word, {'ready': 0}),
    ('not_data_access_fault', word, {'cfsr': 0x80}),
    ('byte_store', struct.pack('<H', 0x701A), {}),
    ('halfword_store', struct.pack('<H', 0x801A), {}),
    ('unknown_instruction', struct.pack('<H', 0xBF00), {}),
    ('unsupported_sp_target', struct.pack('<HH', 0xF8D3, 0xD004), {}),
]
for name, instruction, kwargs in rejects:
    result, out, high_out, requests, original, high = run(instruction, **kwargs)
    assert result == 0 and not requests and out == original and high_out == high, name
    print('PASS reject', name)

result, out, high_out, requests, original, high = run(word, host_error=6)
assert result == 0 and len(requests) == 1 and out == original and high_out == high
print('PASS host error returns to original fault handler')

for current, expected in [(0x0C, 0x18), (0x18, 0)]:
    xpsr = 0x01000000 | ((current & 3) << 25) | ((current & 0xFC) << 8)
    result, out, *_ = run(word, xpsr=xpsr)
    observed = ((out[7] >> 25) & 3) | ((out[7] >> 8) & 0xFC)
    assert result == 1 and observed == expected, (current, observed)
print('PASS Thumb IT state advancement and clearing')
print('PASS: 18 forwarding, rejection, host-error and IT-state cases')
