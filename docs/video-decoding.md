# Video decoding investigation — updated 2026-11-17

**H.264 now works in an isolated experiment; normal builds still leave hardware
decoding disabled.** A decoder-only Linux MMIO proxy resolves the SCP access
failure without changing secure-firmware permissions. Hardware output matched
software decoding byte for byte at 480p and 1080p. An isolated Chrome profile
used `VaapiVideoDecoder` and reduced measured Chrome-plus-proxy CPU time by about
35% in one short comparison. This is not yet a qualified everyday feature.

After the experiment, the reference tablet was returned to its previously
qualified arch14 boot image. No browser flags, sandbox exceptions, experimental
firmware or services were installed permanently. General performance and tablet
usability work remains tracked in the [roadmap](roadmap.md).

The [proxy evidence](evidence/video-proxy-2026-11-17.json) records successful
decoding and restoration checks. The earlier [initial measurements](evidence/video-decoding-2026-11-16.json)
and [access-denial evidence](evidence/video-decoding-2026-11-17.json) remain as
historical stages, not the latest decoding result.

## Working decoder proxy — 2026-11-17

The vendor ATF boot log reports all six MM-domain-3 permission words as
`0xffffffff`, consistent with the denied decoder transaction below. A minimal
SCP read probe separately confirmed that its infrastructure accesses appear as
domain 4, whereas its MM decoder accesses appear as domain 3. These domain IDs
must not be treated as one universal SCP ID. The vendor diagnostic SMC
`0x82000101` with argument 1 returned `0xffffffff` (unsupported); no permission
setter or global DEVAPC reset was invoked.

The [source-only prototype](../experiments/video-decode/scp-proxy/README.md)
instead modifies a temporary copy of the exact pinned SCP image:

1. A hook at H.264 initialization enables an unused Cortex-M4 MPU region to trap
   decoder MMIO. A second unused region provides a private mailbox in SCP TCM.
2. Its fault handler decodes the trapped word load/store and submits a request.
3. A Linux worker performs only aligned 32-bit accesses in AP address ranges
   `0x16000000–0x160000ff` and `0x16020000–0x16024fff`, then returns the result.
4. Other addresses/instructions fail to the original SCP fault handler. There
   is no access to DEVAPC permission registers through this mailbox.

The official firmware file, secure monitor, bootloader and global access policy
remain unchanged. The patched copy was loaded from a temporary firmware search
directory. Linux already has access to the decoder; the proxy delegates that
narrow register operation rather than granting the SCP a new bus domain.

### Confirmed output and performance

| Test | Result |
| --- | --- |
| 640×480 H.264 Baseline, 60 frames | Every I420 frame byte-identical to software |
| 1920×1080 H.264 High with B frames, 120 frames | Every I420 frame byte-identical to software |
| VA-API bridge, same 120-frame 1080p stream | NV12 hash identical to software |
| Three cycles alternating the two streams | Six successful pipeline openings; no decoder errors |
| Adaptive proxy idle, three seconds | Less than one 10-ms CPU accounting tick |
| Native tiled-output decoder throughput | 120 frames in 0.966 s, about 124 fps |
| VA-API download throughput, two runs | About 62–64 fps, including format conversion |

The original busy-poll proxy consumed almost one core even while idle; the
published version sleeps after inactivity. CPU conversion of GStreamer's
uncached MM21 output to I420 was still very expensive (about 15 seconds for
120 frames), so that pipeline is a correctness check, not the browser path.
The separately built VA-API bridge uses its default cacheable-buffer conversion.

One controlled Chrome comparison used the same local 4-second looping 1080p30
H.264 clip, a temporary profile per run, a windowed player, and a 20-second
sampling interval after startup. No builds overlapped these samples:

| Measurement | Software | Hardware + proxy |
| --- | --- | --- |
| Reported decoder | `FFmpegVideoDecoder` | `VaapiVideoDecoder` |
| Platform video decoder | false | true |
| Chrome process CPU seconds | 25.18 | 10.62 |
| Linux proxy CPU seconds | 0.06 | 5.76 |
| Combined CPU, percent of one core | 126.2% | 81.9% |
| Video frames during interval | 593 | 593 |
| Additional dropped frames during interval | 0 | 0 |

Combined measured CPU time fell by **35.1%**. The proxy is included; the rest of
the system is not. This is one short synthetic A/B sample, not a battery/power
measurement, a fullscreen YouTube benchmark, or proof for all H.264 streams.
Startup drops were outside the interval. Chrome still advertised an empty
`videoDecoding` capability list despite actually selecting the hardware decoder.
There was no new human visual acceptance test.

### Before permanent enablement

The prototype has no qualified suspend/resume lifecycle and retains an SCP main
clock reference while loaded. Stopping its worker while the patched firmware
still needs it is unsafe for playback. Early development iterations triggered
SCP watchdogs; only the corrected cold-start version passed the tests above.
The handler is pinned to one firmware image and a limited set of Thumb word
access instructions, not a general-purpose emulator or a new security boundary.

Next qualify repeated cold starts, long clips, seeking, concurrent clients,
malformed streams, clean teardown and standby/wake, then integrate lifecycle,
package/update protection and browser fallback. Review firmware/bridge licensing
before distributing binaries. VP9, HEVC, AV1, protected playback and all other
codecs remain unqualified. The normal kernel configuration, device tree, platform
package and installer were deliberately left unchanged.

