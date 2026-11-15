# Installation and recovery

For the experimental guided workflow, start with [installer.md](installer.md).
This document explains the underlying steps and manual recovery boundaries.

## Reproduction boundaries

The reference tablet was brought up incrementally, initially with Linux in an
ext4 loop image inside Android userdata. It was later migrated to native p18,
then received camera, rotation, update and lock work. The published runtime
and kernel sources represent the **final native-p18 system**, not every
intermediate experiment.

There is **no hardware-verified factory-to-Omarchy unattended installer**, no
hosted Arch root image and no tested second-device migration. The guided
installer automates provisioning, source builds and confirmed recovery writes
for one strict stock layout. Unlocking remains a separate prerequisite.
Do not treat a compiled image or passing source tests as proof of a successful
installation on another unit.

## 1. Identify the tablet and establish recovery

Confirm **Fire HD 10 (2021), 11th generation, Trona / MT8183**. Do not use this
port on a different Fire generation, even if it has a similar screen.

The reference device already had a compatible hacked bootloader path and
**TWRP 3.7.0_9-0**. The upstream
[amonet-koboreru project](https://github.com/R0rt1z2/amonet-koboreru) documents
supported devices and bootloader prerequisites. Follow its current instructions
and constraints directly; this repository neither bundles nor modifies that
exploit or provides an alternate unlock procedure. It does not claim every
Fire OS/bootloader revision can be unlocked.

Before altering partitions, establish a working recovery and a way to reach it
with Power/Volume Up. Save your own partition table, original boot/recovery,
firmware and calibration. Keep them outside this repository. The reference
owner chose not to retain backups later; that personal choice is not an
installation recommendation.

Read-only inventory from a Linux shell:

```sh
scripts/inspect-device.sh
lsblk -o NAME,SIZE,FSTYPE,PARTLABEL,MOUNTPOINTS /dev/mmcblk0
sfdisk --json /dev/mmcblk0 > /path/on/host/device-partitions.json
sfdisk --dump /dev/mmcblk0 > /path/on/host/device-partitions.sfdisk
```

In TWRP, block paths are commonly under `/dev/block/`; use verified
`by-name` mappings. Never substitute a guessed partition number into a flash
command.

## 2. Preserve firmware and identity

Before deleting Android, extract your stock radio firmware listed in
[../firmware/required.json](../firmware/required.json). Locate the files in
your mounted stock system/vendor tree, copy only those named files, then hash
them. File paths can vary by stock release. Do not copy another tablet's
IDME/NVRAM/calibration or replace the preserved firmware partitions.

The radio drivers consume the original per-device state supplied by LK.
The firmware hashes describe the reference version. The repository contains
neither firmware blobs nor private calibration.

## 3. Plan storage independently of flashing

On the tested 32-GB device:

| Area | Final use |
|---|---|
| GPT entries 1–15 | Existing boot-chain, firmware and calibration-related partitions preserved |
| p16, 32 MiB | Boot image |
| p17 | Existing recovery preserved |
| Old entries 18–21 | Merged into native ext4 `linux-root` at p18 |
| p18 start / size | Sector 409600 / 60661727 sectors, 512-byte sectors |
| p18 bytes | 31,058,804,224 (about 28.93 GiB) |

**The final migration destroys Android system/vendor/cache/userdata.** Do not
apply these numbers to a 64-GB unit or a different existing table. Preserve the
original first 17 GPT entries and their GUIDs, disk GUID, boot hardware areas,
and backup-GPT integrity. The initial loop-image installation has a different
boot/root contract; this repository's boot template cannot start it.

Use recovery with all affected filesystems unmounted to perform an independently
reviewed partition migration. A safe migration requires offline filesystem
checks, verified copies if preserving existing Linux data, and comparison of
protected partitions before/after. The guided installer validates both GPTs and
the exact stock geometry; its checks must not be bypassed for other layouts.

For a *new empty* Linux root, create ext4 only after identifying and authorizing
the final p18 partition. Record its actual UUID and exact byte size in the
private profile. Do not reuse the example UUID. Mount it at an explicit staging
root such as `/mnt/trona-root`.

## 4. Prepare Arch Linux ARM and Omarchy userspace

Use the official
[Generic AArch64 root filesystem](https://archlinuxarm.org/platforms/armv8/generic)
and verification instructions. Extract it as root while preserving owners,
permissions and extended attributes. A generic Arch ARM kernel/bootloader is
not the Trona kernel: remove/avoid those packages and do not run a generic boot
installer against the tablet.

Provision in an AArch64 chroot or equivalent ARM build environment, with
/dev, /proc and /sys mounted appropriately and working package-signature
verification. Initialize the Arch Linux ARM keyring. Set locale/timezone,
hostname and fstab for **your** root UUID. The root entry should use ext4 and
`noatime,errors=remount-ro`.

The runtime assumes user/group **trona, UID/GID 1000**, home `/home/trona`.
If the rootfs provides the default `alarm` account at UID 1000, rename and move
that account rather than creating a second user with a conflicting UID. Set a
new account password interactively; lock the root password and remove all
distribution default passwords. Required groups include seat, video, input,
render, audio and wheel. Do not enable a desktop autologin shell.

Install the dependencies in `platform/PKGBUILD`, plus the normal Omarchy
desktop/session packages and tools it invokes. Useful baseline tools include
`alsa-utils bluez-utils blueman networkmanager openssh sudo python-pyserial
gtk3 python-gobject libcamera-tools v4l-utils grim qt6-declarative`, wvkbd build
dependencies and a browser. Exact repository package names can change.

The reference uses the **Omarchy stable AArch64 repository** and
`omarchy=4.5.0-3`, `omarchy-settings=4.5.0-3`.
The recorded repository entry is `Server = https://pkgs.omarchy.org/stable/$arch`
under `[omarchy]`. See the upstream
[ARM discussion](https://github.com/omacom/omarchy/discussions/15702) and current
repository/keyring documentation. Preserve signature checking. Do not run the
generic Omarchy disk installer or bootloader migration on this device.
A future Omarchy/Hyprland version is not interchangeable with the pinned
snapshot; if the recorded versions are unavailable, qualify a new set first.

Copy the matching Omarchy default user configuration into the new user's home.
Apply the small Trona-specific user configuration in
[desktop-setup.md](desktop-setup.md). The package's repair hook expects the
Omarchy configuration and shell.json to already exist.

## 5. Build and stage

Follow [build.md](build.md). Build the root-specific boot image, copy its padded
hash into the private profile, and stage a matching platform package with your
own firmware. Do not share this rendered package casually: it embeds your
root/boot/recovery baseline and locally supplied firmware.

The normal package-install/update hooks assume an already booted native target.
They cannot validate the target board, kernel or boot hashes from a host chroot.
The installer bootstraps a **fresh offline root** by registering the platform
package without live scriptlets, copying its payload, and enabling a required
first-boot integrity/integration service. Do not run this bootstrap against an
existing live system or disable its guards to make installation succeed.
Post-install service operations also require systemd and the standard account.
A bootstrap integration pass is required before enabling normal updates:
`sudo trona-update-check repair`, then `sudo trona-update-check check`.

Enable the intended units after configuration: seatd, trona-desktop,
trona-boot-guard, trona-radio-platform, trona-wmt, trona-wifi,
trona-bluetooth, bluetooth, NetworkManager, trona-battery,
trona-cpufreq-policy, trona-standby and trona-usb-network. Enable the normal
serial getty, **without autologin**. The four trona user units (rotation,
keyboard, window fit, SD mount) belong to graphical-session.target.
Inspect each unit's Install section/dependencies instead of enabling every
file under systemd blindly.

Install your own SSH public key only if cable maintenance is desired; do not
enable password SSH. See [security.md](security.md).

## 6. First boot

Use working recovery to flash **only the verified boot partition**, after
rootfs/modules/firmware/profile agree. Compare the written, padded partition
hash with the generated manifest and confirm recovery is unchanged. Keep
recovery accessible and do not overwrite preloader/LK/TEE/IDME partitions.

The guided installer writes boot last and verifies the complete padded block
contents. Separately, `trona-install-native-boot` is a guarded **later native-system boot updater**,
not a stock-Android installer; it requires the expected native root and an
explicit current-image baseline in its manifest.

First validate boot logs, root mount and a usable recovery route; then modules,
GPU/display/touch, keyboard and login. Check firmware/NetworkManager/BlueZ
before testing radio reconnection. Run `sudo trona-update-check check` and
`pacman -Qkk trona-platform`. Complete [the acceptance sequence](hardware.md)
before relying on suspend or the lock.

## Recovery notes

If the screen is black or stuck at Amazon, do not repeatedly flash unrelated
images. Record what is visible, power down, and use the already established
Volume Up + Power recovery route. USB enumeration sometimes needs a cable
reconnect after switching modes. Do not format anything in TWRP while diagnosing.

An old loop-root boot image cannot boot the merged-p18 system. Recovery's old
Android mount names may also be wrong after migration. Identify partitions
by current table, size and filesystem before mounting or writing.
