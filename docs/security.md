# Access and private data

The reference system uses a normal Linux password for sudo, startup authentication
and the touch lock. A wrong password was rejected in a physical test. The root
password is locked; a serial console presents a normal login prompt.

USB maintenance SSH listens only on the gadget's link-local addresses. It permits
root **public-key** authentication, and disables password and keyboard-interactive
authentication. Generate your own key and install only its public half. This
repository contains no authorized_keys, private key, account password or hash.
The USB MAC/address constants in the scripts are locally administered transport
identifiers, not the tablet's factory Wi-Fi/Bluetooth identity.

The bootloader remains unlocked and TWRP remains accessible. Root storage is
**not encrypted**. An attacker with recovery access can read or change files;
the desktop lock is not a substitute for encryption or verified boot.

The published initramfs template stops on failure instead of starting the old
development root shell. Recovery is the explicit repair route. This change is
part of the public build template, not a claim that the reference tablet was
reflashed during publication.

## Do not publish

- Wi-Fi profiles/passwords, Bluetooth pairing keys, SSH keys or host keys.
- IDME, NVRAM, factory calibration, device serials or full live device-tree dumps.
- Android system/vendor/data dumps, recovery/boot images or firmware blobs without
  a confirmed redistribution basis.
- Browser profiles, photos, microphone recordings, home-directory archives,
  unfiltered journal/dmesg output or hardware register captures.
- Rendered private device profiles and generated platform packages.

Use ignored `local/` for private build inputs. The GitHub source tree has
allowlisted source/config files rather than an exported root filesystem.
Firmware is represented only by filenames/hashes and source-extraction guidance.

The optional USB HTTP proxy helpers are for an explicitly configured rescue path;
normal networking is native WLAN. Do not enable the proxy service unnecessarily,
and do not treat its unauthenticated local transport as a general network proxy.
