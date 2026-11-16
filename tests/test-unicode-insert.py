#!/usr/bin/env python3
import importlib.machinery,importlib.util,os,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
p=Path(__file__).resolve().parents[1]/'platform/payload/usr/local/libexec/trona-insert-unicode'
l=importlib.machinery.SourceFileLoader('insert',str(p));s=importlib.util.spec_from_loader(l.name,l);m=importlib.util.module_from_spec(s);l.exec_module(m)
class UnicodeInsert(unittest.TestCase):
 def exercise(self,reads,focus=None):
  window={'class':'chrome-test','address':'0x123'}
  with tempfile.TemporaryDirectory() as d, patch.dict(os.environ,XDG_RUNTIME_DIR=d), patch.object(sys,'argv',['insert','1f642']), patch.object(m,'active',side_effect=focus or [window]*3), patch.object(m,'read_clip',side_effect=reads), patch.object(m,'copy') as cp, patch.object(m.subprocess,'run') as run, patch.object(m.time,'sleep'):
   try:result=m.main()
   except RuntimeError:result='error'
   return result,cp.call_args_list,run.call_args_list
 def test_native_apps_use_key_events_without_clipboard(self):
  with patch.object(sys,'argv',['insert','1f642']),patch.object(m,'active',return_value={'class':'foot'}),patch.object(m,'read_clip') as read:
   self.assertEqual(m.main(),2);read.assert_not_called()
 def test_utf8_text_restored_after_paste(self):
  result,calls,run=self.exercise([b'text/plain;charset=utf-8\n',b'previous','🙂'.encode()])
  self.assertEqual(result,0);self.assertEqual(calls[0].args,('🙂'.encode(),'text/plain;charset=utf-8'));self.assertEqual(calls[-1].args,(b'previous','text/plain;charset=utf-8'))
  self.assertEqual(run[0].args[0],['wtype','-M','ctrl','-k','v','-m','ctrl'])
 def test_binary_clipboard_restored(self):
  _,calls,_=self.exercise([b'image/png\ntext/plain\n',b'\x89PNG\x00','🙂'.encode()])
  self.assertEqual(calls[-1].args,(b'\x89PNG\x00','image/png'))
 def test_concurrent_user_copy_is_not_overwritten(self):
  _,calls,_=self.exercise([b'text/plain\n',b'previous',b'new user data'])
  self.assertEqual(len(calls),1)
 def test_focus_change_prevents_paste_and_restores_clipboard(self):
  w={'class':'google-chrome','address':'0x123'}
  result,calls,run=self.exercise([b'text/plain\n',b'previous','🙂'.encode()],[w,w,{'address':'0x456'}])
  self.assertEqual(result,'error');self.assertEqual(run,[]);self.assertEqual(calls[-1].args,(b'previous','text/plain'))
 def test_empty_clipboard_cleared_after_paste(self):
  result,_,run=self.exercise([None,'🙂'.encode()]);self.assertEqual(result,0)
  self.assertEqual(run[-1].args[0],['wl-copy','--clear'])
if __name__=='__main__':unittest.main()
