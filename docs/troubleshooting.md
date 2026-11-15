# Troubleshooting

| Symptom | First checks |
|---|---|
| Amazon logo/boot loop | Correct root layout and image pair; header load address; appended Trona DTB; compact initramfs size; recovery reachable |
| Linux USB exists but screen dark | LK board version/backlight DT properties, framebuffer no-map reservation; do not infer a dead kernel from a dark panel |
| USB disappears when changing mode | Allow enumeration to settle, reconnect cable, check Mac accessory authorization; inspect the actual mode before reflashing |
| Wi-Fi shows no networks | Required firmware, WMT/BTIF and WLAN units, module ABI, rfkill and NetworkManager; no USB proxy needed |
| Wi-Fi hangs at handshake | Verify the EAPOL optional-metadata fix and matching radio modules |
| Bluetooth “No adapter” | Native trona-bluetooth transport before bluetooth.service; firmware startup; actual hci device |
| Keyboard does not reconnect | Correct Easy-Switch channel; wake the keyboard briefly, do not repeatedly erase bonding |
| Speaker icon active but silent | Trona UCM and analog stages/amplifier path, not just PipeWire mute |
| Camera black or wrong sensor | Exact sensor revision, DT graph, I2C GPIO wiring, CSI0A calibration, libcamera simple/software-ISP config |
| Camera upside-down | Use the maintained camera app and display transform; avoid stacked manual corrections |
| Lock squeezed to left after rotation | Updated trona-rotate must trigger lock reconfigure; secure surface should report 1536×960 landscape / 960×1536 portrait at scale 1.25 |
| Wi-Fi password field covered | External wvkbd overlay plus patched Omarchy keyboard-panel layer |
| Bar moves accidentally | Managed bar position/top lock and current Omarchy patch |
| Oversized app dialog | Tablet scale control and window-fit service; app-specific minimum sizes can still be limiting |
| SD unmountable | Determine filesystem first. Android-adopted storage is encrypted; exFAT formatting erases it |
| Update blocked | Read trona-update-check output and exact dependency constraints; do not bypass |

For diagnosis start with `scripts/inspect-device.sh`. It is read-only, but review
even its output before posting. Add the package versions, exact model and steps
to reproduce. Redact identifiers from any additional logs yourself.

On the configured tablet the native standby controller owns Power and screen
blanking. Adding an independent hypridle/DPMS policy can conflict with it.
Firmware deep-suspend experiments are not part of the supported path.
