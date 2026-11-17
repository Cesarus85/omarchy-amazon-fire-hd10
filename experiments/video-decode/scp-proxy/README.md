# Trona SCP decoder MMIO proxy — developer prototype

This **source-only experiment** produced byte-exact H.264 at 480p and 1080p on
the reference Trona, plus hardware playback in a temporary Chrome profile.
It is not installed on the reference tablet or included in normal builds.
See [measured results and limits](../../../docs/video-decoding.md) and
[machine-readable evidence](../../../docs/evidence/video-proxy-2026-11-17.json).

## What it does

The Fire secure firmware denies SCP access to the decoder. This prototype
leaves that policy intact. A hook in a temporary copy of the SCP H.264 firmware
sets a Cortex-M4 MPU trap over its decoder window. A fault handler forwards
aligned 32-bit register accesses through a private TCM mailbox. A Linux worker
performs only the following AP physical accesses:

| Inclusive range | Purpose |
| --- | --- |
| `0x16000000–0x160000ff` | VDECSYS global control |
| `0x16020000–0x16024fff` | Decoder register banks |

The worker cannot modify DEVAPC policy or SCP domain registers through this
interface. These are functional bounds for a trusted firmware experiment, not
a security sandbox for hostile firmware. It does not mediate decoder DMA.
Linux's decoder driver remains responsible for power and buffers.

The helper occupies the pinned firmware's unused TCM range beginning at
`0x50000`; its mailbox is at `0x57000`. MPU slot 6 provides a private writable
256-byte page and slot 7 traps the decoder window. Both must be unused.
The hook at `0x2df70` preserves registers/flags, runs setup, replays the original
instruction, then returns to `0x2df74`. MemManage and HardFault vectors delegate
unsupported faults to the original handler. The supported Thumb word-access
forms and IT-state advancement are deliberately limited.

## Build without a tablet

Dependencies: Clang with ARM target support, GNU ARM bare-metal binutils,
Python 3, `pyelftools==0.33`, and `unicorn==2.1.4` for emulator checks.
On Debian/Ubuntu the toolchain packages are `clang binutils-arm-none-eabi`.
From the repository root, using your development Python environment:

```sh
bash experiments/video-decode/scp-proxy/build-helper.sh
python3 experiments/video-decode/scp-proxy/test-proxy.py build/scp-proxy
```

`BUILD_DIR`, `ARM_LD`, and `ARM_OBJCOPY` can override output/tool paths. Output
defaults to the repository's ignored `build/scp-proxy`, never the source tree.
These commands do not access a tablet. The Linux service is built separately
against a **copy** of the exact prepared arch14 kernel build:

```sh
make -C /build/video/linux-6.18.56 ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  M=/repo/experiments/video-decode/scp-proxy/host modules
```

That example produces `.ko`/intermediate files in the host source directory;
use a disposable source checkout or copy the host directory into your build
directory. Never commit those artifacts. The exact kernel `Module.symvers`
and configuration are required.

The patch builder accepts only the official SCP ELF with SHA256
`401e9c027b0e105efa9eb9b31683924a8d5f5ab3b1654f0915b174effe2df33d`
(`kukui_scp_v2.0.13324-280b9fce97`). Supply your own legitimately obtained copy;
this repository does not redistribute firmware or a patched image:

```sh
python3 experiments/video-decode/scp-proxy/patch-firmware.py \
  /path/to/original/scp.img --build-dir build/scp-proxy \
  --output build/scp-proxy/scp-experimental.img
```

It checks the input hash, original vectors/instruction, program-header count
and helper bounds. It refuses to overwrite the input. Compiler changes may
change helper bytes; the exception vector is taken from the helper ELF symbol.
The measured build's patched-image hash is in the evidence. No secure monitor,
bootloader, DEVAPC configuration or installed firmware file is patched.

## Device-test prerequisites and lifecycle

