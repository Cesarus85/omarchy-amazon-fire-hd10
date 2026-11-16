# MT8183 decoder experiment — not enabled in the port

This experiment **does not produce valid decoded video on the reference Trona**.
It is retained for driver development. Do not add it to the normal installer or
advertise browser acceleration. See [results and next investigation](../../docs/video-decoding.md).

## Reproduce the build, without accessing a tablet

Use a **separate copy** of a completed arch14 kernel/module build from the
[build guide](../../docs/build.md). The existing `Module.symvers`, prepared camera
sources and exact kernel release are required. Run inside the cross-build
container; `/repo` is this repository and `/build/video/linux-6.18.56` is the
separate copy. These commands produce files only:

```sh
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
k=/build/video/linux-6.18.56
cd "$k"
cp /repo/kernel/media.config .config
scripts/config --enable MEDIA_PLATFORM_SUPPORT --enable MEDIA_PLATFORM_DRIVERS \
  --enable V4L_MEM2MEM_DRIVERS --module VIDEO_MEDIATEK_VCODEC \
  --module MTK_SCP --module RPMSG_MTK_SCP
make -s olddefconfig prepare modules_prepare
make -s -j4 M=drivers/rpmsg modules
make -s -j4 M=drivers/remoteproc \
  KBUILD_EXTRA_SYMBOLS="$k/drivers/rpmsg/Module.symvers" modules
make -s -j4 M=drivers/media \
  KBUILD_EXTRA_SYMBOLS="$k/drivers/remoteproc/Module.symvers $k/drivers/rpmsg/Module.symvers" modules
cp /repo/kernel/dts/* arch/arm64/boot/dts/mediatek/
cat /repo/experiments/video-decode/scp-video.dtsi >> \
  arch/arm64/boot/dts/mediatek/mt8183-amazon-trona.dts
make -s mediatek/mt8183-amazon-trona.dtb
```

The test reused the known-good arch14 `Image.gz` and initramfs, changing only
its appended DTB. New modules were loaded manually from a temporary directory,
not installed for autoload. Dependencies/load order, with the existing camera
media core already loaded:

1. `mtk_scp_ipi`, `mtk_rpmsg`, `mtk_scp`
2. `v4l2-h264`, `v4l2-vp9`, `v4l2-mem2mem`
3. `mtk-vcodec-common`, `mtk-vcodec-dbgfs`, `mtk-vcodec-dec`

No flashing command is provided for this failed candidate. A developer test must
retain the per-device boot/recovery/hash checks, a tested return image and a
matching update guard. Do not work around the guard or use a different tablet's
boot image. Inspect the live reserved-memory node **and** `/proc/iomem` before
loading SCP. The regular package and device tree deliberately remain unchanged.

## Test stream and actual output

Create a deterministic-content synthetic sample with FFmpeg (encoded bytes may
vary with encoder version):

```sh
ffmpeg -f lavfi -i 'testsrc2=size=1920x1080:rate=30' -t 12 \
  -c:v libx264 -preset fast -crf 23 -pix_fmt yuv420p \
  -profile:v high -level 4.1 -an -movflags +faststart test-1080p.mp4
```

After a developer has brought up the driver, the isolated GStreamer pipeline is:

```sh
gst-launch-1.0 -v filesrc location=test-1080p.mp4 ! qtdemux ! \
  h264parse ! v4l2slh264dec ! fakesink sync=false
```

**EOS and a zero exit code are not evidence of correct decoding.** In this test
GStreamer reached EOS while the kernel reported errors for every frame. Compare
actual output against software decoding and inspect kernel errors before doing
browser or battery comparisons. Avoid retaining large raw-video dumps.
