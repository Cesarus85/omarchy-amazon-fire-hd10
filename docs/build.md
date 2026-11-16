# Building the source release

The scripts build files only. No USB, SSH, partition writer or automatic reboot
is invoked. A Linux build host or Docker is required. Allow roughly 15–25 GB of
build space. Cross compilation works on ARM64 and x86-64 hosts; the runtime
package and keyboard should be built in an **AArch64 Arch** environment.

## Kernel and modules

From the repository root:

```sh
docker build -t trona-public-build .
docker volume create trona-public-build
docker run --rm -v "$PWD:/src:ro" -v trona-public-build:/build \
  trona-public-build scripts/build-kernel.sh /build
docker run --rm -v "$PWD:/src:ro" -v trona-public-build:/build \
  trona-public-build scripts/build-modules.sh /build
mkdir -p local/artifacts
docker run --rm -v trona-public-build:/build:ro -v "$PWD/local/artifacts:/out" \
  trona-public-build sh -c 'cp -a /build/artifacts/. /out/'
```

`build-kernel.sh` refuses to overwrite an existing kernel tree. To reuse a
download, pass its path as the second argument. The Linux 6.18.56 source tarball
is checked against the recorded SHA256 before extraction. Its source is
[kernel.org](https://cdn.kernel.org/pub/linux/kernel/v6.x/).

Outputs: `Image.gz`, `trona.dtb`, recorded kernel config, builtin module
metadata and all modules listed in `kernel/modules.json`. The core image uses
the arch14 configuration; media modules are subsequently built with the
recorded media configuration, matching the original development arrangement.
Do not substitute modules from a distribution kernel with a similar version.

The complete kernel patch restores the arch12 PSCI behavior rather than
including later vendor-suspend experiments. New compiler versions can produce
different binaries; a successful rebuild is not an automatic hardware release.

## Keyboard

In an AArch64 Arch build environment, install the normal build dependencies:
`base-devel git wayland wayland-protocols libxkbcommon cairo pango`.

```sh
userspace/wvkbd/build.sh "$PWD/local/wvkbd"
```

The script obtains wvkbd v0.16 and applies the included layout and IME keymap
patches. It does not
install the result. Supply `local/wvkbd/wvkbd-mobintl` to the package-staging
script. The secure password keyboard is QML and is already in the payload.

## Private inputs

Keep these under ignored `local/`, never commit them:

- A private `device-profile.json`, based on the example.
- The firmware files listed in `firmware/required.json`, extracted from your
  own supported firmware installation. A mismatch stops staging; investigate
  it rather than accepting arbitrary firmware.
- Your ARM root filesystem staging directory.
- A **static AArch64 BusyBox** with the init/shutdown applets, including
  mount, switch_root, blockdev, blkid, losetup and poweroff/reboot.
- Your own backups and any recovery/boot images used during installation.

Obtain BusyBox from its upstream source and build it with
`ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-` and `CONFIG_STATIC=y`.
The boot builder rejects an ELF for another architecture or a dynamic BusyBox.
Use the e2fsprogs and runtime libraries from the prepared Arch ARM rootfs.
The compact initramfs includes the recorded e2fsck library set; if your newer
userspace needs additional shared libraries, update and test that set first.

## Boot image

The profile requires a real root UUID/size and measured hashes. Before building,
`boot_sha256` can contain the existing known boot hash; it is not used by the
initramfs. After building, set it to the *new padded boot hash* before staging
the platform package.

```sh
python3 scripts/build-boot.py \
  --profile local/device-profile.json \
  --kernel local/artifacts/Image.gz --dtb local/artifacts/trona.dtb \
  --busybox local/busybox --rootfs local/rootfs --output local/boot
```

This creates `boot.img` and `manifest.json`. The latter explicitly says
`booted: false`; do not change that claim until you have tested it.
Copy `padded_boot_sha256` into the private profile's `boot_sha256`.

The image uses Android header v1, 2048-byte pages, kernel address
`0x40200000`, ramdisk `0x55000000`, tags `0x54000000` and an appended DTB.
There is no generic EFI/GRUB/limine boot step.

## Platform package

```sh
python3 scripts/stage-platform.py \
  --profile local/device-profile.json --artifacts local/artifacts \
  --firmware-dir local/firmware --keyboard local/wvkbd/wvkbd-mobintl \
  --output local/platform-package
cd local/platform-package
makepkg
```

Run makepkg as an ordinary user inside the AArch64 Arch environment. It needs
the exact dependency versions in `platform/PKGBUILD`. Staging checks required
firmware hashes, module names, ELF architecture and profile syntax; it generates
the package's integrity manifest. It does not certify the hardware compatibility
of arbitrary inputs.

The package deliberately remains tied to username `trona`, UID/GID 1000, the
native p18 layout and the recorded software versions. These are documented
constraints, not automatically portable values.

## Tests

```sh
python3 tests/check-source.py
python3 tests/test-profile.py
python3 tests/test-rotation-policy.py
python3 drivers/connectivity/tests/rx-eapol-optional-status.py
```

The WLAN test uses a host C compiler plus ASan/UBSan and exercises the actual
driver function. For QML keyboard tests, use Qt 6 QuickTest/qmltestrunner on the
configured tablet or in a compatible Qt environment; the fixture points to the
installed canonical keyboard path.

See [validation.md](validation.md) for what was actually run for this release.

The additional `kernel/dvfs.patch` validates MT6358 SRAM tracking for the
[full CPU DVFS platform](cpu-dvfs.md); `arch14.config` enables CCI devfreq.
