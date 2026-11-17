#!/bin/bash
# Build only, inside the documented cross-build container. No tablet access.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
k=$(cd "${1:?Usage: build-modules.sh SEPARATE_PREPARED_KERNEL NEW_OUTPUT}" && pwd)
out=${2:?Output directory required}
[[ ! -e $out ]] || { echo 'Output already exists' >&2; exit 1; }
[[ $(cat "$k/include/config/kernel.release") == 6.18.56-trona-arch14 ]]
mkdir -p "$out";out=$(cd "$out" && pwd)
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
source_hash=$(sha256sum "$k/drivers/remoteproc/mtk_scp.c" | cut -d' ' -f1)
case "$source_hash" in
 2b29e1a7c0c3de1a72bc9b0935b401146090c40b089600c0d3154691ef3e66e4)
  patch -d "$k" -p1 < "$here/../scp-proxy/scp-dependency.patch" ;;
 92a80e59ca6e055904e36ea102b8a38dab98e18d926f1555dfc0b04394614bbd) ;;
 *) echo 'Different SCP driver; review dependency patch' >&2;exit 1 ;;
esac
cp "$repo/kernel/media.config" "$k/.config"
(cd "$k"; scripts/config --enable MEDIA_PLATFORM_SUPPORT --enable MEDIA_PLATFORM_DRIVERS \
 --enable V4L_MEM2MEM_DRIVERS --module VIDEO_MEDIATEK_VCODEC --module MTK_SCP --module RPMSG_MTK_SCP)
make -s -C "$k" olddefconfig prepare modules_prepare
mkdir "$out/host"
cp "$here/../scp-proxy/host/"* "$out/host/"
make -s -C "$k" M="$out/host" modules
make -s -j"${JOBS:-4}" -C "$k" M=drivers/rpmsg modules
make -s -j"${JOBS:-4}" -C "$k" M=drivers/remoteproc \
 KBUILD_EXTRA_SYMBOLS="$k/drivers/rpmsg/Module.symvers $out/host/Module.symvers" modules
make -s -j"${JOBS:-4}" -C "$k" M=drivers/media \
 KBUILD_EXTRA_SYMBOLS="$k/drivers/remoteproc/Module.symvers $k/drivers/rpmsg/Module.symvers" modules
mkdir "$out/modules"
for name in mtk_scp_ipi mtk_rpmsg mtk_scp v4l2-h264 v4l2-vp9 v4l2-mem2mem mtk-vcodec-common mtk-vcodec-dbgfs mtk-vcodec-dec; do
 mapfile -t files < <(find "$k/drivers/remoteproc" "$k/drivers/rpmsg" "$k/drivers/media" -name "$name.ko")
 [[ ${#files[@]} == 1 ]];cp "${files[0]}" "$out/modules/"
done
cp "$out/host/trona_vdec_proxy.ko" "$out/modules/"
cp "$repo/kernel/dts/"* "$k/arch/arm64/boot/dts/mediatek/"
cat "$here/scp-browser.dtsi" >> "$k/arch/arm64/boot/dts/mediatek/mt8183-amazon-trona.dts"
make -s -C "$k" mediatek/mt8183-amazon-trona.dtb
cp "$k/arch/arm64/boot/dts/mediatek/mt8183-amazon-trona.dtb" "$out/trona-video.dtb"
echo 'Built optional modules and DTB. Reuse the qualified Image.gz; nothing flashed.'
