import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import qs.Commons as Commons

// The idle touch target lies entirely inside the bar's status group.
Item {
  id: center
  required property var bar
  required property bool fullscreen
  required property var display
  property bool opened: false
  property var audio
  property var monitor
  property var network
  property var bluetooth

  function open() {
    if (!visible || opened) return
    audio = bar.findPanelWidget("omarchy.audio")
    monitor = bar.findPanelWidget("omarchy.monitor")
    network = bar.findPanelWidget("omarchy.network")
    bluetooth = bar.findPanelWidget("omarchy.bluetooth")
    if (monitor) monitor.refresh()
    bar.requestPopout(center)
    opened = true
  }
  function close() {
    opened = false
    bar.releasePopout(center)
  }
  function details(name) {
    close()
    bar.summonBarWidget("omarchy." + name)
  }
  Component.onDestruction: if (opened) close()

  component StatusTouch: Item {
    id: hit
    property bool swipeOnly: false
    function activate(dx, dy) {
      const across = Math.abs(dx)
      if ((dy >= 24 && dy > across) || (!swipeOnly && Math.abs(dy) <= 10 && across <= 10))
        center.open()
    }
    MouseArea {
      anchors.fill: parent
      property point start
      onPressed: m => start = Qt.point(m.x, m.y)
      onReleased: m => hit.activate(m.x - start.x, m.y - start.y)
    }
    MultiPointTouchArea {
      anchors.fill: parent
      minimumTouchPoints: 1; maximumTouchPoints: 1
      mouseEnabled: false
      property point start
      onPressed: points => start = Qt.point(points[0].x, points[0].y)
      onGestureStarted: gesture => gesture.grab()
      onReleased: points => hit.activate(points[0].x - start.x, points[0].y - start.y)
    }
  }
  StatusTouch { anchors.fill: parent }

  // In fullscreen the bar itself has an empty input region. Reserve only
  // the top few pixels of the corner for an intentional pull from the edge.
  PanelWindow {
    screen: center.display
    visible: center.visible && center.fullscreen && !center.opened
    anchors { top: true; right: true }
    implicitWidth: 100
    implicitHeight: 7
    exclusionMode: ExclusionMode.Ignore
    color: "transparent"
    WlrLayershell.namespace: "trona-control-edge"
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    StatusTouch { anchors.fill: parent; swipeOnly: true }
  }

  FileView {
    id: autoFlag
    path: Quickshell.env("HOME") + "/.local/state/trona/auto-rotate"
    watchChanges: true
    onFileChanged: reload()
  }
  Process {
    id: rotation
    command: ["/usr/local/bin/trona-auto-rotate", "toggle"]
    onExited: autoFlag.reload()
  }

  PanelWindow {
    id: popup
    screen: center.display
    visible: center.opened
    anchors { top: true; bottom: true; left: true; right: true }
    exclusiveZone: 0
    exclusionMode: ExclusionMode.Normal
    color: "transparent"
    WlrLayershell.namespace: "trona-control-center"
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive

    Item {
      anchors.fill: parent
      focus: true
      Keys.onEscapePressed: event => { center.close(); event.accepted = true }
      Rectangle { anchors.fill: parent; color: "#51000000" }
      MouseArea { anchors.fill: parent; onClicked: center.close() }

      Rectangle {
        id: card
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        width: Math.min(parent.width - 16, center.display.width > center.display.height ? 410 : parent.width - 16)
        height: Math.min(parent.height - 16, contents.implicitHeight + 28)
        radius: 16
        color: Commons.Color.background
        border.color: Commons.Color.muted
        // Consume taps on the card's padding as well as its controls.
        MouseArea { anchors.fill: parent }
        Flickable {
          anchors.fill: parent
          anchors.margins: 14
          clip: true
          contentWidth: width
          contentHeight: contents.implicitHeight
          interactive: contentHeight > height
          boundsBehavior: Flickable.StopAtBounds

          ColumnLayout {
            id: contents
            width: parent.width
            spacing: 6
            Item {
              Layout.fillWidth: true
              implicitHeight: 48
              Text {
                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                text: "Kontrollzentrum"; color: Commons.Color.foreground
                font.pixelSize: 21; font.bold: true
              }
              Controls.ToolButton {
                anchors.right: parent.right
                width: 48; height: 48
                text: "×"; font.pixelSize: 24; palette.buttonText: Commons.Color.foreground
                Accessible.name: "Kontrollzentrum schließen"
                onClicked: center.close()
              }
            }
            Level {
              title: "Helligkeit"
              available: !!center.monitor && center.monitor.brightnessAvailable
              percent: center.monitor ? center.monitor.brightnessPercent : 0
              minimum: 1
              onAdjusted: value => center.monitor.previewBrightness(value)
            }
            Level {
              title: "Lautstärke"
              available: !!center.audio && center.audio.hasOutput
              percent: center.audio && !center.audio.outputMuted ? Math.round(center.audio.outputVolume * 100) : 0
              onAdjusted: value => {
                if (center.audio.outputMuted && value > 0) center.audio.volumeSink.audio.muted = false
                center.audio.setOutputVolume(value / 100)
              }
            }
            GridLayout {
              Layout.fillWidth: true
              columns: width >= 540 ? 3 : 2
              columnSpacing: 6; rowSpacing: 6
              Tile {
                title: "WLAN"
                detail: center.network?.connectedWifiNetwork?.name || "Kein WLAN verbunden"
                onClicked: center.details("network")
              }
              Tile {
                title: "Bluetooth"
                detail: center.bluetooth?.connectedDevices?.map(d => d.name).join(", ") ||
                        (center.bluetooth?.adapter?.enabled ? "Kein Gerät verbunden" : "Aus")
                onClicked: center.details("bluetooth")
              }
              Tile {
                title: "Drehsperre"
                detail: autoFlag.text().trim() === "1" ? "Aus" : "Ein"
                enabled: !rotation.running
                onClicked: rotation.running = true
              }
            }
          }
        }
        DragHandler {
          target: null
          xAxis.enabled: false
          property bool pulled: false
          onActiveTranslationChanged: if (active) pulled = activeTranslation.y < -32
          onActiveChanged: {
            if (active) pulled = false
            else if (pulled) { pulled = false; center.close() }
          }
          // Scroll the controls instead when a very short work area clips them.
          enabled: contents.implicitHeight <= card.height - 28
        }
      }
    }
  }

  component Level: ColumnLayout {
    required property string title
    required property bool available
    required property int percent
    property int minimum: 0
    signal adjusted(int value)
    Layout.fillWidth: true
    spacing: 0
    Text {
      text: parent.title + "   " + (parent.available ? slider.value.toFixed(0) + " %" : "–")
      color: Commons.Color.foreground; font.pixelSize: 15
    }
    Controls.Slider {
      id: slider
      Layout.fillWidth: true
      implicitHeight: 42
      from: parent.minimum; to: 100; stepSize: 1
      enabled: parent.available
      Accessible.name: parent.title
      value: parent.percent
      palette.highlight: Commons.Color.accent
      palette.mid: Commons.Color.muted
      onMoved: parent.adjusted(Math.round(value))
    }
  }

  component Tile: Controls.AbstractButton {
    id: tile
    required property string title
    required property string detail
    Layout.fillWidth: true
    Layout.preferredWidth: 1
    implicitHeight: 52
    Accessible.name: title + ", " + detail
    background: Rectangle {
      radius: 9
      color: Qt.tint(Commons.Color.background, Qt.rgba(Commons.Color.foreground.r, Commons.Color.foreground.g, Commons.Color.foreground.b, tile.down ? 0.22 : 0.10))
    }
    contentItem: Column {
      leftPadding: 10; rightPadding: 10
      topPadding: 6
      Text { text: tile.title; color: Commons.Color.foreground; font.bold: true; font.pixelSize: 14 }
      Text {
        width: parent.width - 20
        text: tile.detail; color: Commons.Color.foreground; font.pixelSize: 13
        elide: Text.ElideRight
      }
    }
  }
}
