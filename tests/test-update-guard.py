#!/usr/bin/env python3
"""Verify exact ABI migration scope without invoking pacman or touching a device."""
import importlib.machinery
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch
path=Path(__file__).resolve().parents[1]/'platform/payload/usr/local/sbin/trona-update-check'
loader=importlib.machinery.SourceFileLoader('guard',str(path))
spec=importlib.util.spec_from_loader(loader.name,loader);guard=importlib.util.module_from_spec(spec);loader.exec_module(guard)
class Paths(unittest.TestCase):
 def test_exact_old_platform_can_remove_previous_abi(self):
  with patch.object(guard,'run',return_value='trona-platform 2026.11.15-7'):
   guard.guard_paths(['usr/lib/modules/6.18.56-trona-arch12/extra/test.ko','usr/lib/modules/6.18.56-trona-arch14/extra/test.ko'],True)
   with self.assertRaises(RuntimeError):guard.guard_paths(['usr/lib/modules/6.18.56-trona-arch13/test.ko'],True)
 def test_new_platform_cannot_reintroduce_old_abi(self):
  with patch.object(guard,'run',return_value='trona-platform 2026.11.16-1'):
   with self.assertRaises(RuntimeError):guard.guard_paths(['usr/lib/modules/6.18.56-trona-arch12/test.ko'],True)
 def test_other_packages_and_generic_boot_stay_blocked(self):
  with self.assertRaises(RuntimeError):guard.guard_paths(['usr/lib/modules/6.18.56-trona-arch14/test.ko'],False)
  with patch.object(guard,'run',return_value='trona-platform 2026.11.15-7'):
   with self.assertRaises(RuntimeError):guard.guard_paths(['boot/vmlinuz-linux'],True)
if __name__=='__main__':unittest.main()
