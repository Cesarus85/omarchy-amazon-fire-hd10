#!/usr/bin/env python3
"""Remove only the exact obsolete browser policy from early USB-only bring-up."""
import json
import os
from pathlib import Path

POLICY = Path('/etc/opt/chrome/policies/managed/trona-usb-proxy.json')
LEGACY = {'ProxyMode': 'fixed_servers',
          'ProxyServer': 'http://127.0.0.1:18080',
          'ProxyBypassList': 'localhost;127.0.0.1;169.254.77.2'}

def remove_legacy(path):
    if path.is_symlink():
        raise RuntimeError('Refusing a symlinked policy')
    if not path.exists():
        return 'absent'
    if json.loads(path.read_text()) != LEGACY:
        raise RuntimeError('Different policy; review it manually. Nothing removed.')
    path.unlink()
    return 'removed'

def main():
    if os.geteuid() != 0:
        raise SystemExit('Run as root on the tablet')
    if b'amazon,trona' not in Path('/sys/firmware/devicetree/base/compatible').read_bytes().split(b'\0'):
        raise SystemExit('Only Amazon Trona is supported')
    if POLICY.exists() and POLICY.stat().st_uid != 0:
        raise SystemExit('Unexpected policy owner; nothing removed')
    print('Legacy USB browser policy:', remove_legacy(POLICY))
    print('Reload the page. If necessary, fully exit and reopen the browser.')

if __name__ == '__main__':
    main()
