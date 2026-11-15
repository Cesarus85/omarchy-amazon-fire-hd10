# Architecture and hardware adaptations

## Boot and graphics

The existing Amazon boot chain and an already installed compatible recovery are
preserved. LK loads an Android-header-v1 boot image containing an ARM64
`Image.gz` followed by the Trona DTB, plus a small XZ initramfs. The verified
kernel load address is `0x40200000`; a generic ARM board boot recipe is wrong.

The DT keeps the original LK-required board version/backlight properties.
`trona-bootfb` reads LK's `atag,videolfb`. Reserved framebuffer RAM is marked
no-map before mapping it for SimpleDRM. The DRM adaptations allow the display
path to cooperate with GPU rendering; Panfrost drives the Mali-G72. The current
desktop still depends on the bootloader's display initialization. A working
Hyprland desktop should not be confused with a finished native DSI driver.

The initramfs verifies the expected root UUID and partition size, marks protected
partitions read-only, checks ext4, then switches to systemd on native p18.
The public template removes the original migration fallback to an Android
userdata loop image and does not open an unauthenticated emergency shell.

## Power and charging

MT6370 Type-C/TCPCI changes establish the connection/role needed for charging
while Linux runs. A board-specific battery driver supplies Linux power-supply
properties. A cpufreq policy and CPU-hotplug completion cooperate with the
vendor firmware. The display domain remains protected; unused camera/codec
domains can power down through the kernel's dependency-aware power domains.

The standby controller requires a confirmed lock before blanking. With external
power it leaves charging active. Off cable, it coordinates radio shutdown,
CPU hotplug and s2idle, then restores them after wake. Firmware CPU_SUSPEND/SPM
experiments were removed from the published core patch.

## Connectivity

The radio stack adapts the Amazon/MediaTek WMT, BTIF and gen3 WLAN sources to
Linux 6.18. A native HCI module exposes the Bluetooth transport to BlueZ.
Stock MT6631 firmware lacks some reported LE capability bits; the board-specific
post-init restores the required legacy event mask.

A WLAN EAPOL statistics path could dereference missing optional RX metadata
and hang networking. The adapted source checks those pointers; a regression
test compiles and executes the real function with missing metadata.

Calibration and identity come from the device's own IDME/vendor state. Do not
replace them with another tablet's MAC addresses, NVRAM or partition contents.

## Audio and cameras

The audio machine driver routes MT8183 DL1/ADDA through MT6358 and controls the
speaker amplifier. The ALSA UCM profile enables the analog stages required for
audible output and selects working capture gain.

Cameras use the SENINF/CAMSV raw capture pipeline and libcamera's simple pipeline
with a software ISP. The HI556 rear sensor needs the verified register tables
and two-lane CSI0A offset calibration. The OV02B10 front sensor uses a separate
path. The bundled camera app maps sensor orientation to current monitor
transform and saves photos in the user's pictures directory.

## Desktop and lock

`trona-rotate` changes display and touch together, verifies the bar/keyboard
geometry and persists orientation. The IIO controller debounces poses, rejects
motion/flat poses, and postpones rotation while a finger is down.

Hyprland 0.57.2's soft transform update omits the mode-change notification used
by existing session-lock surfaces. Reapplying the same native mode explicitly,
then restoring `preferred`, gives the lock its new dimensions without unlocking
or restarting it. All four orientations were checked with the lock continuously
secure.

The on-desktop wvkbd keyboard supports visibility toggling and Super/Alt modifiers.
The password keyboard is a different QML component *inside* the real Wayland
session-lock surface. At boot, an empty compositor and touch greeter are started;
normal Omarchy configuration and startup callbacks are activated after PAM succeeds.
The password is the Linux account password, not a value embedded in QML.

## Packaging

`trona-platform` owns runtime adaptations, modules, firmware supplied privately
by the builder, and exact dependency constraints for sensitive desktop/camera
components. ALPM hooks verify the board/boot/module baseline before transactions
and reapply checked patches afterward. Per-device UUID and image hashes are
rendered from a private profile. See [updates.md](updates.md).
