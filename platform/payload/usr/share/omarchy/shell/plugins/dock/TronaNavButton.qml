import QtQuick
import QtQuick.Controls as Controls
import Quickshell
import Quickshell.Hyprland
import qs.Commons as Commons

Controls.ToolButton {
  id: button
  property bool closeWindow: false
  property bool allowClose: true
  property string pressedAddress: ""
  signal back()
  focusPolicy: Qt.NoFocus
  Accessible.name: closeWindow ? "Fenster schließen" : "Zurück / Esc"
  text: closeWindow ? "×" : "←"
  enabled: allowClose && !cooldown.running
  background: Rectangle {
    color: button.down ? Commons.Color.accent : "transparent"
    radius: 6
  }
  contentItem: Text {
    text: button.text
    color: button.down ? Commons.Color.background : Commons.Color.bar.text
    font.pixelSize: Math.min(36, button.height * 0.64)
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
  }
  function activeAddress() {
    for (const client of Hyprland.toplevels.values) {
      if (!client.activated) continue
      const address = String(client.address).replace(/^0x/i, "")
      if (/^[0-9a-f]+$/i.test(address)) return "0x" + address
    }
    return ""
  }
  onPressed: pressedAddress = activeAddress()
  onClicked: {
    if (!closeWindow) {
      back()
    } else if (pressedAddress && pressedAddress === activeAddress()) {
      cooldown.restart()
      Hyprland.dispatch('hl.dsp.window.close({ window = "address:' + pressedAddress + '" })')
      pressedAddress = ""
    }
  }
  Timer { id: cooldown; interval: 650 }
}
