# Source and device validation

## CPU DVFS platform — 2026-11-16

The arch14 kernel, complete 28-module set and device tree were rebuilt, installed
through the guarded native-boot/package paths and rebooted on the reference
tablet. The owner also confirmed Power-key wake, touch unlock, Wi-Fi and the
paired keyboard after USB-disconnected s2idle; post-resume integrity passed. See [CPU DVFS](cpu-dvfs.md) and the [measurement summary](evidence/cpu-dvfs-2026-11-16.json).

| Current artifact | SHA256 |
|---|---|
| Image.gz | 9e1b01c52e8c9bf1a03f583449f6aaec45c9bc82bb40eee61083213aef59eaad |
| trona.dtb | b49354adef09ad046b8769755add3fbd0c123c1bea7c979854e9d9b34fe1c899 |

The current builds used the existing source build tree. A second independent
clean-build hash reproduction has not been performed for arch14. The older
bit-identical clean-build evidence below applies specifically to arch12.

Governor-policy tests cover the exact full table and reject extra OPPs. Update
guard tests cover the one permitted arch12-to-arch14 package migration and
rejection of other kernel trees. The 16 Linux installer tests, including the
independent sfdisk check, pass; this still is not a second-tablet install test.

## Original source release — 2026-11-15

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
- Gitleaks 8.30.1 source scan. Exact exclusions cover two upstream WLAN enum
  identifiers and the public Omarchy signing-key fingerprint, documented in
  .gitleaks.toml; they are not credentials.

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

## Experimental installer validation (0.2)

The installer was developed without accessing or modifying the reference tablet.
Validation used generated device identity/GPT fixtures, a fresh official Arch
ARM root in an ARM64 Docker volume, and privately supplied matching firmware.

- 16 automated installer tests pass on Linux, including independent `sfdisk`
  validation of a generated sparse 32-GB disk. They cover GPT CRC corruption,
  valid-CRC wrong layouts, extra partitions, capacity mismatches, ambiguous ADB
  selection, cancellation, protected-area/root readback failures, disconnects,
  boot-last ordering and refusal to erase completed installs through resume.
- Clean userspace provisioning installed signature-checked packages and matched
  all seven qualified platform package versions. Repository-qualified names
  select Arch's Hyprland and Omarchy's Quickshell deliberately.
- The patched keyboard compiled in the fresh ARM64 root. The previously
  reproduced kernel/DTB and all 28 modules were reused for integration testing;
  the installer invokes those same source-build scripts for a normal build.
- Offline platform packaging/bootstrap passed `pacman -Qkk trona-platform`:
  **177 files, 0 altered files**. Sudoers syntax passed.
- The generated 8-GiB ext4 filesystem passed all five `e2fsck -fn` phases.
- The complete private fixture bundle was assembled inside a Linux Docker
  volume, exported through the installer's actual copy path, then verified on
  the Mac: compressed/decompressed SHA256, all 8,589,934,592 raw bytes, ext4 UUID
  and size, boot/profile agreement and the protected GPT plan passed. Source/
  input streaming into the volume was also exercised. Large writable host bind
  mounts were removed after macOS shared-folder I/O errors during testing.
- First-boot integration helpers passed in the offline root, including package
  versions, platform file hashes, module vermagic, user configuration and the
  checked touch/lock/menu patches. This test supplied a synthetic board marker
  and selected the target kernel for `modinfo`; it did **not** pretend to test
  hardware, boot-partition hashes or a running target kernel.

The build was exercised and corrected in stages. This is not yet an unattended
factory-to-desktop acceptance run on physical hardware. USB recovery writes,
recovery-specific command behavior, first boot and physical touch/radio/standby
acceptance still need a second, already unlocked 32-GB Trona. The installer
therefore remains a prerelease with no universal binary image.
