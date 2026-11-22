import QtQuick
import QtQuick.Controls.Basic as Controls
import Quickshell
import Quickshell.Wayland
import qs.Commons as Commons

PanelWindow {
  id: overview
  required property var dock
  property bool opened: false
  property string appKey: ""
  function open(key) {
    focusAfterClose.stop()
    appKey = key
    opened = true
    grid.positionViewAtBeginning()
  }
  function dismiss() { opened = false }
  function choose(address, handle) {
    // Unmap the exclusive layer before Hyprland raises the chosen client.
    focusAfterClose.address = address
    focusAfterClose.handle = handle
    dismiss()
    focusAfterClose.restart()
  }
  Timer {
    id: focusAfterClose
    property string address: ""
    property var handle: null
    interval: 120
    onTriggered: {
      overview.focusCurrentWindow(address, handle)
      handle = null
    }
  }
  function focusCurrentWindow(address, handle) {
    for (const item of dock.items) {
      for (const win of item.windows) {
        if (win.address !== address || !handle || win.toplevel !== handle) continue
        dock.bringForward(win)
        return
      }
    }
  }

  visible: opened
  anchors { top: true; bottom: true; left: true; right: true }
  exclusionMode: ExclusionMode.Normal
  exclusiveZone: 0
  color: "transparent"
  WlrLayershell.namespace: "trona-window-overview"
  WlrLayershell.layer: WlrLayer.Overlay
  WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive

  readonly property var cards: {
    const result = []
    if (opened && dock) {
      for (const item of dock.items) {
        if (appKey && item.key !== appKey) continue
        for (const win of item.windows) {
          if (win.toplevel) result.push({win: win, item: item})
        }
      }
    }
    return result
  }

  Item {
    anchors.fill: parent
    focus: true
    Keys.onEscapePressed: event => { overview.dismiss(); event.accepted = true }
    Rectangle { anchors.fill: parent; color: "#99000000" }
    MouseArea { anchors.fill: parent; onClicked: overview.dismiss() }

    Rectangle {
      id: panel
      anchors.centerIn: parent
      width: Math.min(1080, Math.max(1, parent.width - 24))
      height: Math.min(parent.height - 16, 76 + Math.max(104, Math.ceil(grid.count / grid.columns) * grid.cellHeight))
      radius: 14
      color: Commons.Color.background
      border.color: Commons.Color.muted
      MouseArea { anchors.fill: parent }
      Item {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right; margins: 12 }
        height: 44
        Text {
          anchors { left: parent.left; right: closeButton.left; verticalCenter: parent.verticalCenter; leftMargin: 6; rightMargin: 8 }
          readonly property var group: overview.appKey && overview.dock ? overview.dock.itemFor(overview.appKey) : null
          text: group ? (overview.dock.entryFor(group)?.name || group.key) : "Fensterübersicht"
          textFormat: Text.PlainText
          elide: Text.ElideRight
          color: Commons.Color.foreground
          font.pixelSize: 23; font.bold: true
        }
        Controls.ToolButton {
          id: closeButton
          anchors.right: parent.right
          width: 48; height: 44
          text: "×"; font.pixelSize: 26
          Accessible.name: "Übersicht schließen"
          palette.buttonText: Commons.Color.foreground
          onClicked: overview.dismiss()
        }
      }
      GridView {
        id: grid
        anchors { top: header.bottom; left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 }
        clip: true
        readonly property int columns: Math.max(1, Math.min(3, Math.floor(width / 280)))
        cellWidth: width / columns
        cellHeight: Math.max(135, Math.min(295, cellWidth * 0.76, overview.height - 98))
        boundsBehavior: Flickable.StopAtBounds
        model: overview.cards
        cacheBuffer: 0
        Controls.ScrollBar.vertical: Controls.ScrollBar {}
        delegate: Item {
          id: card
          required property var modelData
          width: grid.cellWidth; height: grid.cellHeight
          readonly property var handle: modelData.win.toplevel
          readonly property string caption: handle?.title || handle?.appId || modelData.item.key
          Accessible.role: Accessible.Button
          Accessible.name: caption
          Accessible.onPressAction: overview.choose(modelData.win.address, handle)
          Rectangle {
            anchors { fill: parent; margins: 4 }
            radius: 9
            color: Qt.tint(Commons.Color.background, Qt.rgba(Commons.Color.foreground.r, Commons.Color.foreground.g, Commons.Color.foreground.b, tap.pressed ? 0.2 : 0.085))
            Item {
              id: picture
              anchors { top: parent.top; left: parent.left; right: parent.right; margins: 8; bottom: captionRow.top }
              clip: true
              // Frame once per visible card. Hidden overviews have no delegates.
              ScreencopyView {
                id: preview
                captureSource: card.handle
                live: false
                paintCursor: false
                width: picture.width
                height: sourceSize.width > 0 ? width * sourceSize.height / sourceSize.width : picture.height
                y: Math.max(0, (picture.height - height) / 2)
              }
              Image {
                anchors.centerIn: parent
                visible: !preview.hasContent
                width: 48; height: 48
                source: overview.dock.iconFor(card.modelData.item)
              }
            }
            Item {
              id: captionRow
              anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 10 }
              height: 38
              Image {
                id: appIcon
                anchors.verticalCenter: parent.verticalCenter
                width: 26; height: 26
                source: overview.dock.iconFor(card.modelData.item)
              }
              Text {
                anchors { left: appIcon.right; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 9 }
                text: card.caption
                textFormat: Text.PlainText
                color: Commons.Color.foreground
                font.pixelSize: 16; font.bold: true
                maximumLineCount: 2; wrapMode: Text.WrapAnywhere; elide: Text.ElideRight
              }
            }
          }
          TapHandler {
            id: tap
            gesturePolicy: TapHandler.DragThreshold
            onTapped: overview.choose(card.modelData.win.address, card.handle)
          }
        }
      }
      Text {
        anchors.centerIn: grid
        visible: grid.count === 0
        text: "Keine offenen Fenster"
        color: Commons.Color.foreground
      }
    }
  }
}
