import QtQuick
import Quickshell
import Quickshell.Hyprland
import Quickshell.Io
import Quickshell.Wayland
import qs.Commons as Commons

// This small surface only takes touches that start on the bottom-centre grip.
// The touch sequence stays with it as the finger moves up across the keyboard.
PanelWindow {
  id: edge
  required property bool tracking
  required property bool interacting
  property bool keyboardMapped: false
  property bool peeking: false
  property int revision: 0

  visible: tracking && keyboardMapped
  anchors.bottom: true
  implicitWidth: 144
  implicitHeight: 12
  exclusionMode: ExclusionMode.Ignore
  WlrLayershell.namespace: "trona-keyboard-grip"
  WlrLayershell.layer: WlrLayer.Overlay
  WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
  color: "transparent"

  function refresh() {
    revision++
    if (tracking) settle.restart()
    else keyboardMapped = false
  }
  onTrackingChanged: refresh()
  Component.onCompleted: refresh()
  onKeyboardMappedChanged: peeking = false

  Connections {
    target: Hyprland
    function onRawEvent(event) {
      if (event.name === "openlayer" && event.data === "omarchy-dock-apps")
        edge.peeking = false
      if ((event.name === "openlayer" || event.name === "closelayer") && event.data === "wlroots") {
        // A replacement keyboard can map above a still-mapped grip on rotation.
        // Remap the grip last even when close/open arrive in the same frame.
        edge.keyboardMapped = false
        edge.refresh()
      }
    }
  }
  Connections {
    target: edge.screen
    function onWidthChanged() { edge.keyboardMapped = false; edge.refresh() }
    function onHeightChanged() { edge.keyboardMapped = false; edge.refresh() }
  }
  // Read the actual layer on initial load and on keyboard map/unmap. Coalesce
  // restarts during rotation and discard a snapshot superseded by a new event.
  Timer {
    id: settle
    interval: 40
    onTriggered: {
      if (snapshot.running) { restart(); return }
      snapshot.revision = edge.revision
      snapshot.running = true
    }
  }
  Process {
    id: snapshot
    property int revision: -1
    command: ["/usr/bin/hyprctl", "-j", "layers"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: {
        if (!edge.tracking || snapshot.revision !== edge.revision || !text.trim()) return
        try {
          const levels = JSON.parse(text)[edge.screen.name]?.levels ?? {}
          edge.keyboardMapped = Object.values(levels).some(group => group.some(layer => layer.namespace === "wlroots"))
        } catch (error) { console.warn("Tablet keyboard layer query:", error) }
      }
    }
  }

  Rectangle {
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 2
    width: 62
    height: 4
    radius: 2
    color: edge.peeking ? Commons.Color.accent : Commons.Color.bar.text
  }
  MultiPointTouchArea {
    anchors.fill: parent
    maximumTouchPoints: 1
    mouseEnabled: false
    property real startX
    property real startY
    onPressed: points => { startX = points[0].x; startY = points[0].y }
    onGestureStarted: gesture => gesture.grab()
    onReleased: points => {
      const up = startY - points[0].y
      if (up >= 28 && up > Math.abs(points[0].x - startX)) {
        edge.peeking = !edge.peeking
      }
    }
  }
  Timer {
    interval: 6000
    running: edge.peeking && !edge.interacting
    onTriggered: edge.peeking = false
  }
}