Restoration verified the original boot hash, SCP disabled, no experimental
modules, an empty firmware override path, update integrity passing, 183 platform
files with zero altered files, and no failed system units. Desktop, networking,
Bluetooth and standby services were active; both CPU clusters retained 1989 MHz
maximums. Temporary tablet files were deleted. Physical wake was not retested.

## Earlier baseline

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

The follow-up below identified the access-control failure. The proxy experiment
above subsequently established correct frame output with delegated MMIO.

## SCP access investigation — 2026-11-17

The same isolated DT/module candidate was tested again. The SCP address remap
matches the MT8183 Chromium EC initialization: its `0xA602xxxx` decoder accesses
map to AP physical `0x1602xxxx`. `SCP_SECURE_CTRL` was `0x800`, with the remap-disable
bit clear. The three active remap words were `0x07060504`, `0x02010008`,
`0x10090a03`. No remap or security-control bits were changed.

**DEVAPC records a denied write to the decoder control block during H.264
initialization.** Before decoding, VDECSYS_GLOBAL_CON violation status was clear.
After a one-frame Baseline H.264 attempt, status index 248 was set. Selecting its
diagnostic FIFO produced:

| Field | Observed value |
| --- | --- |
| DEVAPC debug word | `0x00430026` |
| Target address | `0x16000000` — VDECSYS_GLOBAL_CON |
| Bus master ID | `0x26` |
| Domain | `3` |
| Operation | Write |

After acknowledging only that decoder diagnostic status and repeating the frame,
the same record returned. The SCP attribution follows the isolated decode trigger;
the raw master ID is retained rather than equated with a DEVAPC configuration
table index. A separate AP read of a protected DEVAPC configuration register
reported master `0x4b`, domain `0`, read at `0x1000ea00`. Thus ordinary Linux/AP
access and the denied decoder transaction are distinguishable.

Reads of the secure DEVAPC AO configuration returned zeros **and caused an access
violation**. Those zeros cannot be interpreted as permissive settings. Diagnostic
FIFO selection/acknowledgement was used, but no access-policy register was written.
Reading a FIFO entry consumes its debug data; a repeated empty debug word without
a fresh event is not evidence that the denial disappeared.

Upstream TF-A initializes the SCP in domain 1 and grants that domain access to
VDECSYS_GLOBAL_CON and VDECSYS_FULL_TOP. That is a reference configuration, **not
the Fire's verified active policy**. The Fire vendor kernel exposes a DEVAPC debug
dump call and several broader secure-monitor services, but this investigation did
not find a verified interface for changing only the required decoder permissions.
The vendor-wrapped ATF payload read from `tee1` did not provide directly inspectable
AArch64 code; no secure firmware was modified or flashed.

At this stage, a narrowly scoped permission grant through the Fire boot chain
remained unresolved. The later proxy above avoids needing one. Do not invoke DEVAPC init,
change the SCP to domain 0, or disable protection globally. If a firmware change
is required, establish its exact handler, policy, boot compatibility and recovery
route first. The proxy still needs seeking, long-running playback and
post-standby qualification before normal installer integration.

The [follow-up evidence](evidence/video-decoding-2026-11-17.json) records the two
matching denial events and restoration checks. Source references:

- [MT8183 SCP remapping implementation](https://chromium.googlesource.com/chromiumos/platform/ec/+/be4498ebc2ee23e72c9b4919d48cc5bf692f68c0/chip/mt_scp/mt818x/memmap.c)
- [Amazon MT8183 DEVAPC driver and violation mapping](https://github.com/amazon-mt8183-devs/android_kernel_amazon_mt8183/blob/7ec747b431c61293a00ed6a01f2a01fa50c6afde/drivers/misc/mediatek/devapc/mt8183/devapc.c)
- [TF-A MT8183 DEVAPC initialization](https://github.com/ARM-software/arm-trusted-firmware/blob/master/plat/mediatek/mt8183/drivers/devapc/devapc.c)
- [TF-A MT8183 permissions table](https://github.com/ARM-software/arm-trusted-firmware/blob/master/plat/mediatek/mt8183/drivers/devapc/devapc.h)

## Browser bridge boundary

The test built [shelterx's MT8183 bridge](https://github.com/shelterx/vaapi-v4l2-bridge-mt8183)
at commit `7e5d1dd49e37e9b99521d6ea63da59c6e1a2bfec` in a temporary directory.
In the successful proxy follow-up it was selected for FFmpeg and an isolated
Chrome profile using process-local `LIBVA_DRIVER_NAME` and `LIBVA_DRIVERS_PATH`;
it was not installed globally or enabled in the user's browser. No sandbox was
disabled. Permanent browser integration, licensing review for redistribution,
other codecs, seeking, long-running playback and post-standby qualification still
need work. Do not infer hardware support from Chrome's generic `video_decode:
enabled` label alone.

Relevant upstream references:

- [Linux MT8183 device tree](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/b3438e5ca785565a65ef231bc03cd5a05c3be5c7/arch/arm64/boot/dts/mediatek/mt8183.dtsi)
- [Official firmware inventory and license references](https://kernel.googlesource.com/pub/scm/linux/kernel/git/firmware/linux-firmware.git/+/refs/tags/20250211/WHENCE)
- [GStreamer stateless H.264 decoder](https://gstreamer.freedesktop.org/documentation/v4l2codecs/v4l2slh264dec.html)
