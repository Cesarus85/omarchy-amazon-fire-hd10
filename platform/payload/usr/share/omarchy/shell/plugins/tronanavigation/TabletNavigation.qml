pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import Quickshell
import Quickshell.Hyprland
import Quickshell.Wayland
import qs.Commons as Commons

Item {
    id: root
    property var shell: null
    property var manifest: null
    property bool gridOpen: false
    readonly property int stripHeight: 52

    Connections {
        target: Hyprland
        function onRawEvent(event) {
            if (event.name === "openlayer" && event.data === "omarchy-dock-apps")
                root.gridOpen = true
            if (event.name === "closelayer" && event.data === "omarchy-dock-apps")
                root.gridOpen = false
        }
    }

    function activeAddress() {
        for (const window of Hyprland.toplevels.values) {
            if (!window.activated) continue
            let address = String(window.address)
            if (!address.startsWith("0x")) address = "0x" + address
            if (/^0x[0-9a-f]+$/i.test(address)) return address
        }
        return ""
    }

    component Actions: Row {
        id: buttons
        spacing: 6
        Repeater {
            model: ["Apps", "Zurück / Esc", "Schließen"]
            Controls.Button {
                id: button
                required property int index
                required property string modelData
                property string pressedAddress: ""
                text: modelData
                width: Math.min(160, Math.floor((buttons.width - 2 * buttons.spacing) / 3))
                height: 48
                focusPolicy: Qt.NoFocus
                enabled: index !== 2 || (!root.gridOpen && !closeGuard.running)
                onPressed: if (index === 2) pressedAddress = root.activeAddress()
                onClicked: {
                    if (index === 0) {
                        Quickshell.execDetached(["/usr/bin/omarchy-shell", "omarchy.dock", "apps"])
                    } else if (index === 1) {
                        Quickshell.execDetached(["/usr/bin/wtype", "-k", "Escape"])
                    } else if (pressedAddress && pressedAddress === root.activeAddress()) {
                        closeGuard.restart()
                        Hyprland.dispatch('hl.dsp.window.close({ window = "address:' + pressedAddress + '" })')
                    }
                }
                background: Rectangle {
                    radius: 7
                    color: button.down ? Commons.Color.accent : Commons.Color.bar.background
                    border.color: Commons.Color.muted
                    opacity: button.enabled ? 1 : 0.45
                }
                contentItem: Text {
                    text: button.text
                    color: button.down ? Commons.Color.background : Commons.Color.bar.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 16
                }
            }
        }
    }
    Timer { id: closeGuard; interval: 650 }

    Variants {
        model: Quickshell.screens
        delegate: Item {
            id: output
            required property var modelData
            PanelWindow {
                screen: output.modelData
                visible: output.modelData.name === "DSI-1"
                anchors { bottom: true; left: true; right: true }
                implicitHeight: root.stripHeight
                exclusiveZone: root.stripHeight
                WlrLayershell.namespace: "trona-touch-nav"
                WlrLayershell.layer: WlrLayer.Top
                WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
                color: Commons.Color.bar.background
                Actions { anchors.centerIn: parent; width: Math.min(parent.width - 8, 492) }
            }
            // The grid covers the display. Keep its navigation row tappable
            // over the scrim without reserving the same space a second time.
            PanelWindow {
                screen: output.modelData
                visible: root.gridOpen && output.modelData.name === "DSI-1"
                anchors { bottom: true; left: true; right: true }
                implicitHeight: root.stripHeight
                exclusionMode: ExclusionMode.Ignore
                WlrLayershell.namespace: "trona-grid-nav"
                WlrLayershell.layer: WlrLayer.Overlay
                WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
                color: Commons.Color.bar.background
                Actions { anchors.centerIn: parent; width: Math.min(parent.width - 8, 492) }
            }
        }
    }
}
