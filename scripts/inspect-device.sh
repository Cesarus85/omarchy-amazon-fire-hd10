#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Run locally on the tablet. Read-only and intentionally avoids personal identifiers.
set -eu
test -r /sys/firmware/devicetree/base/compatible
tr '\000' '\n' < /sys/firmware/devicetree/base/compatible
uname -r
lsblk -o NAME,SIZE,FSTYPE,PARTLABEL,MOUNTPOINTS /dev/mmcblk0
printf '\nPower supplies\n'
for p in /sys/class/power_supply/*; do
 printf '%s: ' "${p##*/}"
 for f in status capacity online; do test ! -r "$p/$f" || cat "$p/$f"; done
done
printf '\nNative platform\n'
pacman -Q trona-platform omarchy hyprland quickshell mesa 2>/dev/null || true
systemctl --failed --no-legend
