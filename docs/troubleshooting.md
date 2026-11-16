# Troubleshooting

| Symptom | First checks |
|---|---|
| Amazon logo/boot loop | Correct root layout and image pair; header load address; appended Trona DTB; compact initramfs size; recovery reachable |
| Linux USB exists but screen dark | LK board version/backlight DT properties, framebuffer no-map reservation; do not infer a dead kernel from a dark panel |
| USB disappears when changing mode | Allow enumeration to settle, reconnect cable, check Mac accessory authorization; inspect the actual mode before reflashing |
| Wi-Fi shows no networks | Required firmware, WMT/BTIF and WLAN units, module ABI, rfkill and NetworkManager; no USB proxy needed |
| Wi-Fi hangs at handshake | Verify the EAPOL optional-metadata fix and matching radio modules |
| Browser/webapps show ERR_EMPTY_RESPONSE despite working Wi-Fi | Check for the obsolete USB-only Chrome proxy policy; see below |
| Bluetooth “No adapter” | Native trona-bluetooth transport before bluetooth.service; firmware startup; actual hci device |
| Keyboard does not reconnect | Correct Easy-Switch channel; wake the keyboard briefly, do not repeatedly erase bonding |
| Speaker icon active but silent | Trona UCM and analog stages/amplifier path, not just PipeWire mute |
| Camera black or wrong sensor | Exact sensor revision, DT graph, I2C GPIO wiring, CSI0A calibration, libcamera simple/software-ISP config |
| Camera upside-down | Use the maintained camera app and display transform; avoid stacked manual corrections |
| Lock squeezed to left after rotation | Updated trona-rotate must trigger lock reconfigure; secure surface should report 1536×960 landscape / 960×1536 portrait at scale 1.25 |
| Wi-Fi password field covered | External wvkbd overlay plus patched Omarchy keyboard-panel layer |
| Bar moves accidentally | Managed bar position/top lock and current Omarchy patch |
| Oversized app dialog | Tablet scale control and window-fit service; app-specific minimum sizes can still be limiting |
| Apps open as small floating windows | Platform 2026.11.16-4 fills regular floating-workspace apps; ordinary tiled apps open maximized. Dialogs and manually floated windows retain their sizing |
| OSK emojis open a context menu or repeat the first emoji | Rebuild wvkbd with both included patches; the IME keymap workaround preserves Fcitx. See [tablet window and emoji fixes](tablet-windows-and-emojis.md) |
| SD unmountable | Determine filesystem first. Android-adopted storage is encrypted; exFAT formatting erases it |
| Update blocked | Read trona-update-check output and exact dependency constraints; do not bypass |

For diagnosis start with `scripts/inspect-device.sh`. It is read-only, but review
even its output before posting. Add the package versions, exact model and steps
to reproduce. Redact identifiers from any additional logs yourself.

On the configured tablet the native standby controller owns Power and screen
blanking. Adding an independent hypridle/DPMS policy can conflict with it.
Firmware deep-suspend experiments are not part of the supported path.

## Browser fails while native WLAN works

Early experimental installations may retain
`/etc/opt/chrome/policies/managed/trona-usb-proxy.json`, forcing Chrome and
webapps through `127.0.0.1:18080`. That USB rescue proxy can fail even when
NetworkManager, DNS and direct HTTPS are working. Check `chrome://policy` for
`ProxyMode` and `ProxyServer`, and compare direct access on the tablet:

```sh
curl --noproxy '*' --connect-timeout 5 --max-time 15 -I https://example.com
```

From a checkout of this repository **on the tablet**, remove only the known
obsolete policy with:

```sh
sudo python3 tools/trona-remove-legacy-proxy.py
```

The tool checks the Trona model and root ownership and refuses symlinks,
malformed JSON or any policy differing from the exact historical settings.
It does not reset browser profiles or change WLAN, DNS, or the USB maintenance
connection. Reload the page; if the policy remains cached, fully exit the browser
through its menu and reopen it. Custom proxy policies require manual review.
The public installer does not install this legacy policy.

On the reference tablet (2026-11-16), the old proxy aborted HTTPS CONNECT while
direct IPv4 and IPv6 HTTPS returned HTTP 200. After removing the policy, the owner
confirmed that both websites and webapps loaded again; the platform integrity
check passed. No kernel or platform package replacement was needed.
