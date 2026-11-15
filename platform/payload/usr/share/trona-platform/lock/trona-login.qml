import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import Quickshell.Services.Pam

ShellRoot {
  id: login
  property string password: ""
  property string pending: ""
  property string error: ""
  property bool checking: false
  property bool accepted: false
  function submit() {
    if (checking || accepted || password.length === 0) return
    pending = password; password = ""; error = ""; checking = true
    if (!pam.start()) { pending = ""; checking = false; error = "Anmeldung nicht verfügbar" }
  }
  function respond() { if (pam.active && pam.responseRequired) pam.respond(pending) }
  PamContext {
    id: pam
    config: "omarchy-lock-password"
    user: "trona"
    onResponseRequiredChanged: login.respond()
    onPamMessage: login.respond()
    onCompleted: function(result) {
      login.pending = ""; login.checking = false
      if (result === PamResult.Success) { login.accepted = true; activate.running = true }
      else login.error = "Passwort nicht korrekt – erneut versuchen"
    }
    onError: { login.pending = ""; login.checking = false; login.error = "Anmeldung fehlgeschlagen" }
  }
  Process {
    id: ready
    command: ["uwsm", "finalize"]
  }
  Process {
    id: activate
    command: ["/usr/local/libexec/trona-session-activate"]
    onExited: function(code, status) {
      if (code === 0) { lock.locked = false; Qt.quit() }
      else { login.accepted = false; login.error = "Desktopstart fehlgeschlagen. Wartungszugang bleibt verfügbar." }
    }
  }
  WlSessionLock {
    id: lock
    locked: true
    onSecureStateChanged: { if (secure) ready.running = true }
    WlSessionLockSurface {
      color: "#10141d"
      Item {
        anchors.fill: parent
        Text {
          anchors.horizontalCenter: parent.horizontalCenter
          y: Math.max(28, (parent.height - keyboard.height) * 0.22)
          text: "OMARCHY · TABLET ENTSPERREN"
          color: "#d8e2f3"; font.pixelSize: 25
        }
        Rectangle {
          id: field
          anchors.horizontalCenter: parent.horizontalCenter
          y: Math.max(85, (parent.height - keyboard.height) * 0.48)
          width: Math.min(parent.width - 36, 520); height: 64
          radius: 9; color: "#242d3b"; border.color: "#829fd4"; border.width: 2
          TextInput {
            id: input
            anchors.fill: parent; anchors.margins: 10
            echoMode: TextInput.Password; passwordMaskDelay: 0
            color: "white"; font.pixelSize: 28
            horizontalAlignment: TextInput.AlignHCenter; verticalAlignment: TextInput.AlignVCenter
            focus: true; enabled: !login.checking && !login.accepted
            text: login.password
            onTextEdited: login.password = text
            onAccepted: login.submit()
            Keys.onEscapePressed: login.password = ""
          }
        }
        Text {
          anchors.top: field.bottom; anchors.topMargin: 12; anchors.horizontalCenter: field.horizontalCenter
          text: login.checking ? "Wird geprüft …" : login.error || "Benutzerpasswort eingeben"
          color: login.error ? "#ffb7b7" : "#becde2"; font.pixelSize: 18
          width: parent.width - 24; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
        }
        TouchKeyboard {
          id: keyboard
          anchors.bottom: parent.bottom; anchors.bottomMargin: 16; anchors.horizontalCenter: parent.horizontalCenter
          width: Math.min(parent.width - 24, 1080)
          height: Math.min(350, Math.max(260, parent.height * 0.37))
          inputEnabled: !login.checking && !login.accepted
          onInsertText: function(value) { login.password += value; login.error = ""; input.forceActiveFocus() }
          onBackspace: { login.password = login.password.slice(0, -1); input.forceActiveFocus() }
          onClear: { login.password = ""; login.error = ""; input.forceActiveFocus() }
          onSubmit: login.submit()
        }
      }
    }
  }
}
