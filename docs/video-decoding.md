# Video decoding investigation — 2026-11-16

**Hardware video decoding remains disabled.** The test brought up the MT8183
SCP and decoder device, but both tested userspace paths failed to produce valid
H.264 video. The reference tablet was returned to its previously qualified
arch14 boot image; no browser acceleration flags or sandbox exceptions were
installed. General performance and tablet usability work is tracked separately
in the [roadmap](roadmap.md).

The [measurement summary](evidence/video-decoding-2026-11-16.json) records both
the failed decoder qualification and the final restoration checks: update
integrity passed, 183 platform files checked with zero altered files, and no
failed system services. Temporary modules, firmware and media were removed
from the tablet after reboot. No persistent experimental service remains.

## Baseline

Arch14, Chrome 157.0.8103.78, Mesa 26.2.7, FFmpeg 9.0.2, GStreamer 1.28.9.
An isolated temporary Chrome profile used Panfrost for compositing but advertised
an empty `videoDecoding` capability list. Its local 1920x1080/30 H.264 playback
reported `FFmpegVideoDecoder` and `kIsPlatformVideoDecoder=false`.

In one 20-second sample after startup, summed Chrome-process CPU time was
27.8 CPU-seconds: about **139% of one CPU core**, not 139% of the eight-core SoC.
The player reported 734 total / 23 dropped frames over its entire playback
lifetime, including startup; those frame counters do not cover exactly the CPU
sampling interval. This was a synthetic 12-second looping clip in a window,
not fullscreen YouTube, a battery test or an estimate for all codecs. A short
native build overlapped part of the first probe, so it is an indicative baseline,
not a controlled before/after optimization result.

## Kernel / firmware bring-up

Before the test, `/dev/video0` and `/dev/video1` were camera streams; no decoder
was registered and SCP was disabled in the device tree.

The isolated [experiment](../experiments/video-decode/README.md) used:

- Unchanged arch14 kernel executable and rootfs, with a DT-only candidate.
- An 8 MiB `shared-dma-pool` at physical `0x50000000`, reserved `no-map` before
  Linux memory allocation. The installed firmware ELF has a LOAD segment there;
  this cannot safely be added as a live overlay to already-allocated RAM.
- Upstream `mtk_scp`/RPMsg, V4L2 mem2mem and MediaTek decoder modules built against
  the exact kernel release and existing media configuration.
- Packaged `mediatek/mt8183/scp.img`, reporting
  `kukui_scp_v2.0.13324-280b9fce97`, SHA256
  `401e9c027b0e105efa9eb9b31683924a8d5f5ab3b1654f0915b174effe2df33d`.

The owner confirmed the desktop after reboot. Linux confirmed the reserved pool,
SCP `running`, `/dev/video2` named `MT8183 video decoder`, and `/dev/media1`.
GStreamer registered `v4l2slh264dec`, with MM21 tiled capture output. These are
bring-up results, **not successful decoding**.

## Reproduced blocker

Both GStreamer and a separately built experimental VA-API/V4L2 bridge reached
the hardware but failed on H.264. The 1080p stream produced firmware status
`196738` (`0x30082`) for every input frame. A 640x480 Baseline-profile stream
failed identically. FFmpeg through the bridge reported 360 decode errors; its
two output frames had identical checksums and did not constitute video output.

Additional isolated tests did not resolve it:

- An older official linux-firmware image from tag `20220310`, reporting
  `kukui_scp_v2.0.2486-e20d73eaa` (the archive labels it v2.0.2745).
- A temporary 31-bit DMA mask, which moved actual bitstream IOVAs from
  `0xff700000` to `0x7ff00000`.
- Temporarily holding the decoder's SMI LARB runtime power active.

A diagnostic module showed valid-looking first-slice bytes (`00 00 01 65 ...`),
SPS dimensions and allocated buffers. Inspection of the older official firmware
located the `0x30082` return at its barrel-shifter initialization timeout, whose
log string is `init BarelShift fail`. Correctly indexed VDEC_LD register reads
in the kernel after the failed request were zero at offsets `0x8c`, `0xb0`,
`0xc8`, `0xe8`, `0xf4`, `0x108` and `0x110`; decoder IRQ count remained zero.

This narrows the next investigation to the decoder hardware access / reset /
clock / bus setup and the SCP's view of it on the Fire boot chain. It does **not**
prove which of those is wrong. Do not disable global access protections or
change unrelated PMIC/clock settings speculatively. The next useful evidence is
an AP-versus-SCP register/access comparison against the stock MT8183 setup.

## Browser bridge boundary

The test built [shelterx's MT8183 bridge](https://github.com/shelterx/vaapi-v4l2-bridge-mt8183)
at commit `7e5d1dd49e37e9b99521d6ea63da59c6e1a2bfec` in a temporary directory.
It was only selected for the FFmpeg test using process-local `LIBVA_DRIVER_NAME`
and `LIBVA_DRIVERS_PATH`; it was not installed globally or enabled in the user's
browser. No sandbox was disabled. Browser integration, licensing review for any
redistribution, VP9/AV1 coverage, seeking, repeated playback and post-standby
qualification still need work after the underlying decoder produces correct
frames. Do not infer hardware support from Chrome's generic `video_decode:
enabled` label alone.

Relevant upstream references:

- [Linux MT8183 device tree](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/b3438e5ca785565a65ef231bc03cd5a05c3be5c7/arch/arm64/boot/dts/mediatek/mt8183.dtsi)
- [Official firmware inventory and license references](https://kernel.googlesource.com/pub/scm/linux/kernel/git/firmware/linux-firmware.git/+/refs/tags/20250211/WHENCE)
- [GStreamer stateless H.264 decoder](https://gstreamer.freedesktop.org/documentation/v4l2codecs/v4l2slh264dec.html)
