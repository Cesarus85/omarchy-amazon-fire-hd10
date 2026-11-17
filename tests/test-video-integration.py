#!/usr/bin/env python3
"""Failure paths for optional video selection and durable touch recovery."""
import importlib.machinery,importlib.util,json,sys,tempfile,types,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.dont_write_bytecode=True
def load(name,path):
 loader=importlib.machinery.SourceFileLoader(name,str(path))
 spec=importlib.util.spec_from_loader(name,loader);m=importlib.util.module_from_spec(spec);loader.exec_module(m);return m
chrome=load('chrome_video',ROOT/'experiments/video-decode/integration/payload/usr/local/libexec/trona-chrome-video')
standby=load('standby',ROOT/'platform/payload/usr/local/libexec/trona-standby')

class Video(unittest.TestCase):
 def setUp(self):
  self.files={'/run/trona-video/ready.json':json.dumps({'boot_id':'current'}),
   '/proc/sys/kernel/random/boot_id':'current\n',
   '/sys/module/trona_vdec_proxy/parameters/clock_active':'Y\n',
   '/sys/module/trona_vdec_proxy/parameters/errors':'0\n',
   '/sys/class/remoteproc/remoteproc0/state':'running\n'}
  self.uid=0;self.mode=0o100644
 def fake(self,path):
  return types.SimpleNamespace(read_text=lambda:self.files[path],
                               stat=lambda:types.SimpleNamespace(st_uid=self.uid,st_mode=self.mode))
 def enabled(self):
  with patch.object(chrome,'Path',side_effect=self.fake),patch.dict(chrome.os.environ,{},clear=True):
   return chrome.enabled()
 def test_current_healthy_boot(self):self.assertTrue(self.enabled())
 def test_previous_boot(self):
  self.files['/proc/sys/kernel/random/boot_id']='old';self.assertFalse(self.enabled())
 def test_untrusted_ready(self):
  self.uid=1000;self.assertFalse(self.enabled());self.uid=0;self.mode=0o100666;self.assertFalse(self.enabled())
 def test_invalid_marker(self):
  self.files['/run/trona-video/ready.json']='bad';self.assertFalse(self.enabled())
  self.files['/run/trona-video/ready.json']='[]';self.assertFalse(self.enabled())
 def test_proxy_or_firmware_unhealthy(self):
  for name,value in [('clock_active','N'),('errors','1')]:
   key='/sys/module/trona_vdec_proxy/parameters/'+name;old=self.files[key];self.files[key]=value
   self.assertFalse(self.enabled());self.files[key]=old
  self.files['/sys/class/remoteproc/remoteproc0/state']='crashed';self.assertFalse(self.enabled())
 def test_explicit_software(self):
  with patch.dict(chrome.os.environ,{'TRONA_VIDEO':'software'}):self.assertFalse(chrome.enabled())
 def test_features_and_arguments_preserved(self):
  args=chrome.arguments(['--enable-features=One,Two','--app=https://example.invalid',
                         '--enable-features=Two,Three','--incognito'])
  self.assertEqual(args,['--app=https://example.invalid','--incognito',
                        '--enable-features=AcceleratedVideoDecodeLinuxGL,One,Two,Three'])

class Recovery(unittest.TestCase):
 def test_failed_touch_does_not_skip_other_restoration_and_is_retried(self):
  with tempfile.TemporaryDirectory() as t:
   r=Path(t);bl=r/'brightness';bl.write_text('0');udc=r/'UDC';udc.write_text('')
   s={'state':'sleeping','brightness':500,'touch_reset':True,'wifi':True}
   (r/'state.json').write_text(json.dumps(s));actions=[]
   with patch.object(standby,'R',r),patch.object(standby,'BL',bl),patch.object(standby,'UDC',udc),\
        patch.object(standby,'run',side_effect=lambda *a:actions.append(a)),\
        patch.object(standby,'dpms',side_effect=lambda on:actions.append(('display',on))),\
        patch.object(standby,'report'),patch.object(standby,'reset_touch') as reset:
    reset.side_effect=RuntimeError('transient bind failure')
    with self.assertRaises(RuntimeError):standby.recover()
    self.assertIn(('systemctl','start','trona-wifi.service'),actions)
    self.assertIn(('display',True),actions);self.assertEqual(bl.read_text(),'500')
    self.assertTrue(json.loads((r/'state.json').read_text())['touch_reset'])
    reset.side_effect=None;standby.recover()
    self.assertFalse(json.loads((r/'state.json').read_text())['touch_reset'])
    standby.recover();self.assertEqual(reset.call_count,2)

if __name__=='__main__':unittest.main()