This is not an installer. Start with the [DT/module build procedure](../README.md)
in an isolated build tree. The 8 MiB SCP pool must already be reserved by the
boot device tree. Do not add it to live, allocated RAM. Preserve a device-bound,
verified known-good boot image and keep boot/update-guard hashes synchronized.
Do not substitute another tablet's boot image or bypass integrity checks.

The tested sequence on a fresh candidate boot was:

1. Put only the patched SCP image under a temporary directory matching
   `mediatek/mt8183/scp.img`; select that directory with `firmware_class.path`.
2. Load `mtk_scp_ipi`, `mtk_rpmsg`, then `trona_vdec_proxy`, then `mtk_scp`.
   The worker must be ready before any H.264 command reaches SCP.
3. Confirm SCP starts. For isolated diagnostics, disable automatic SCP crash
   recovery so a failed firmware does not repeatedly restart unnoticed.
4. Load the matching H.264/VP9 control, V4L2 mem2mem and MediaTek decoder
   modules listed in the parent experiment. Do not confuse the camera devices
   with the newly registered decoder; device numbers are not universal.
5. Clear the firmware override by writing a **newline**, not a zero-byte write,
   and run bounded synthetic tests. Inspect kernel and SCP errors as well as
   output; successful EOS alone proved misleading in the original experiment.

No autoload configuration, browser defaults or package ownership was changed.
The tested host worker sleeps after inactivity; it still holds an SCP clock
reference. Suspend/resume and concurrent-client recovery are not implemented
or qualified. Do not suspend an active test. Do not unload the worker while
the patched SCP may issue MMIO requests. For final cleanup, stop test clients,
restore the verified normal boot/guard, reboot, verify SCP is disabled, and
remove all temporary modules/firmware/media. Merely unloading the `.ko` is
not a complete rollback.

Early helper bugs caused SCP watchdogs, and repeated remoteproc restarts left
stale contexts. A clean reboot with the corrected helper was needed. The
successful results apply to that final cold-start build, not every prototype.

## Reproduce correctness and browser measurements

Use short synthetic Baseline and High-profile H.264 streams; the successful
samples were 2 seconds of 640×480/30 Baseline and 4 seconds of 1920×1080/30
High with B frames. Decode through `v4l2slh264dec`, convert to I420 and compare
every frame with FFmpeg software output. SHA256 equality of the complete
ordered raw streams is required; no raw-video dump needs to be retained.

For the isolated VA-API test, build the unmodified
[MT8183 bridge at the tested commit](https://github.com/shelterx/vaapi-v4l2-bridge-mt8183/tree/7e5d1dd49e37e9b99521d6ea63da59c6e1a2bfec)
in a temporary directory. The measured configuration used its default cached
capture/CPU conversion, not MDP3 or experimental GPU de-tiling. Select it only
for the test process with `LIBVA_DRIVER_NAME=v4l2stateless` and
`LIBVA_DRIVERS_PATH=/path/to/temporary/build`. Compare FFmpeg VA-API `hwdownload`
NV12 against software NV12 before browser measurements.

Chrome testing used an isolated temporary profile, local muted synthetic clip,
Panfrost and the process-local VA environment. The software control reported
`FFmpegVideoDecoder`; the hardware run reported `VaapiVideoDecoder`.
No sandbox was disabled. Verify the **actual** media decoder name and platform
flag; `video_decode: enabled` and capability lists alone are insufficient.
Count all Chrome processes **plus the Linux proxy** over the same interval,
and compare dropped-frame counter deltas after startup. Neither the user's
profile nor browser defaults need to be changed.

Before everyday use: implement lifecycle and fallback, then test long clips,
seeking, malformed streams, multiple clients, repeated cold boots and standby/
wake. Qualify updates and firmware hashes. Other codecs and protected streams
are not covered. Firmware and bridge redistribution also need licensing review;
this directory contains only this project's helper, patch tool and Linux module.
