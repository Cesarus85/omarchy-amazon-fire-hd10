# Omarchy on Amazon Fire HD 10 (2021)

Native Arch Linux ARM, Hyprland and Omarchy on the **11th-generation Fire HD 10,
codename Trona, MediaTek MT8183**. This is a community hardware port, running
Linux directly on the tablet rather than inside Android.

[Deutsch](docs/README.de.md) · [Hardware status](docs/hardware.md) ·
[Build](docs/build.md) · [Installation](docs/install.md) ·
[Updates](docs/updates.md) · [Architecture](docs/architecture.md)

## What this project provides

- A Linux **6.18.56 / trona-arch12** patch set, configuration and complete board
  device-tree sources.
- Adapted Wi-Fi/Bluetooth, audio, battery/power, accelerometer and camera drivers.
- The tablet integration used with **Omarchy 4.5.0-3**: touch app grid,
  rotation/rotation lock, keyboard visibility and Super/Alt keys, window sizing,
  camera app, standby, charging integration, hardware volume keys and a touch lock.
- Pacman packaging and update checks protecting the native boot chain and tablet
  customizations.
- Build scripts, explicit per-device configuration, tests, troubleshooting and
  source/author credits.

**This first publication is a source release for experienced Linux porters,
not a one-command installer or a ready-to-flash system image.** The original
tablet has been tested interactively; installing these exported sources on a
second, factory-state tablet has not yet been tested. Read the
[reproduction boundaries](docs/install.md#reproduction-boundaries) first.

## Working on the reference tablet

| Function | Result |
|---|---|
| Native ARM64 boot and internal ext4 storage | Working; all eight CPU cores |
| Display, GPU and touch | Hyprland, Mesa Panfrost, 1200 × 1920 panel |
| Orientation | Automatic and manual, four orientations; rotation lock |
| On-screen keyboard | Show/hide, Super/Alt shortcuts; separate secure lock keyboard |
| Wi-Fi | Scan, connect, reconnect after reboot and standby |
| Bluetooth | MX Keys Mini, authenticated pairing and reconnect |
| Speakers and volume buttons | Audible playback and physical volume control |
| Front / rear cameras | Preview, photo saving and orientation confirmed |
| Microphone | Understandable recording confirmed during bring-up; further testing deferred |
| Battery / USB-C power | Battery display; charging from the attached Mac |
| Standby / power button | Tested s2idle, wake to touch lock, Wi-Fi and keyboard recovery |
| SD card | Reader and exFAT mount working |
| Shutdown | Confirmed with and without USB attached |

This table describes observed results, not a guarantee for other sensor revisions
or every app. Deep firmware suspend, USB-A charger detection, headphone-jack
behavior, Bluetooth audio, USB host mode and long-term battery measurements are
not claimed complete. See [hardware.md](docs/hardware.md).

## Start here

1. Check the exact model and [unlock/recovery prerequisites](docs/install.md).
2. Read [what must stay private](docs/security.md), especially firmware calibration.
3. [Build the kernel and modules](docs/build.md) without touching the tablet.
4. Prepare a device-specific Arch ARM root filesystem, firmware and boot image.
5. Follow the staged [installation and validation checklist](docs/install.md).
6. Use the [guarded update process](docs/updates.md).

The build scripts only produce local files. They do not repartition or flash a
connected device. The reference port ultimately removed Android system/vendor/data
to give Linux about **28.93 GiB** of partition space on the 32-GB model, while
preserving bootloader/firmware/recovery partitions. That migration is destructive;
the old Android-loop boot layout and the final native-p18 layout are incompatible.

## Repository map

| Directory | Contents |
|---|---|
| `kernel/` | Core patch, recorded configurations, board DTS, module inventory |
| `drivers/` | Adapted external drivers, radio sources and regression test |
| `platform/payload/` | Runtime scripts, units, UCM audio, camera, Hyprland and lock integration |
| `boot/` | Device-profile initramfs template and shutdown helper |
| `scripts/` | Kernel/module/boot builds, package staging, read-only diagnostics |
| `userspace/wvkbd/` | Keyboard patch and native build recipe |
| `tests/` | Source, profile, rotation and keyboard checks |
| `docs/` | Installation, operation, limitations and implementation details |
| `firmware/` | Required filenames/hashes only; no firmware blobs |

## Attribution and license

Built and physically tested by Stefan Maier with OpenAI Codex assistance.
This work depends on substantial upstream work by Linux, MediaTek, Amazon GPL
contributors, Omarchy, Hyprland, Quickshell, Mesa/Panfrost, Arch Linux ARM,
TECHO5, the MT8183 camera community and wvkbd.

See [CREDITS.md](CREDITS.md) and [NOTICE](NOTICE) for exact sources and
component licenses. Original copyright/SPDX notices are retained. New port glue
is GPL-2.0-only unless a file states otherwise. This is not an official Amazon,
Arch Linux ARM or Omarchy product.

Bug reports and improvements are welcome. Please read
[CONTRIBUTING.md](CONTRIBUTING.md) before attaching logs.
