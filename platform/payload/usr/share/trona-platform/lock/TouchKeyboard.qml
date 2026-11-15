import QtQuick

Rectangle {
  id: keyboard
  property bool inputEnabled: true
  property bool shifted: false
  property bool symbols: false
  signal insertText(string value)
  signal backspace()
  signal clear()
  signal submit()
  signal activity()
  color: "#171b25"
  radius: 12
  readonly property var rows: symbols ? [
    ["1","2","3","4","5","6","7","8","9","0"],
    ["!","@","#","$","%","^","&","*","(",")"],
    ["-","_","=","+","[","]","{","}","/","\\"],
    ["ABC",";",":","'","\"",",",".","?","⌫"],
    ["Leeren","Leerzeichen","Entsperren"]
  ] : [
    ["1","2","3","4","5","6","7","8","9","0"],
    ["q","w","e","r","t","z","u","i","o","p","ü"],
    ["a","s","d","f","g","h","j","k","l","ö","ä"],
    ["⇧","y","x","c","v","b","n","m","ß",".","⌫"],
    ["#+=","Leeren","Leerzeichen","Entsperren"]
  ]
  function press(key) {
    keyboard.activity()
    if (key === "⇧") { shifted = !shifted; return }
    if (key === "#+=" || key === "ABC") { symbols = !symbols; return }
    if (key === "⌫") { backspace(); return }
    if (key === "Leeren") { clear(); return }
    if (key === "Entsperren") { submit(); return }
    insertText(key === "Leerzeichen" ? " " : shifted ? key.toUpperCase() : key)
    if (shifted) shifted = false
  }
  Column {
    anchors.fill: parent; anchors.margins: 8; spacing: 6
    Repeater {
      model: keyboard.rows
      delegate: Row {
        id: keyRow
        required property var modelData
        width: parent.width
        height: (keyboard.height - 16 - 24) / 5
        spacing: 5
        Repeater {
          model: keyRow.modelData
          delegate: Rectangle {
            required property string modelData
            width: (keyRow.width - 5 * (keyRow.modelData.length - 1)) / keyRow.modelData.length
            height: keyRow.height
            radius: 7
            color: pointer.pressed ? "#526b96" : modelData === "Entsperren" ? "#354e78" : modelData === "⇧" && keyboard.shifted ? "#526b96" : "#303744"
            opacity: keyboard.inputEnabled ? 1 : 0.5
            Text {
              anchors.centerIn: parent
              text: modelData.length === 1 && keyboard.shifted ? modelData.toUpperCase() : modelData
              color: "#ffffff"; font.pixelSize: modelData.length > 2 ? 17 : 23
            }
            MouseArea {
              id: pointer; anchors.fill: parent; enabled: keyboard.inputEnabled
              onClicked: keyboard.press(modelData)
            }
          }
        }
      }
    }
  }
}
