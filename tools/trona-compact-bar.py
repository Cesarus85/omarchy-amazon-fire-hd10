#!/usr/bin/env python3
"""Apply a compact, touch-friendly layout to the current user's Omarchy bar.

Run as the desktop user. No system QML is patched. Omitted widget settings are
retained inside shell.json, so they can be reused through normal customization.
"""
import json
import os
from pathlib import Path

SECTIONS = ('left', 'centerBegin', 'center', 'centerEnd', 'right', 'tray')
LEFT = ('omarchy.menu', 'trona.rotation', 'trona.autorotate',
        'trona.keyboard', 'trona.scale', 'omarchy.workspaces')
RIGHT = ('omarchy.bluetooth', 'omarchy.network',
         'omarchy.audio', 'omarchy.monitor', 'omarchy.clock', 'omarchy.power')


def compact(data):
    bar = data['bar']
    layout = bar['layout']
    entries = {}
    for section in SECTIONS:
        for item in layout.get(section, []):
            if isinstance(item, str):
                item = {'id': item}
            if isinstance(item, dict) and item.get('id'):
                entries[item['id']] = dict(item)
    for item in bar.get('tronaCompactHidden', []):
        if isinstance(item, dict) and item.get('id'):
            entries.setdefault(item['id'], dict(item))
    for name in LEFT + RIGHT:
        if name not in entries:
            raise ValueError('Missing expected tablet widget: ' + name)
    # Keep the native automatic-rotation state text; its helper owns that field.
    entries['trona.rotation'].update(text='↻', horizontalMargin=8)
    entries['trona.rotation']['tooltip'] = 'Manuell drehen und automatische Drehung sperren'
    entries['omarchy.clock'].update(format='HH:mm', formatAlt='HH:mm')
    layout.update({section: [] for section in SECTIONS})
    layout['left'] = [entries[name] for name in LEFT]
    layout['right'] = [entries[name] for name in RIGHT]
    bar['tronaCompactHidden'] = [item for name, item in entries.items()
                                if name not in LEFT + RIGHT]
    bar.update(position='top', locked=True)
    return data


def main():
    if os.geteuid() == 0:
        raise SystemExit('Run as the desktop user, not root')
    config = Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config')) / 'omarchy/shell.json'
    data = compact(json.loads(config.read_text()))
    temp = config.with_name(f'.shell-compact-{os.getpid()}.tmp')
    try:
        with temp.open('x') as f:
            os.chmod(temp, config.stat().st_mode & 0o777)
            json.dump(data, f, ensure_ascii=False, indent=2)
            f.write('\n')
            f.flush()
            os.fsync(f.fileno())
        temp.replace(config)
    finally:
        if temp.exists():
            temp.unlink()
    print('Compact tablet bar applied; Omarchy reloads shell.json automatically.')


if __name__ == '__main__':
    main()
