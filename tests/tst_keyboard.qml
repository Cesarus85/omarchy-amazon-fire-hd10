import QtQuick
import QtTest
import "file:/usr/share/trona-platform/lock" as L

Item {
  width: 1200; height: 900
  L.TouchKeyboard { id: keyboard; width: 1080; height: 350 }
  TestCase {
  id: tests
  name: "TronaProtectedKeyboard"
  when: windowShown
  SignalSpy { id: letters; target: keyboard; signalName: "insertText" }
  SignalSpy { id: submit; target: keyboard; signalName: "submit" }
  SignalSpy { id: deletion; target: keyboard; signalName: "backspace" }
  function tap(row, column, count) {
    var w=(keyboard.width-16-5*(count-1))/count
    var h=(keyboard.height-40)/5
    mouseClick(keyboard,8+column*(w+5)+w/2,8+row*(h+6)+h/2)
  }
  function init() { keyboard.inputEnabled=true;keyboard.symbols=false;keyboard.shifted=false;letters.clear();submit.clear();deletion.clear() }
  function test_layouts_data() { return [{tag:"portrait",w:776,h:350},{tag:"landscape",w:1080,h:350},{tag:"compact",w:776,h:280}] }
  function test_layouts(data) {
    keyboard.width=data.w;keyboard.height=data.h;wait(50)
    tap(0,2,10);compare(letters.signalArguments[0][0],"3")
    tap(3,2,11);compare(letters.signalArguments[1][0],"x")
    tap(3,9,11);compare(letters.signalArguments[2][0],".")
    tap(3,10,11);compare(deletion.count,1)
    tap(4,3,4);compare(submit.count,1)
    keyboard.inputEnabled=false;tap(0,0,10);compare(letters.count,3)
  }
  function test_shift_and_symbols() {
    keyboard.width=1080;keyboard.height=350;wait(50)
    tap(3,0,11);tap(3,2,11);compare(letters.signalArguments[0][0],"X");verify(!keyboard.shifted)
    tap(4,0,4);wait(50);verify(keyboard.symbols)
    tap(1,0,10);compare(letters.signalArguments[1][0],"!")
    tap(3,0,9);verify(!keyboard.symbols)
  }
}
}
