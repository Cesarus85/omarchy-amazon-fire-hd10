# Hardware and validation status

Reference: Fire HD 10, 11th generation / 2021, **Trona**, MT8183, 32-GB eMMC.
Other Fire generations, Fire HD 8, Fire Max 11, 64-GB layouts and alternate camera
modules are not verified. A shared SoC alone is not sufficient compatibility.

Status recorded on 2026-11-15:

| Component | Evidence | Remaining limitation |
|---|---|---|
| Boot / storage | Repeated cold/warm starts, ext4 checks, read/write/fsync tests | No independent second-device installation |
| CPU | Eight cores, 793–1989 MHz CPU/CCI DVFS; [load/thermal tests](cpu-dvfs.md) | Short qualification, not a multi-day stress campaign |
| Panel | Bootloader scanout retained, framebuffer reservation and DRM fixes | Not a complete native DSI panel driver; depends on LK initialization |
| GPU | Panfrost GLES tests, Weston and Hyprland; Mesa update survived | No claim of hardware video decoding or Vulkan coverage |
| Touch | FT8201/EDT driver, physical typing in both orientations | Multi-touch gestures vary by app |
| Rotation | STK8BA53 IIO, all four orientations, keyboard/bar/camera follow | Flat/diagonal/accelerating poses intentionally keep orientation |
| Lock | PAM, wrong-password rejection, touch unlock, reboot and standby | Recovery remains accessible; storage not encrypted |
| Wi-Fi | Firmware init, scan, WPA connection, reboot/radio-cycle recovery | Long coexistence tests and EMI region-25 hardening remain |
| Bluetooth | LE discovery, PIN-authenticated MX Keys Mini bonding, reconnect | Bluetooth audio and many-device testing not done |
| Audio | MT8183 ADDA / MT6358 and speaker amplifier; both test frequencies audible | Physical stereo separation not conclusively established; jack untested |
| Microphone | Analog capture and increased gain; voice understood | Further app/quality testing deferred |
| Front camera | OV02B10, preview, color, rotation and photos | Alternate sensor revisions unsupported |
| Rear camera | HI556, CSI0A calibration, preview, rotation and photos | Not a tuned vendor ISP; libcamera software ISP |
| Volume keys | Linux key events and actual YouTube volume changes | — |
| Battery | State-of-charge visible, desktop usable unplugged | Calibration and long-run lifetime not characterized |
| Charging | Running desktop charges from Mac USB-C connection | Other USB-C supplies need their own test; USB-A deferred |
| Suspend | Controlled s2idle with radios/cores managed, Power wake and reconnect | Firmware deep suspend intentionally excluded |
| Poweroff | Stays off with USB attached and detached | No exhaustive charger matrix |
| SD | MMC reader, exFAT and desktop mounting | Android-adopted encrypted cards need their original key or erasure |
| Browser/apps | Working general browsing/media; touch app-grid scrolling | Some app dialogs require scale/window adjustments |

The source release rebuilds integration that evolved on one device. Software
build success does not replace physical acceptance testing.

## Acceptance sequence for another device

Keep recovery available. First test boot, storage and display. Then verify touch
and rotation before relying on the password screen. Pair a Bluetooth keyboard
with a fresh PIN. Check native Wi-Fi without a USB proxy. Check recording/playback,
both cameras and saving a photo. Test Power-off with USB in and out. Finally,
repeat unplugged standby/wake, touch unlock, Wi-Fi and keyboard use over several
cycles. Measure battery drain separately from a cable-powered screen-off test.

For measured CPU response, rendering limitations and an optional reversible
governor change, see [Performance](performance.md).
