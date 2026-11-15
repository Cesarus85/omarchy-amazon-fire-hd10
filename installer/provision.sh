#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Fresh Arch ARM chroot only, called by build.sh. Never run on a live installation.
set -euo pipefail
layout=${1:?}
[[ -d /tmp/trona-source && ! -e /usr/local/sbin/trona-update-check ]]
# Remove generic boot machinery before a full package upgrade.
mapfile -t bootpkgs < <(pacman -Qq | grep -E '^(linux-aarch64|mkinitcpio|mkinitcpio-busybox)$' || true)
if ((${#bootpkgs[@]})); then pacman -Rdd --noconfirm "${bootpkgs[@]}"; fi
pacman-key --init
pacman-key --populate archlinuxarm
mkdir /tmp/trona-keyring
bsdtar -xf /tmp/keyring.pkg.tar.zst -C /tmp/trona-keyring
cp /tmp/trona-keyring/usr/share/pacman/keyrings/omarchy* /usr/share/pacman/keyrings/
gpg --show-keys --with-colons /usr/share/pacman/keyrings/omarchy.gpg | grep -q ':40DFB630FF42BCFFB047046CF0134EE680CAC571:'
pacman-key --populate omarchy
# Trust is now in pacman's GnuPG database. Let the signed package own these files.
rm -f /usr/share/pacman/keyrings/omarchy.gpg /usr/share/pacman/keyrings/omarchy-trusted \
  /usr/share/pacman/keyrings/omarchy-revoked
cat >> /etc/pacman.conf <<'EOF'

[omarchy]
SigLevel = Required DatabaseOptional
Server = https://pkgs.omarchy.org/stable/$arch
EOF
pacman -Syu --needed --noconfirm python
# Abort before an unqualified package set can become an installation image.
python3 - <<'PY'
import json,subprocess
p='/tmp/trona-source/platform/payload/usr/share/trona-platform/qualified.json'
for name,expected in json.load(open(p)).items():
 repo='omarchy' if name in ('omarchy','omarchy-settings','quickshell') else 'extra'
 text=subprocess.check_output(['pacman','-Si',repo+'/'+name],text=True,env={'PATH':'/usr/bin','LC_ALL':'C'})
 actual=next(x.split(':',1)[1].strip() for x in text.splitlines() if x.startswith('Version '))
 if actual!=expected:raise SystemExit(f'Unqualified repository package: {name} {actual}; need {expected}. No tablet has been modified.')
PY
# The upstream disk installer is deliberately never invoked.
pacman -S --needed --noconfirm base-devel git yay omarchy/omarchy omarchy/omarchy-settings \
 extra/hyprland omarchy/quickshell extra/libcamera extra/libcamera-ipa extra/gst-plugin-libcamera \
 python python-pyserial python-gobject bash systemd kmod util-linux mesa \
 pipewire pipewire-pulse wireplumber alsa-ucm-conf alsa-utils networkmanager \
 bluez bluez-utils blueman seatd uwsm foot gtk3 gst-plugin-gtk gst-plugins-base-libs \
 gst-plugins-good udisks2 socat openssh sudo libcamera-tools v4l-utils grim \
 qt6-declarative wayland wayland-protocols libxkbcommon cairo pango scdoc \
 chromium thunar thunar-volman gvfs polkit polkit-gnome xdg-desktop-portal-hyprland \
 xdg-desktop-portal-gtk noto-fonts e2fsprogs exfatprogs \
 zram-generator pacman-contrib
python3 - <<'PY'
import json,subprocess
for name,version in json.load(open('/tmp/trona-source/platform/payload/usr/share/trona-platform/qualified.json')).items():
 assert subprocess.check_output(['pacman','-Q',name],text=True).strip()==name+' '+version,name
PY
[[ $(id -u alarm) == 1000 && $(id -g alarm) == 1000 ]]
usermod -l trona -d /home/trona -m alarm
groupmod -n trona alarm
for group in seat video input render audio wheel; do
  getent group "$group" >/dev/null || groupadd --system "$group"
done
usermod -aG seat,video,input,render,audio,wheel trona
passwd -l root
passwd -l trona
install -d -m755 /home/trona/.config /home/trona/.local/state/trona
cp -a /etc/skel/. /home/trona/
cp -a /usr/share/omarchy/config/. /home/trona/.config/
cat > /home/trona/.config/hypr/autostart.lua <<'EOF'
hl.on("hyprland.start", function()
  hl.exec_cmd("uwsm finalize")
  hl.exec_cmd("/usr/local/bin/trona-desktop-apps")
end)
EOF
cat > /home/trona/.config/hypr/looknfeel.lua <<'EOF'
hl.config({decoration={blur={enabled=false},shadow={enabled=false}},
  animations={enabled=false},cursor={no_hardware_cursors=true},
  xwayland={enabled=false}})
EOF
printf 'hl.config({input={kb_layout="%s",follow_mouse=0}})\n' "$layout" > /home/trona/.config/hypr/input.lua
printf 'hl.monitor({output="",mode="preferred",position="auto",scale=1.25})\n' > /home/trona/.config/hypr/monitors.lua
printf '0\n' > /home/trona/.local/state/trona/orientation
printf '1.25\n' > /home/trona/.local/state/trona/scale
printf '1\n' > /home/trona/.local/state/trona/auto-rotate
python3 - <<'PY'
import json,pathlib
p=pathlib.Path('/home/trona/.config/omarchy/shell.json')
d=json.loads(p.read_text()); d['idle']={'screensaver':0,'lock':0}
p.write_text(json.dumps(d,indent=2)+'\n')
PY
chown -R trona:trona /home/trona
echo trona-linux > /etc/hostname
echo LANG=C.UTF-8 > /etc/locale.conf
ln -sf /usr/share/zoneinfo/UTC /etc/localtime
systemctl disable sshd sddm systemd-networkd systemd-resolved || true
systemctl mask systemd-networkd systemd-resolved
rm -rf /tmp/trona-keyring
