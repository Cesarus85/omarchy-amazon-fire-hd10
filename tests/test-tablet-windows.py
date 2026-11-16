#!/usr/bin/env python3
"""Exercise work-area geometry with rotation, OSK, dialogs and external screens."""
import importlib.machinery,importlib.util,json,unittest
from pathlib import Path
from unittest.mock import patch
p=Path(__file__).resolve().parents[1]/'platform/payload/usr/local/libexec/trona-fit-windows'
l=importlib.machinery.SourceFileLoader('fit',str(p));s=importlib.util.spec_from_loader(l.name,l);m=importlib.util.module_from_spec(s);l.exec_module(m)
class Windows(unittest.TestCase):
 def run_fit(self,transform=0,tags=None,**overrides):
  monitor=dict(id=0,name='DSI-1',width=1200,height=1920,transform=transform,scale=1.25,reserved=[0,40,0,280],x=0,y=0)
  window=dict(mapped=True,floating=True,fullscreen=0,monitor=0,size=[400,300],at=[100,100],address='0xabc',tags=tags or [],pinned=False)
  window.update(overrides);calls=[]
  def hypr(*args):
   if args==('-j','monitors'):return json.dumps([monitor])
   if args==('-j','clients'):return json.dumps([window])
   calls.append(args);return ''
  with patch.object(m,'hypr',hypr):m.fit()
  return ' '.join(str(x) for x in calls)
 def test_auto_floated_apps_fill_each_orientation(self):
  for t in (0,2):self.assertIn('x=936,y=1192',self.run_fit(t,['omarchy-mode-floated']))
  for t in (1,3):self.assertIn('x=1512,y=616',self.run_fit(t,['omarchy-mode-floated']))
 def test_dialogs_and_manual_floats_not_enlarged(self):
  self.assertEqual(self.run_fit(), '')
  self.assertEqual(self.run_fit(tags=['omarchy-mode-floated'],pinned=True),'')
 def test_oversized_dialog_clamped(self):
  self.assertIn('x=936,y=1192',self.run_fit(size=[1800,1600]))
 def test_fullscreen_unmapped_external_and_tiled_untouched(self):
  for kw in [dict(fullscreen=1),dict(fullscreen=2),dict(mapped=False),dict(monitor=1),dict(floating=False)]:
   self.assertEqual(self.run_fit(tags=['omarchy-mode-floated'],**kw),'')
if __name__=='__main__':unittest.main()
