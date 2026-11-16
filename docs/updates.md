# Updates and qualification

On the configured reference tablet, ordinary updates use `omarchy update`.
The full flow and a real Omarchy reinstall were tested, followed by reboot.
The native kernel does not follow a generic Arch kernel package.

The platform package currently requires:

| Component | Qualified version |
|---|---|
| Omarchy / settings | 4.5.0-3 |
| Hyprland | 0.57.2-1 |
| Quickshell | 0.3.2.r7.g2b4501a-1 |
| libcamera / IPA / GStreamer plugin | 0.7.2-4 |
| Native kernel | 6.18.56-trona-arch14 |
| Trona platform package | 2026.11.16-2 |

Mesa 26.2.7 and the tested Qt update worked with this set. Future versions are
not automatically qualified.

## What the guard does

Before package transactions it checks board identity, kernel ABI, root
UUID/size, boot/recovery read-only state and hashes, module metadata, platform
file ownership/hashes and expected Omarchy integration. It blocks generic
kernel/bootloader packages and unrelated /boot or kernel-module payloads.
Removing trona-platform through an ordinary transaction is blocked.

After transactions it reapplies checked tablet patches, restores integration
and verifies it again. A boot service repeats the checks and protects boot and
recovery before the desktop. The image hashes are per-device inputs, not a
universal whitelist copied from the author's tablet.

```sh
sudo trona-update-check check
pacman -Qkk trona-platform
```

An unsatisfied exact dependency means the whole transaction needs attention.
Do not bypass it with `--nodeps`, broad `--overwrite`, disabled hooks or a
partial `IgnorePkg` upgrade. The current package is intentionally not a
promise that arbitrary rolling-release combinations remain safe.

## Maintaining a new release

1. Build against the intended kernel and record module ABI.
2. Review upstream changes to every QML/Lua/shell anchor used by the patchers.
3. Update exact dependencies and `qualified.json` together.
4. Build a new device-specific payload/manifest.
5. Test the real pacman transaction and restoration after reinstalling Omarchy.
6. Reboot and test touch, rotation while locked, password rejection/unlock,
   cameras, radios, poweroff and offline standby.
7. Publish the new source qualification and clearly distinguish compile checks
   from physical tests.

Security updates to pinned components require timely port maintenance. The guard
can prevent an unreviewed upgrade, but cannot make old software secure forever.

No automatic recovery from interrupted package transactions or encrypted-data
rollback is implemented. Boot/kernel updates are a separate reviewed operation.
The source release's newly parameterized build/staging tools have not themselves
been installed on a second tablet.

The arch14 CPU-DVFS migration is an explicit platform update. The path guard
allows removal of arch12 modules only while upgrading the exact previous
`trona-platform 2026.11.15-7` package. It does not allow arbitrary old kernels
or generic boot payloads. See [CPU qualification](cpu-dvfs.md).
