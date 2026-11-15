#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
build=$(cd "${1:?Usage: build-modules.sh BUILD_DIRECTORY}" && pwd)
k="$build/linux-6.18.56"
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
[[ $(cat "$k/include/config/kernel.release") == 6.18.56-trona-arch12 ]]
mkdir -p "$build/external" "$build/artifacts/modules"
cp -a "$repo/drivers/." "$build/external/"
# Media modules are prepared with their own recorded config, as on the tablet.
cp -a "$repo/drivers/camera/isp" "$k/drivers/media/platform/mediatek/"
cp "$repo/drivers/camera/sensors/hi556.c" "$k/drivers/media/i2c/hi556.c"
cp "$repo/drivers/camera/phy/phy-mtk-mipi-csi-0-5.c" "$k/drivers/phy/mediatek/"
grep -q 'mediatek/isp/Kconfig' "$k/drivers/media/platform/mediatek/Kconfig" ||
 printf '\nsource "drivers/media/platform/mediatek/isp/Kconfig"\n' >> "$k/drivers/media/platform/mediatek/Kconfig"
grep -q 'isp/' "$k/drivers/media/platform/mediatek/Makefile" ||
 printf '\nobj-y += isp/\n' >> "$k/drivers/media/platform/mediatek/Makefile"
cp "$repo/kernel/media.config" "$k/.config"
make -s -C "$k" olddefconfig prepare modules_prepare
for dir in drivers/media drivers/phy/mediatek drivers/input/keyboard fs/exfat fs/nls fs/fuse; do
 make -s -j"${JOBS:-4}" -C "$k" M="$dir" modules
done
# Resolve the camera stack's module-to-module interfaces explicitly.
media_symbols="$k/drivers/media/Module.symvers"
for dir in audio battery poweroff cpufreq accelerometer camera/csi0a camera/udmabuf camera/sensors; do
 make -s -j"${JOBS:-4}" -C "$k" M="$build/external/$dir" KBUILD_EXTRA_SYMBOLS="$media_symbols" modules
done
make -s -j"${JOBS:-4}" -C "$k" M="$build/external/connectivity" \
 AUTOCONF_H="$k/include/generated/autoconf.h" MTK_PLATFORM=mt8183 \
 CONFIG_MTK_COMBO=m CONFIG_MTK_BTIF=y MTK_CONSYS_ADIE=MT6631 \
 'CONFIG_MTK_COMBO_CHIP="CONSYS_6771"' TARGET_BUILD_VARIANT=user modules
python3 "$repo/scripts/collect-modules.py" "$build"
