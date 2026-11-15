# Source-release validation — 2026-11-15

## Reference-device evidence

The original Trona has booted the native system repeatedly. The owner physically
confirmed display/touch, keyboard, rotation and rotation lock, Wi-Fi, bonded
Bluetooth keyboard reconnection, speakers and volume keys, voice recording,
both cameras and photo saving, SD mounting, shutdown, standby/wake and touch
password rejection/unlock. The final lock layout correction was confirmed in
landscape as well.

An actual update cycle upgraded 27 Arch/Omarchy packages, reinstalled Omarchy to
test patch restoration, completed `omarchy update`, and rebooted successfully.
This is evidence for the pinned versions, not future rolling releases.

## Export/build evidence

The publication was assembled from allowlisted sources rather than a device
filesystem dump. A fresh Linux 6.18.56 source tree was patched and compiled using
the Ubuntu 24.04 AArch64 cross-toolchain environment. The first build exposed a
missing charging-policy header; that header is now included in the final patch.
The final patch was also checked against the resulting source tree.

**Both core outputs match the reference tablet's working artifacts byte for byte:**

| Artifact | SHA256 |
|---|---|
| Image.gz | f220ae728b0c058b776588cde0bbe69857cda39cc070a824bdd5ef76f38ebc36 |
| trona.dtb | be03e65a47ca939fc34b3090c0b0c21b9dc918150942ca099b743072dbcf6817 |

All **28 required external modules** were built and collected from the exported
sources. Kernel modpost completed without unresolved-symbol errors. Legacy
vendor code still emits warnings, including undefined optional configuration
macros and a missing MODULE_DESCRIPTION; this is not a warning-free upstream
kernel port.

Additional completed checks:

- Python/shell syntax, JSON, local documentation links, symlink destinations and
  exclusion of binary/private build artifacts.
- Per-device profile rendering and ten malformed/unresolved-input cases.
- Rotation sensor-axis, motion rejection, finger-contact, debounce and cooldown
  regression test.
- The actual WLAN EAPOL function executed under ASan/UBSan on Linux with absent
  optional metadata.
- Boot-image header/load-address checks and XZ/newc round-trip using explicit
  fixture inputs; checked UUID substitution and absence of the old open
  emergency shell. The generated fixture image was **not flashed**.
- Package staging with the reference modules/keyboard and privately supplied
  firmware; verified all 123 payload-manifest entries, symlink and sudoers mode.
- wvkbd patch application against the original v0.16 source archive.
- ShellCheck on the new shell build/inspection scripts.
- Gitleaks 8.30.1 source scan. The only exclusions are two exact upstream WLAN
  enum identifiers, documented in .gitleaks.toml; no remaining findings.

The original QML keyboard fixture passed six tests on the configured tablet.
The public CI runs portable source/profile/rotation/WLAN checks; it does not
pretend to exercise a real tablet.

## What this does not establish

The publication process did not flash or change the owner's running tablet.
The newly parameterized boot template, private-profile packaging and the
factory-to-native installation sequence have **not been validated end to end
on a second tablet**. There is no ready-to-flash public system image.

A bit-identical core kernel/DTB does not guarantee that a differently provisioned
root filesystem, changed firmware, sensor revision, newly compiled module set
or future package versions will behave identically. Physical acceptance and a
working recovery route remain necessary.
