#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Invoked only inside the disposable ARM64 Docker build environment.
set -euo pipefail
[[ $(uname -m) == aarch64 && -f /output/device.json && ! -e /build/rootfs ]]
IFS= read -r password
layout=${1:?keyboard layout required}
mkdir -p /build/rootfs
bsdtar --numeric-owner -xpf /output/arch.tar.gz -C /build/rootfs
root=/build/rootfs
cleanup() {
  umount -Rl "$root" 2>/dev/null || true
  if [[ ${TRONA_HOST_UID:-} =~ ^[0-9]+$ && ${TRONA_HOST_GID:-} =~ ^[0-9]+$ ]]; then
    chown -hR "$TRONA_HOST_UID:$TRONA_HOST_GID" /output
  fi
}
trap cleanup EXIT
mkdir -p "$root"/{proc,sys,dev,run,tmp/trona-source}
tar -C /src --exclude=./.git --exclude=./local --exclude=./build --exclude=./dist \
  --exclude=__pycache__ -cf - . | tar -C "$root/tmp/trona-source" -xf -
cp /output/keyring.pkg.tar.zst "$root/tmp/"
cp /etc/resolv.conf "$root/etc/resolv.conf.new"
rm -f "$root/etc/resolv.conf"
mv "$root/etc/resolv.conf.new" "$root/etc/resolv.conf"
mount --bind "$root" "$root"
mount -t proc proc "$root/proc"
mount -t sysfs sysfs "$root/sys"
mount --rbind /dev "$root/dev"
mount --make-rslave "$root/dev"
mount -t tmpfs -o mode=0755 tmpfs "$root/run"
# Fresh disposable root only. Never invoke this script against a mounted tablet.
chroot "$root" bash /tmp/trona-source/installer/provision.sh "$layout"
printf 'trona:%s\n' "$password" | chroot "$root" chpasswd
unset password
if [[ -f /output/maintenance.pub ]]; then
  install -d -m700 "$root/root/.ssh"
  install -m600 /output/maintenance.pub "$root/root/.ssh/authorized_keys"
fi

# Build the pinned kernel only after package availability/signatures were checked.
/src/scripts/build-kernel.sh /build/kernel
/src/scripts/build-modules.sh /build/kernel
chroot "$root" bash /tmp/trona-source/userspace/wvkbd/build.sh /tmp/trona-wvkbd
python3 /src/scripts/build-boot.py --profile /output/device-profile.json \
  --kernel /build/kernel/artifacts/Image.gz --dtb /build/kernel/artifacts/trona.dtb \
  --busybox /bin/busybox --rootfs "$root" --output /build/boot
python3 - <<'PY'
import json,pathlib,os,tempfile
p=pathlib.Path('/output/device-profile.json'); d=json.loads(p.read_text())
d['boot_sha256']=json.loads(pathlib.Path('/build/boot/manifest.json').read_text())['padded_boot_sha256']
fd,name=tempfile.mkstemp(dir=p.parent,prefix='.profile-')
with os.fdopen(fd,'w') as f:
 f.write(json.dumps(d,indent=2)+'\n'); f.flush(); os.fsync(f.fileno())
os.replace(name,p)
PY
python3 /src/scripts/stage-platform.py --profile /output/device-profile.json \
  --artifacts /build/kernel/artifacts --firmware-dir /output/firmware \
  --keyboard "$root/tmp/trona-wvkbd/wvkbd-mobintl" --output "$root/tmp/trona-package"
chroot "$root" chown -R trona:trona /tmp/trona-package
chroot "$root" runuser -u trona -- bash -c 'cd /tmp/trona-package && makepkg --noconfirm --nodeps'
# Bootstrap the pacman database without live board checks or package scriptlets.
# No platform hooks exist in this fresh root yet; payload is copied immediately after.
chroot "$root" bash -ec 'cd /tmp/trona-package
  mapfile -t packages < <(runuser -u trona -- makepkg --packagelist)
  [[ ${#packages[@]} == 1 && -f ${packages[0]} ]]
  pacman -U --noconfirm --dbonly --noscriptlet "${packages[@]}"
  printf "%s\n" "${packages[0]}" > /tmp/trona-package.path'
package=$(cat "$root/tmp/trona-package.path")
[[ $package == /tmp/trona-package/trona-platform-*.pkg.tar.* ]]
bsdtar --numeric-owner --exclude .PKGINFO --exclude .BUILDINFO --exclude .MTREE --exclude .INSTALL \
  -xpf "$root$package" -C "$root"
python3 /src/installer/finalize-root.py "$root" /output
chroot "$root" depmod 6.18.56-trona-arch14
chroot "$root" visudo -cf /etc/sudoers.d/00-trona-admin
# The Omarchy service owns tty1; keep the separate USB serial getty.
chroot "$root" systemctl mask getty@tty1.service
chroot "$root" systemctl enable seatd trona-desktop trona-boot-guard trona-radio-platform \
  trona-wmt trona-wifi trona-bluetooth bluetooth NetworkManager trona-battery \
  trona-cpufreq-policy trona-standby trona-usb-network serial-getty@ttyGS0
if [[ -f /output/maintenance.pub ]]; then chroot "$root" systemctl enable sshd; fi
chroot "$root" systemctl set-default multi-user.target
python3 /src/installer/pack-tools.py "$root" /output/recovery-tools.tar
chroot "$root" pacman -Q > /output/packages.txt
chroot "$root" pacman -Qkk trona-platform > /output/platform-check.txt

# Remove build dependencies' caches and transient material, never user-supplied firmware.
rm -rf "$root/tmp/trona-source" "$root/tmp/trona-wvkbd" "$root/tmp/trona-package"
rm -f "$root/tmp/keyring.pkg.tar.zst" "$root/tmp/trona-package.path" "$root/etc/ssh/ssh_host_"*
rm -rf "$root/var/cache/pacman/pkg/"* "$root/var/log/"*
: > "$root/etc/machine-id"
mkdir -p "$root/var/lib/dbus"
rm -f "$root/var/lib/dbus/machine-id"
ln -s /etc/machine-id "$root/var/lib/dbus/machine-id"
chroot "$root" gpgconf --homedir /etc/pacman.d/gnupg --kill all
umount -R "$root"
python3 /src/installer/package-image.py "$root" /output /build/boot
echo 'Private installer bundle complete. No tablet was accessed.'
