#!/usr/bin/env python3
"""Opt-in, reversible governor selection for the qualified Trona 800 mV table.

No voltage, min/max frequency, thermal, or CPU-online writes. State belongs in
/run so an interrupted session cannot restore settings from an earlier boot.
"""
import argparse
import json
import os
from pathlib import Path

CPU = Path('/sys/devices/system/cpu/cpufreq')
BOARD = Path('/sys/firmware/devicetree/base/compatible')
STATE = Path('/run/trona-responsive-cpu/governors.json')
FREQUENCIES = [793000, 910000, 1014000, 1131000, 1248000]


def validate(cpu=CPU, board=BOARD):
    if b'amazon,trona' not in board.read_bytes().split(b'\0'):
        raise RuntimeError('Only the qualified Amazon Trona board is supported')
    policies = [cpu / 'policy0', cpu / 'policy4']
    for p in policies:
        frequencies = list(map(int, (p / 'scaling_available_frequencies').read_text().split()))
        if frequencies != FREQUENCIES:
            raise RuntimeError(f'{p.name}: unexpected CPU table; leaving policy unchanged')
        if (p / 'scaling_driver').read_text().strip() != 'mtk-cpufreq':
            raise RuntimeError(f'{p.name}: unexpected CPU driver')
        for name, expected in [('cpuinfo_min_freq', 793000), ('cpuinfo_max_freq', 1248000)]:
            if int((p / name).read_text()) != expected:
                raise RuntimeError(f'{p.name}: unexpected hardware frequency limit')
        if 'ondemand' not in (p / 'scaling_available_governors').read_text().split():
            raise RuntimeError(f'{p.name}: ondemand governor unavailable')
    return policies


def apply(cpu=CPU, board=BOARD, state=STATE):
    policies = validate(cpu, board)  # Validate both before changing either.
    if state.exists():
        raise RuntimeError('A saved policy exists; restore it before applying again')
    original = {p.name: (p / 'scaling_governor').read_text().strip() for p in policies}
    state.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    with state.open('x') as f:
        os.chmod(state, 0o600)
        json.dump(original, f)
        f.flush()
        os.fsync(f.fileno())
    try:
        for p in policies:
            (p / 'scaling_governor').write_text('ondemand')
            if (p / 'scaling_governor').read_text().strip() != 'ondemand':
                raise RuntimeError(f'{p.name}: governor readback failed')
    except BaseException:
        # Preserve the recovery record if any restoration fails.
        for p in policies:
            (p / 'scaling_governor').write_text(original[p.name])
        state.unlink()
        raise
    print('Both CPU clusters: ondemand; original frequencies and thermal limits retained')


def restore(cpu=CPU, board=BOARD, state=STATE):
    if not state.exists():
        return
    policies = validate(cpu, board)
    original = json.loads(state.read_text())
    if set(original) != {p.name for p in policies}:
        raise RuntimeError('Unexpected restoration record')
    for p in policies:
        if original[p.name] not in (p / 'scaling_available_governors').read_text().split():
            raise RuntimeError(f'{p.name}: saved governor unavailable')
    for p in policies:
        current = (p / 'scaling_governor').read_text().strip()
        if current == 'ondemand':
            (p / 'scaling_governor').write_text(original[p.name])
        elif current != original[p.name]:
            print(f'{p.name}: retaining subsequent external change to {current}')
    state.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['apply', 'restore', 'status'])
    args = parser.parse_args()
    if args.action == 'status':
        for p in validate():
            print(p.name, {n: (p / n).read_text().strip() for n in
                          ['scaling_governor', 'scaling_cur_freq', 'scaling_min_freq', 'scaling_max_freq']})
    else:
        if os.geteuid() != 0:
            parser.error('Root is required to change CPU policy')
        (apply if args.action == 'apply' else restore)()


if __name__ == '__main__':
    main()
