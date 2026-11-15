# Guided installer (experimental)

The installer replaces the manual root provisioning, per-device configuration,
image construction and recovery writes with a guided workflow. **It is an
experimental clean installer, not a verified consumer release.** Its new
factory-to-native sequence has not yet been boot-tested on a second tablet.
It does not upgrade an existing Linux installation.

## Supported starting point

- **Amazon Fire HD 10 (2021), Trona, 32 GB**, exact original 21-partition layout.
  Other generations, the 64-GB model, modified partitions and already-native
  Linux layouts are refused.
- An already compatible/unlocked boot chain and **TWRP 3.7.0_9-0**, with working
  root ADB. Unlocking is separate: follow the applicable
  [upstream prerequisites](https://github.com/R0rt1z2/amonet-koboreru).
  The wizard cannot make an unsupported Fire OS revision unlockable.
- At least 50% battery, a reliable USB data cable, and only one Android device
  attached. Confirm that you can return to recovery before installing.
- macOS or Linux with Python 3.10+, Git, Android SDK Platform Tools (`adb`),
  and a Docker engine capable of running **linux/arm64** containers.
  Apple Silicon runs ARM64 natively. Other hosts need working ARM64 emulation;
  native Windows/WSL USB forwarding is not covered by this release.
- At least **25 GiB free on the host and in Docker's storage**, plus internet.
  A full source build takes substantially longer than downloading a ready image.

**Installation erases Android, apps and all internal user data.** Export them
before starting. An Android-adopted/encrypted SD card may also become unreadable
when Android's internal encryption keys are lost, although the installer never
writes the SD card. Export those files through Android first.

## Simple start

Download/clone a reviewed release of this repository, then:

```sh
git clone https://github.com/Cesarus85/omarchy-amazon-fire-hd10.git
cd omarchy-amazon-fire-hd10
./install.sh
```

Choose **6** for the complete guided sequence, or run
`./install.sh wizard --keyboard de`. The final erasure confirmation is still
mandatory. The menu also offers the five steps separately; its default is the
read-only requirements check.
Each step also has a command for reproducible use:

```sh
./install.sh doctor
./install.sh prepare
./install.sh build --keyboard de
./install.sh verify
./install.sh install
```

Do not run the host wizard with `sudo`. Host `adb` must already have permission
to access USB. Docker performs privileged filesystem preparation **inside a
disposable container**; the tablet is never passed into the container. Inputs
are streamed into a Linux Docker volume and finished files are copied back;
large writable images do not live in a macOS shared-folder mount.

### 1. Check and prepare

Start the tablet in TWRP and connect USB. Leave System and Vendor unmounted in
TWRP's Mount screen so the installer can mount its own read-only views.
`prepare` verifies the model, recovery,
capacity, battery, both GPT CRCs, and every partition's name/type/bounds.
It creates a private `local/installer/` workspace with:

- the original first 200 MiB (boot chain, calibration, boot and recovery), plus
  both original GPT copies;
- original hashes, the device identity and a new unique root filesystem UUID;
- the four required firmware files from **this tablet's** stock system/vendor,
  mounted read-only with journal replay disabled;
- a proposed GPT that preserves entries 1–17 byte for byte and merges 18–21
  into a 28.93-GiB Linux partition.

This protected-area capture is **not a backup of Android or user data**.
It is private and must never be uploaded to an issue or public release.
Preparation writes local files and mounts stock filesystems read-only; it does
not change tablet storage. An existing workspace is never overwritten.

### 2. Build

`build` asks for a new tablet password twice. It builds a fresh Arch ARM root,
checks package signatures and the qualified versions, compiles the kernel and
drivers, builds the patched keyboard, and constructs the device-specific boot
image and native ext4 root. It does **not** access the tablet.

For optional cable maintenance, supply **your own public key**:

```sh
./install.sh build --keyboard de --ssh-key /path/to/my-public-key.pub
```

No key is generated or copied from the reference tablet. Without this option,
the normal password-authenticated local login remains available; root/password
SSH is not enabled. The new desktop account is `trona` (UID/GID 1000). Default
Arch ARM passwords are removed. Timezone starts at UTC and can be changed later.

The source archive and keyring have pinned SHA256 values in
[sources.json](../installer/sources.json). If a rolling upstream download has
changed or a required package disappeared, the build stops **before flashing**.
Do not remove version checks or disable signature verification to continue.
A maintainer must qualify the replacement inputs. If you already have the
matching official archive, it may be copied to `local/installer/arch.tar.gz`;
the same checksum check still applies.

The resulting bundle contains private firmware, a password hash and device
identifiers. **It is for this tablet only; do not publish it.** There is no
hosted universal root image. Build errors leave the named Docker volume for
diagnosis; its name and the build-container name are in `build-state.json`.
Inspect failures with `docker logs <recorded-container-name>`. Remove that
particular container and volume when no longer needed; the wizard prints their
names. Do not run a blanket `docker system prune`.

### 3. Verify and install

`verify` checks the compressed and decompressed root image, boot image, profile,
tools and planned GPT, without opening ADB. `install` repeats this check and
requires the exact phrase `ERASE <connected-device-serial>`.

Only after confirmation does it freeze the recovery UI, unmount internal
filesystems and check for active device-mapper/loop use. It writes the backup
GPT first, then the primary GPT, verifies both, writes and reads back Linux,
checks/resizes ext4, and checks the protected partitions. **Boot is written
last**, followed by full boot and recovery hash verification. It does not
write preloader, LK, TEE, calibration, eMMC boot areas or the SD card.

The UI remains frozen after writes so old Android mount definitions cannot
remount the new root. Type `BOOT` when offered to start Linux. If you decline,
ADB stays available for inspection; do not attempt to use TWRP's old Format/
Wipe controls. The verified storage write is not a claim of a successful boot.

On first boot the platform guard checks the native root/boot/modules/packages,
then applies the checked Omarchy adaptations before the desktop starts. A
failed guard blocks the desktop rather than silently continuing with a broken
integration. Log in as `trona` with the password chosen during build.

## Acceptance after first boot

1. Confirm local touch login, display, touch and GPU desktop.
2. Connect Wi-Fi, then check keyboard visibility, Super and rotation/lock.
3. Run `sudo trona-update-check check` and `pacman -Qkk trona-platform`.
4. Check sound, cameras and the remaining [hardware checklist](hardware.md).
5. Test reboot, shutdown, Power wake and the touch lock before daily use.

No personal Wi-Fi network, Bluetooth pairing, SD UUID, timezone, machine ID or
SSH host key is cloned. Pair/connect your devices normally. An ordinary SD card
can be mounted through the file manager; the optional UUID-specific automatic
mount helper is not configured by this installer.

## Interrupted installation

Keep the exact workspace, cable access and recovery. `install-state.json`
records which write was about to start. Do not restore the old GPT and assume
Android data will return: a partly overwritten root area cannot be undone by
a partition-table restore.

After reconnecting **the same tablet** in recovery, an interrupted operation
may be explicitly restarted with:

```sh
./install.sh install --resume
```

This is not a continuation at a guessed byte offset: it rechecks the complete
bundle and protected area, accepts only the saved old/new GPT and boot states,
asks for erasure confirmation again, then rewrites Linux from the beginning.
Completed installs are refused. A torn GPT header or partial/unknown boot image
requires manual recovery; the installer will not guess. Preserve the private
original prefix and recovery image for expert repair.

To report an issue, provide the release/commit, host architecture, recovery
version, last successful stage and the error text. Do not attach the workspace,
firmware, password hash, complete device capture or personal keys.

## Implementation and test scope

The host wizard uses only the Python standard library plus `adb` and Docker.
GPT parsing/planning is pure Python and independently tested with generated
tables, including valid-CRC wrong layouts, wrong capacities and corruption.
Device selection, cancellation, and refusal of completed-install resumes are
tested with mocks. Source CI does not flash hardware.

See [validation.md](validation.md) for the recorded build evidence and limits.
The existing tablet's hardware validation remains distinct from acceptance of
a new clean installation.
