#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Run in the supplied container. Produces files only; never accesses a tablet.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
build=${1:?Usage: build-kernel.sh BUILD_DIRECTORY [KERNEL_TARBALL]}
archive=${2:-$build/linux-6.18.56.tar.xz}
mkdir -p "$build"
build=$(cd "$build" && pwd)
if [[ ! -f $archive ]]; then
  curl --fail --location --retry 3 https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.18.56.tar.xz -o "$archive"
fi
echo "4633bbda2ab95eaf2a7af4041236451d6759c72e6946265531af4b3aacefbf89  $archive" | sha256sum -c -
k="$build/linux-6.18.56"
[[ ! -e $k ]] || { echo "Use a fresh build directory." >&2; exit 1; }
tar -xf "$archive" -C "$build"
cd "$k"
patch --batch -p1 < "$repo/kernel/linux-6.18.56-trona.patch"
patch --batch -p1 < "$repo/kernel/dvfs.patch"
cp "$repo"/kernel/dts/* arch/arm64/boot/dts/mediatek/
cp "$repo/kernel/arch14.config" .config
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
export KBUILD_BUILD_USER=trona KBUILD_BUILD_HOST=linux-build
export KBUILD_BUILD_TIMESTAMP='2026-11-16 20:00:00 UTC' KBUILD_BUILD_VERSION=1
make -s olddefconfig
make -s -j"${JOBS:-4}" Image.gz mediatek/mt8183-amazon-trona.dtb
mkdir -p "$build/artifacts"
cp arch/arm64/boot/Image.gz "$build/artifacts/"
cp arch/arm64/boot/dts/mediatek/mt8183-amazon-trona.dtb "$build/artifacts/trona.dtb"
cp .config "$build/artifacts/kernel.config"
cp vmlinux.symvers Module.symvers
cp modules.builtin* "$build/artifacts/"
printf 'Kernel and DT built. Next: scripts/build-modules.sh %q\n' "$build"
