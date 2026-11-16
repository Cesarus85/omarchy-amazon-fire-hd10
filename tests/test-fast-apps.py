#!/usr/bin/env python3
import importlib.machinery,importlib.util,unittest,subprocess,tempfile
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'platform/payload/usr/local/sbin/trona-fast-apps'
l=importlib.machinery.SourceFileLoader('fast',str(p));s=importlib.util.spec_from_loader(l.name,l);m=importlib.util.module_from_spec(s);l.exec_module(m)
LIB='''import QtQuick
  property var pendingIconIndex: ({})
    if (!iconIndexScan.running) iconIndexScan.running = true
      Util.execDetached("omarchy-launch-tui " + command.map(function(word) { return Util.shellQuote(word) }).join(" "))
    Util.execDetached("uwsm-app -- gtk-launch " + Util.shellQuote(id + ".desktop"))
    onExited: root.iconIndex = root.pendingIconIndex
'''
DOCK='    var target = "window = \\\"address:" + address + "\\\""\n'
class FastApps(unittest.TestCase):
 def test_safe_argv_and_scope_retained(self):
  out=m.patch_library(LIB)
  self.assertIn('["uwsm-app", "--", "gtk-launch", id + ".desktop"]',out)
  self.assertNotIn('Util.execDetached(',out)
  self.assertIn('if (changed) root.iconIndex = next',out)
  self.assertIn('exitCode !== 0',out)
  self.assertEqual(out,m.patch_library(out))
 def test_native_focus_only_for_non_minimized_windows(self):
  out=m.patch_dock(DOCK)
  self.assertIn('if (!window.minimized)',out)
  self.assertIn('Hyprland.dispatch(',out)
  self.assertEqual(out,m.patch_dock(out))
 def test_changed_second_file_leaves_first_untouched(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);lib=root/'services/AppLibrary.qml';dock=root/'plugins/dock/Dock.qml'
   lib.parent.mkdir(parents=True);dock.parent.mkdir(parents=True);lib.write_text(LIB);dock.write_text('upstream changed')
   result=subprocess.run(['python3',str(p),'--shell-dir',d],capture_output=True)
   self.assertNotEqual(result.returncode,0)
   self.assertEqual(lib.read_text(),LIB)
   self.assertEqual(dock.read_text(),'upstream changed')
if __name__=='__main__':unittest.main()
