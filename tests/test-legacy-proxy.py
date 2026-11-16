#!/usr/bin/env python3
import importlib.util,json,tempfile,unittest
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'tools/trona-remove-legacy-proxy.py'
s=importlib.util.spec_from_file_location('proxy',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class LegacyProxy(unittest.TestCase):
 def test_exact_policy_and_idempotent_absence(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'policy.json';p.write_text(json.dumps(m.LEGACY))
   self.assertEqual(m.remove_legacy(p),'removed')
   self.assertEqual(m.remove_legacy(p),'absent')
 def test_preserves_other_and_malformed_policies(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'policy.json'
   for text in [json.dumps(dict(m.LEGACY,ProxyServer='http://other:8080')),json.dumps(dict(m.LEGACY,ExtraPolicy=True)),'{invalid']:
    p.write_text(text)
    with self.assertRaises((RuntimeError,ValueError)):m.remove_legacy(p)
    self.assertEqual(p.read_text(),text)
 def test_refuses_symlinks(self):
  with tempfile.TemporaryDirectory() as d:
   target=Path(d)/'original';target.write_text(json.dumps(m.LEGACY));p=Path(d)/'policy';p.symlink_to(target)
   with self.assertRaises(RuntimeError):m.remove_legacy(p)
   self.assertTrue(target.exists())
if __name__=='__main__':unittest.main()
